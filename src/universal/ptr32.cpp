#include "ptr32.h"

#ifdef KISAK_PTR32

#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <unordered_map>

uintptr_t g_ptr32Base;
void *const *g_ptr32Handles;

namespace
{
// Region layout: g_ptr32Base + 0x10000 is the first usable byte, so offset 0
// (null) never names region memory.
constexpr uintptr_t REGION_SKIP = 0x10000;
constexpr size_t REGION_GRANULE = 0x4000;   // Apple Silicon page size

constexpr size_t HANDLE_CAPACITY = 64u << 20;   // 64M handles = 512 MB reserved address space

std::mutex &RegionMutex() { static std::mutex m; return m; }
std::mutex &HandleMutex() { static std::mutex m; return m; }

// Free ranges keyed by start address, and live allocations' sizes.
std::map<uintptr_t, size_t> &FreeRanges() { static std::map<uintptr_t, size_t> m; return m; }
std::map<uintptr_t, size_t> &LiveRanges() { static std::map<uintptr_t, size_t> m; return m; }

uintptr_t g_regionStart;
uintptr_t g_regionEnd;

void **g_handleTable;
uint32_t g_handleCount;

[[noreturn]] void Ptr32_Fatal(const char *msg)
{
    fprintf(stderr, "ptr32: %s\n", msg);
    abort();
}

void InitRegionLocked()
{
    if (g_regionStart)
        return;
    void *p = mmap(nullptr, PTR32_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED)
        Ptr32_Fatal("could not reserve the zone memory region");
    g_regionStart = (uintptr_t)p;
    g_regionEnd = g_regionStart + PTR32_REGION_SIZE;
    FreeRanges()[g_regionStart] = PTR32_REGION_SIZE;
    // Publish the base last: until now every pointer took the handle path.
    __atomic_store_n(&g_ptr32Base, g_regionStart - REGION_SKIP, __ATOMIC_RELEASE);
}
}

void Ptr32_InitRegion()
{
    std::lock_guard<std::mutex> lock(RegionMutex());
    InitRegionLocked();
}

bool Ptr32_InRegion(const void *p)
{
    return (uintptr_t)p >= g_regionStart && (uintptr_t)p < g_regionEnd;
}

void *Ptr32_RegionAlloc(size_t size)
{
    size = (size + REGION_GRANULE - 1) & ~(REGION_GRANULE - 1);
    if (!size)
        size = REGION_GRANULE;
    std::lock_guard<std::mutex> lock(RegionMutex());
    InitRegionLocked();
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
    mmap((void *)start, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);

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

uint32_t Ptr32_EncodeSlow(const void *p)
{
    if (!p)
        return 0;
    intptr_t s = (intptr_t)p;
    if (s < 0 && s >= -(intptr_t)(0x100000000ull - PTR32_SENTINEL_FIRST))
        return (uint32_t)s;

    uintptr_t base = __atomic_load_n(&g_ptr32Base, __ATOMIC_ACQUIRE);
    uintptr_t d = (uintptr_t)p - base;
    if (base && d - 1 < (uintptr_t)PTR32_REGION_LIMIT - 1)
        return (uint32_t)d;

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
    if (g_handleCount >= HANDLE_CAPACITY || g_handleCount >= PTR32_SENTINEL_FIRST - PTR32_REGION_LIMIT)
        Ptr32_Fatal("handle table full");
    uint32_t v = PTR32_REGION_LIMIT + g_handleCount;
    __atomic_store_n(&g_handleTable[g_handleCount], (void *)p, __ATOMIC_RELEASE);
    ++g_handleCount;
    s_handles.emplace(p, v);
    return v;
}

#endif
