#pragma once
// ptr32.h — 32-bit pointer fields for structs loaded verbatim from fastfiles.
//
// The fastfile loader (db_load.cpp) reads asset structs straight out of the zone
// stream, so their in-memory layout must match the 32-bit on-disk layout. On a
// 32-bit build Ptr32<T> is just T*. On a 64-bit build it is a 4-byte handle:
//
//   0                          null
//   [1, PTR32_REGION_LIMIT)    offset from g_ptr32Base, i.e. a pointer into the
//                              reserved region that zone memory is allocated from
//   [PTR32_REGION_LIMIT, ...)  index into a handle table, for the few pointers that
//                              live outside the region (static data, malloc'd
//                              runtime objects such as GL textures)
//   [PTR32_SENTINEL_FIRST, ~0] the loader's raw markers ((T *)-1, (T *)-2, ...),
//                              which decode back to the same pointer values so the
//                              loader's `ptr == (T *)-1` checks keep working
//
// Ptr32<T> converts implicitly to and from T*, so most code that reads or writes
// these fields compiles unchanged. Code that takes the address of a field (T**),
// or passes one through varargs, has to say what it means.

#include <cstdint>
#include <cstddef>
#include <type_traits>

#if UINTPTR_MAX > 0xFFFFFFFFu

#define KISAK_PTR32 1

static constexpr uint32_t PTR32_REGION_LIMIT   = 0xC0000000u;
static constexpr uint32_t PTR32_SENTINEL_FIRST = 0xFFFFFF00u;
static constexpr size_t   PTR32_REGION_SIZE    = PTR32_REGION_LIMIT - 0x10000u;

extern uintptr_t g_ptr32Base;
extern void *const *g_ptr32Handles;

uint32_t Ptr32_EncodeSlow(const void *p);

inline void *Ptr32_Decode(uint32_t v)
{
    if (v < PTR32_REGION_LIMIT)
        return v ? (void *)(g_ptr32Base + v) : nullptr;
    if (v >= PTR32_SENTINEL_FIRST)
        return (void *)(intptr_t)(int32_t)v;
    return g_ptr32Handles[v - PTR32_REGION_LIMIT];
}

inline uint32_t Ptr32_Encode(const void *p)
{
    uintptr_t d = (uintptr_t)p - g_ptr32Base;
    if (d - 1 < (uintptr_t)PTR32_REGION_LIMIT - 1)
        return (uint32_t)d;
    return Ptr32_EncodeSlow(p);
}

// The reserved region. Zone memory (fastfile blocks, the physical-memory pool,
// hunk) is allocated here so pointers into it encode as plain offsets.
void Ptr32_InitRegion();
bool Ptr32_InRegion(const void *p);
void *Ptr32_RegionAlloc(size_t size);
void Ptr32_RegionFree(void *p);
size_t Ptr32_RegionAllocSize(const void *p);

// C-style casts a raw T* would allow and that stay correct for a 32-bit field:
// dropping const/volatile, and byte or scalar views of the pointee. Casting to a
// pointer-to-pointer or to an unrelated struct is left an error on purpose: if
// the pointee holds Ptr32 fields, reading it through native pointer types is a
// 64-bit bug.
template <class T, class U>
inline constexpr bool kPtr32CastOk =
    std::is_same_v<std::remove_cv_t<T>, std::remove_cv_t<U>> ||
    std::is_void_v<U> || std::is_arithmetic_v<U>;

template <class T>
struct Ptr32
{
    uint32_t v;

    Ptr32() = default;
    Ptr32(T *p) : v(Ptr32_Encode((const void *)p)) {}

    Ptr32 &operator=(T *p) { v = Ptr32_Encode((const void *)p); return *this; }

    T *get() const { return (T *)Ptr32_Decode(v); }
    operator T *() const { return get(); }
    T *operator->() const { return get(); }

    template <class U, std::enable_if_t<!std::is_same_v<U, T> && kPtr32CastOk<T, U>, int> = 0>
    explicit operator U *() const { return (U *)get(); }
};

static_assert(sizeof(Ptr32<char>) == 4, "Ptr32 must keep the 32-bit field width");

#else

template <class T>
using Ptr32 = T *;

#endif
