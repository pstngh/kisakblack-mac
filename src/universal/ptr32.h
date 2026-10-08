#pragma once
// ptr32.h — 32-bit pointers on 64-bit builds.
//
// The engine is decompiled 32-bit code. Asset structs are read verbatim from
// fastfiles, the network and script code keep pointers in 32-bit integers, and
// script bytecode embeds 4-byte code positions. On a 32-bit build everything
// here is the identity. On a 64-bit build the engine gets a 32-bit address
// space of its own:
//
//   - The base is the executable's Mach-O header. The image (code, globals)
//     and a large zero-fill heap array inside it (Ptr32_RegionAlloc) lie within
//     2 GB above it, so a pointer into any of them encodes as its offset from the
//     base. Offsets are linear: Encode(p) + n == Encode(p + n).
//   - Other pointers (system malloc, dylibs) get a slot in a handle table. They
//     round-trip exactly but are not linear.
//   - Either way an encoding keeps the pointer's low 4 bits, so alignment checks
//     on an encoded value (`(int)p & 15`) still hold.
//   - The fastfile loader's markers ((T *)-1, (T *)-2, ...) encode as themselves.
//
//   0                              null
//   [1, PTR32_LINEAR_LIMIT)        offset from Ptr32_Base()
//   [PTR32_LINEAR_LIMIT, SENTINEL) handle-table index
//   [PTR32_SENTINEL_FIRST, ~0]     loader markers
//
// Ptr32<T> is a 4-byte field holding such a value. It converts implicitly to and
// from T*, so most code that reads or writes these fields compiles unchanged.
// Code that takes the address of a field (T**), or passes one through varargs,
// has to say what it means. Ptr32_Encode/Ptr32_Decode convert explicitly where
// the decompiled code stores a pointer in an int.

#include <cstdint>
#include <cstddef>
#include <type_traits>

#if UINTPTR_MAX > 0xFFFFFFFFu

#define KISAK_PTR32 1

static constexpr uint32_t PTR32_LINEAR_LIMIT   = 0x80000000u;
static constexpr uint32_t PTR32_SENTINEL_FIRST = 0xFFFFFF00u;

// The lowest address of the executable image (a linker-defined symbol, so it
// is usable before static initialization and costs no memory load).
#if defined(__APPLE__)
extern "C" const char kisak_image_base __asm("__mh_execute_header");
#else
extern "C" const char kisak_image_base __asm("__executable_start");
#endif
inline uintptr_t Ptr32_Base() { return (uintptr_t)&kisak_image_base; }

extern void *const *g_ptr32Handles;

uint32_t Ptr32_EncodeSlow(const void *p);

inline void *Ptr32_Decode(uint32_t v)
{
    if (v < PTR32_LINEAR_LIMIT)
        return v ? (void *)(Ptr32_Base() + v) : nullptr;
    if (v >= PTR32_SENTINEL_FIRST)
        return (void *)(intptr_t)(int32_t)v;
    return g_ptr32Handles[(v - PTR32_LINEAR_LIMIT) >> 4];
}

inline uint32_t Ptr32_Encode(const void *p)
{
    uintptr_t d = (uintptr_t)p - Ptr32_Base();
    if (d - 1 < (uintptr_t)PTR32_LINEAR_LIMIT - 1)
        return (uint32_t)d;
    return Ptr32_EncodeSlow(p);
}

// The heap inside the linear window. Engine allocators (the physical-memory
// pool, hunk, zone blocks, Z_Malloc) take their memory from here. Allocations
// are page-granular (16 KB) and zero-filled.
void *Ptr32_RegionAlloc(size_t size);
void Ptr32_RegionFree(void *p);
size_t Ptr32_RegionAllocSize(const void *p);
bool Ptr32_InRegion(const void *p);

// General-purpose allocation from the same region (malloc semantics, 16-byte
// aligned, not zeroed), for the engine's Z_Malloc family.
void *Ptr32_HeapAlloc(size_t size);
void Ptr32_HeapFree(void *p);

// C-style casts a raw T* would allow: to another object or scalar type (the
// decompiled code puns between union members and byte views). Casting to a
// pointer-to-pointer is left an error on purpose: the pointee here is a 32-bit
// slot (or an array of them), and reading it as native pointers is a 64-bit bug.
template <class T, class U>
inline constexpr bool kPtr32CastOk = !std::is_pointer_v<U>;

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

// The stored 32-bit value (an offset, handle or loader marker), for code that
// inspects a field's raw contents.
template <class T>
inline uint32_t Ptr32_Raw(const Ptr32<T> &p) { return p.v; }

// Store a 32-bit value that is not a pointer (the decompiled structs keep
// script string indices in pointer-typed fields).
template <class T>
inline void Ptr32_SetRaw(Ptr32<T> &p, uint32_t v) { p.v = v; }

#else

template <class T>
using Ptr32 = T *;

inline void *Ptr32_Decode(uint32_t v) { return (void *)(uintptr_t)v; }
inline uint32_t Ptr32_Encode(const void *p) { return (uint32_t)(uintptr_t)p; }

template <class T>
inline uint32_t Ptr32_Raw(T *p) { return (uint32_t)(uintptr_t)p; }

template <class T>
inline void Ptr32_SetRaw(T *&p, uint32_t v) { p = (T *)(uintptr_t)v; }

#endif

// Function pointers (script builtins, callbacks) are stored in ints too.
template <class R, class... A>
inline uint32_t Ptr32_Encode(R (*f)(A...)) { return Ptr32_Encode((const void *)f); }
