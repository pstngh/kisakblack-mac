#include "ptr32.h"

#ifdef KISAK_PTR32

#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <unordered_map>

void *const *g_ptr32Handles;

namespace
{
constexpr size_t REGION_GRANULE = 0x4000;   // Apple Silicon page size

// The heap: zero-fill (it costs neither file size nor memory until touched),
// inside the image so it sits in the linear window with the globals. macOS
// loads an image this large below the dyld shared cache; ~2 GB does not fit.
constexpr size_t REGION_SIZE = 1536ull << 20;
alignas(REGION_GRANULE) unsigned char g_region[REGION_SIZE];

constexpr size_t HANDLE_CAPACITY = 64u << 20;   // 512 MB of reserved address space

std::mutex &RegionMutex() { static std::mutex m; return m; }
std::mutex &HandleMutex() { static std::mutex m; return m; }

// Free ranges keyed by start address, and live allocations' sizes.
std::map<uintptr_t, size_t> &FreeRanges()
{
    static std::map<uintptr_t, size_t> m{{(uintptr_t)g_region, REGION_SIZE}};
    return m;
}
std::map<uintptr_t, size_t> &LiveRanges() { static std::map<uintptr_t, size_t> m; return m; }

void **g_handleTable;
uint32_t g_handleCount;

[[noreturn]] void Ptr32_Fatal(const char *msg)
{
    fprintf(stderr, "ptr32: %s\n", msg);
    abort();
}

struct WindowCheck
{
    WindowCheck()
    {
        if ((uintptr_t)g_region + REGION_SIZE - Ptr32_Base() >= PTR32_LINEAR_LIMIT)
            Ptr32_Fatal("the heap region does not fit in the 2 GB linear window");
    }
} g_windowCheck;
}

bool Ptr32_InRegion(const void *p)
{
    return (uintptr_t)p >= (uintptr_t)g_region && (uintptr_t)p < (uintptr_t)g_region + REGION_SIZE;
}

void *Ptr32_RegionAlloc(size_t size)
{
    size = (size + REGION_GRANULE - 1) & ~(REGION_GRANULE - 1);
    if (!size)
        size = REGION_GRANULE;
    std::lock_guard<std::mutex> lock(RegionMutex());
    auto &freeRanges = FreeRanges();
    for (auto it = freeRanges.begin(); it != freeRanges.end(); ++it)
    {
        if (it->second < size)
            continue;
        uintptr_t start = it->first;
        size_t rest = it->second - size;
        freeRanges.erase(it);
        if (rest)
            freeRanges[start + size] = rest;
        LiveRanges()[start] = size;
        return (void *)start;
    }
    fprintf(stderr, "ptr32: heap region exhausted (%zu bytes requested)\n", size);
    return nullptr;
}

size_t Ptr32_RegionAllocSize(const void *p)
{
    std::lock_guard<std::mutex> lock(RegionMutex());
    auto &live = LiveRanges();
    auto it = live.find((uintptr_t)p);
    return it == live.end() ? 0 : it->second;
}

