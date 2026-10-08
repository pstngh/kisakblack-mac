// malloc.h — macOS has no <malloc.h>; the engine only wants the allocators and
// alloca() from it. On the include path for macOS builds only.
#pragma once
#include <stddef.h>
#include <alloca.h>
#include <corecrt_malloc.h>
