// corecrt_malloc.h — portable stand-in for MSVC's CRT malloc header.
// Declare the allocators directly (NOT via <cstdlib>) to avoid pulling POSIX
// random(), which collides with the engine's own random().
#ifndef KISAK_CORECRT_MALLOC_H
#define KISAK_CORECRT_MALLOC_H
// The exception spec must match the libc's own prototypes (noexcept on glibc,
// none on musl/Emscripten and Apple; see KISAK_LIBC_NOEXCEPT). The MSVC
// _aligned_* helpers are not in any of them, so declare those ourselves.
extern "C" {
    void *malloc(size_t) KISAK_LIBC_NOEXCEPT;
    void *calloc(size_t, size_t) KISAK_LIBC_NOEXCEPT;
    void *realloc(void *, size_t) KISAK_LIBC_NOEXCEPT;
    void  free(void *) KISAK_LIBC_NOEXCEPT;
    void *_aligned_malloc(size_t, size_t) KISAK_LIBC_NOEXCEPT;
    void  _aligned_free(void *) KISAK_LIBC_NOEXCEPT;
}
#endif