void Ptr32_RegionFree(void *p)
{
    if (!p)
        return;
    std::lock_guard<std::mutex> lock(RegionMutex());
    auto &live = LiveRanges();
    auto it = live.find((uintptr_t)p);
    if (it == live.end())
        Ptr32_Fatal("free of a pointer the region did not allocate");
    uintptr_t start = it->first;
    size_t size = it->second;
    live.erase(it);

    // Give the pages back and leave them demand-zero, like a fresh allocation.
    if (mmap((void *)start, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED)
        Ptr32_Fatal("could not reset freed region pages");

    auto &freeRanges = FreeRanges();
    auto next = freeRanges.lower_bound(start);
    if (next != freeRanges.end() && start + size == next->first)
    {
        size += next->second;
        next = freeRanges.erase(next);
    }
    if (next != freeRanges.begin())
    {
        auto prev = std::prev(next);
        if (prev->first + prev->second == start)
        {
            prev->second += size;
            return;
        }
    }
    freeRanges[start] = size;
}

// ---- General-purpose heap over the region -----------------------------------
// Small blocks come from per-size-class free lists carved out of 256 KB slabs;
// large ones take whole region pages. A 16-byte header in front of each block
// records which.
namespace
{
constexpr size_t HEAP_HEADER = 16;
constexpr uint32_t HEAP_MAGIC = 0x4B425033;   // "KBP3"
constexpr uint32_t HEAP_LARGE = 0xFFFFFFFFu;
constexpr size_t HEAP_SLAB = 256u << 10;
constexpr size_t kClassSizes[] = {
    16, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096,
    6144, 8192, 12288, 16384, 24576, 32768, 49152, 65536,
};
constexpr uint32_t HEAP_CLASSES = sizeof(kClassSizes) / sizeof(kClassSizes[0]);

struct HeapHeader
{
    uint32_t magic;
    uint32_t sizeClass;   // index into kClassSizes, or HEAP_LARGE
    uint64_t largeSize;   // whole region allocation size for HEAP_LARGE
};
static_assert(sizeof(HeapHeader) == HEAP_HEADER);

struct HeapClass
{
    void *freeList;
    unsigned char *bump;
    unsigned char *bumpEnd;
};
HeapClass g_heapClasses[HEAP_CLASSES];
std::mutex &HeapMutex() { static std::mutex m; return m; }
}

void *Ptr32_HeapAlloc(size_t size)
{
    size_t total = size + HEAP_HEADER;
    uint32_t c = 0;
    while (c < HEAP_CLASSES && kClassSizes[c] < total)
        ++c;
    if (c == HEAP_CLASSES)
    {
        unsigned char *block = (unsigned char *)Ptr32_RegionAlloc(total);
        if (!block)
            return nullptr;
        HeapHeader *h = (HeapHeader *)block;
        h->magic = HEAP_MAGIC;
        h->sizeClass = HEAP_LARGE;
        h->largeSize = total;
        return block + HEAP_HEADER;
    }

    std::lock_guard<std::mutex> lock(HeapMutex());
    HeapClass &hc = g_heapClasses[c];
    unsigned char *block = (unsigned char *)hc.freeList;
    if (block)
    {
        hc.freeList = *(void **)(block + HEAP_HEADER);
    }
    else
    {
        if (hc.bump + kClassSizes[c] > hc.bumpEnd)
        {
            hc.bump = (unsigned char *)Ptr32_RegionAlloc(HEAP_SLAB);
            if (!hc.bump)
                return nullptr;
            hc.bumpEnd = hc.bump + HEAP_SLAB;
        }
        block = hc.bump;
        hc.bump += kClassSizes[c];
    }
    HeapHeader *h = (HeapHeader *)block;
    h->magic = HEAP_MAGIC;
    h->sizeClass = c;
    h->largeSize = 0;
    return block + HEAP_HEADER;
}

void Ptr32_HeapFree(void *p)
{
    if (!p)
        return;
    unsigned char *block = (unsigned char *)p - HEAP_HEADER;
    HeapHeader *h = (HeapHeader *)block;
    if (h->magic != HEAP_MAGIC)
        Ptr32_Fatal("heap free of a block it did not allocate (or a corrupted header)");
    if (h->sizeClass == HEAP_LARGE)
    {
        h->magic = 0;
        Ptr32_RegionFree(block);
        return;
    }
    if (h->sizeClass >= HEAP_CLASSES)
        Ptr32_Fatal("heap header has a bad size class");
    std::lock_guard<std::mutex> lock(HeapMutex());
    HeapClass &hc = g_heapClasses[h->sizeClass];
    h->magic = 0;
    *(void **)p = hc.freeList;
    hc.freeList = block;
}

uint32_t Ptr32_EncodeSlow(const void *p)
{
    if (!p)
        return 0;
    intptr_t s = (intptr_t)p;
    if (s < 0 && s >= -(intptr_t)(0x100000000ull - PTR32_SENTINEL_FIRST))
        return (uint32_t)s;

    std::lock_guard<std::mutex> lock(HandleMutex());
    static std::unordered_map<const void *, uint32_t> s_handles;
    auto it = s_handles.find(p);
    if (it != s_handles.end())
        return it->second;
    if (!g_handleTable)
    {
        void *t = mmap(nullptr, HANDLE_CAPACITY * sizeof(void *), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (t == MAP_FAILED)
            Ptr32_Fatal("could not reserve the handle table");
        g_handleTable = (void **)t;
        __atomic_store_n(&g_ptr32Handles, (void *const *)g_handleTable, __ATOMIC_RELEASE);
    }
    if (g_handleCount >= HANDLE_CAPACITY || g_handleCount >= (PTR32_SENTINEL_FIRST - PTR32_LINEAR_LIMIT) >> 4)
        Ptr32_Fatal("handle table full");
    uint32_t v = PTR32_LINEAR_LIMIT + (g_handleCount << 4) + ((uintptr_t)p & 15);
    __atomic_store_n(&g_handleTable[g_handleCount], (void *)p, __ATOMIC_RELEASE);
    ++g_handleCount;
    s_handles.emplace(p, v);
    return v;
}

#endif
