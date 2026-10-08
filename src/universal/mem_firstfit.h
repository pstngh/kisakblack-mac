#pragma once
#include "mem_userhunk.h"
#include <universal/ptr32.h>

// The allocator also treats nodes as pairs of 32-bit words, so `next` stays
// 4 bytes (Ptr32) on 64-bit builds.
struct _firstfit_heapnode // sizeof=0x8
{                                       // XREF: FIRSTFIT_HEAPNODE/r
    Ptr32<_firstfit_heapnode> next;
    int size;
};

HunkUser *__cdecl Hunk_FirstFitInit(
                unsigned int *buffer,
                unsigned int size,
                HU_ALLOCATION_SCHEME scheme,
                unsigned int flags,
                void *scheme_specific_data,
                const char *name,
                int type);
void __cdecl Hunk_FirstFitReset(HunkUser *_user);
void __cdecl Hunk_FirstFitDestroy(HunkUser *_user);
int __cdecl Hunk_FirstFitAlloc(HunkUser *_user, int size, int alignment);
void __cdecl Hunk_FirstFitFree(HunkUser *_user, unsigned int *ptr);
