// tmmintrin.h — arm64 stand-in for the x86 SSE header of the same name.
// Placed ahead of the system include path on arm64 builds only; sse2neon maps the
// SSE intrinsics the engine uses onto NEON.
#pragma once
#include <sse2neon.h>
// sse2neon spells MSVC's __int64 as int64_t, which breaks `unsigned __int64`.
#undef __int64
#define __int64 long long
