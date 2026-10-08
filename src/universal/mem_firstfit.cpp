#include "mem_firstfit.h"
#include <universal/ptr32.h>
#include "assertive.h"
#include <win32/win_common.h>

// FIRSTFIT_HUNKUSER: 32 bytes on 32-bit builds, where the decompiled code
// addressed these fields as _user[1].scheme/flags/name/type.
struct FirstFitHunk
{
    HunkUser user;
    unsigned int size;        // _user[1].scheme
    unsigned int freeBlocks;  // hunk->freeBlocks: encoded first free node
    int reserved;             // _user[1].name
    int used;                 // hunk->used
};
static_assert(sizeof(void *) != 4 || sizeof(FirstFitHunk) == 32);

HunkUser *__cdecl Hunk_FirstFitInit(
                unsigned int *buffer,
                unsigned int size,
                HU_ALLOCATION_SCHEME scheme,
                unsigned int flags,
                void *scheme_specific_data,
                const char *name,
                int type)
{
    if ( (flags & 2) == 0
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_firstfit.cpp",
                    49,
                    0,
                    "%s",
                    "(flags & HF_FROMBUFFER)!=0") )
    {
        __debugbreak();
    }
    if ( size <= sizeof(FirstFitHunk) + sizeof(_firstfit_heapnode)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_firstfit.cpp",
                    50,
                    0,
                    "%s",
                    "size>sizeof(FIRSTFIT_HUNKUSER)+sizeof(FIRSTFIT_HEAPNODE)") )
    {
        __debugbreak();
    }
    FirstFitHunk *hunk = (FirstFitHunk *)buffer;
    _firstfit_heapnode *first = (_firstfit_heapnode *)(hunk + 1);
    hunk->user.name = name;
    hunk->user.scheme = scheme;
    hunk->user.flags = flags;
    hunk->user.type = type;
    hunk->reserved = -1;
    hunk->size = size;
    hunk->freeBlocks = Ptr32_Encode(first);
    first->next = 0;
    first->size = size - sizeof(FirstFitHunk);
    hunk->used = sizeof(FirstFitHunk);
    return &hunk->user;
}

void __cdecl Hunk_FirstFitReset(HunkUser *_user)
{
    FirstFitHunk *hunk = (FirstFitHunk *)_user;
    _firstfit_heapnode *first = (_firstfit_heapnode *)(hunk + 1);
    hunk->freeBlocks = Ptr32_Encode(first);
    first->next = 0;
    first->size = hunk->size - sizeof(FirstFitHunk);
    hunk->used = sizeof(FirstFitHunk);
}

void __cdecl Hunk_FirstFitDestroy(HunkUser *_user)
{
    _user->scheme = HU_SCHEME_DEFAULT;
    _user->flags = 0;
    _user->name = 0;
    _user->type = 0;
    FirstFitHunk *hunk = (FirstFitHunk *)_user;
    hunk->size = 0;
    hunk->freeBlocks = 0;
    hunk->reserved = 0;
    hunk->used = 0;
}

int __cdecl Hunk_FirstFitAlloc(HunkUser *_user, int size, int alignment)
{
    FirstFitHunk *hunk = (FirstFitHunk *)_user;
    unsigned int *free_link; // [esp+10h] [ebp-20h]
    int adj_size; // [esp+18h] [ebp-18h]
    _firstfit_heapnode *last; // [esp+2Ch] [ebp-4h]

    if ( alignment <= 0
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_firstfit.cpp", 90, 0, "%s", "alignment>0") )
    {
        __debugbreak();
    }
    Sys_EnterCriticalSection(CRITSECT_MEMFIRSTFIT);
    free_link = (unsigned int *)Ptr32_Decode(hunk->freeBlocks);
    last = 0;
    while ( 1 )
    {
        if ( !free_link )
            goto LABEL_16;
        adj_size = (~(alignment - 1) & ((unsigned int)Ptr32_Encode(free_link) + alignment + 11)) - (unsigned int)Ptr32_Encode(free_link) + size;
        if ( (int)free_link[1] >= adj_size )
            break;
        last = (_firstfit_heapnode *)free_link;
        free_link = (unsigned int *)Ptr32_Decode(*free_link);
    }
    if ( !*free_link && (int)(free_link[1] - adj_size) <= 1024 )
    {
LABEL_16:
        Sys_LeaveCriticalSection(CRITSECT_MEMFIRSTFIT);
        return 0;
    }
    if ( (int)(free_link[1] - adj_size) > 1024 )
    {
        *(unsigned int *)Ptr32_Decode((~(alignment - 1) & ((unsigned int)Ptr32_Encode(free_link) + alignment + 11)) + size) = *free_link;
        *(unsigned int *)((char *)free_link + adj_size + 4) = free_link[1] - adj_size;
        *free_link = (~(alignment - 1) & ((unsigned int)Ptr32_Encode(free_link) + alignment + 11)) + size;
        free_link[1] = adj_size;
    }
    if ( last )
        last->next = (_firstfit_heapnode *)Ptr32_Decode(*free_link);
    else
        hunk->freeBlocks = *free_link;
    *free_link = -559038737;
    *(unsigned int *)Ptr32_Decode((~(alignment - 1) & ((unsigned int)Ptr32_Encode(free_link) + alignment + 11)) - 12 + 8) = (unsigned int)Ptr32_Encode(free_link);
    hunk->used += free_link[1];
    Sys_LeaveCriticalSection(CRITSECT_MEMFIRSTFIT);
    return ~(alignment - 1) & ((unsigned int)Ptr32_Encode(free_link) + alignment + 11);
}

void __cdecl Hunk_FirstFitFree(HunkUser *_user, unsigned int *ptr)
{
    FirstFitHunk *hunk = (FirstFitHunk *)_user;
    _firstfit_heapnode *free_link; // [esp+0h] [ebp-14h]
    _firstfit_heapnode *scan; // [esp+8h] [ebp-Ch]
    _firstfit_heapnode *last; // [esp+10h] [ebp-4h]

    if ( ptr )
    {
        Sys_EnterCriticalSection(CRITSECT_MEMFIRSTFIT);
        last = 0;
        scan = (_firstfit_heapnode *)Ptr32_Decode(hunk->freeBlocks);
        free_link = (_firstfit_heapnode *)Ptr32_Decode(*(ptr - 1));
        if ( Ptr32_Raw(free_link->next) == 0xDEADBEEF )   // the allocated-block marker
        {
            hunk->used -= free_link->size;
            if ( !hunk->freeBlocks
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_firstfit.cpp",
                            178,
                            0,
                            "%s",
                            "hunk->free_blocks!=NULL") )
            {
                __debugbreak();
            }
            while ( scan )
            {
                if ( scan > free_link )
                {
                    if ( last )
                    {
                        free_link->next = scan;
                        last->next = free_link;
                    }
                    else
                    {
                        free_link->next = (_firstfit_heapnode *)Ptr32_Decode(hunk->freeBlocks);
                        hunk->freeBlocks = (unsigned int)Ptr32_Encode(free_link);
                    }
                    if ( last && (_firstfit_heapnode *)((char *)last + last->size) == free_link )
                    {
                        last->size += free_link->size;
                        last->next = free_link->next;
                        free_link = last;
                    }
                    if ( free_link->next && (_firstfit_heapnode *)((char *)free_link + free_link->size) == free_link->next )
                    {
                        free_link->size += free_link->next->size;
                        free_link->next = free_link->next->next;
                    }
                    goto LABEL_24;
                }
                last = scan;
                scan = scan->next;
            }
            if ( !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_firstfit.cpp",
                            220,
                            0,
                            "%s\n\t%s",
                            "0",
                            "invalid pointer passed into first fit free\n") )
                __debugbreak();
        }
        else if ( !Assert_MyHandler(
                                 "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_firstfit.cpp",
                                 173,
                                 0,
                                 "%s\n\t%s",
                                 "0",
                                 "buffer overrun or underrun or illegal ptr detected") )
        {
            __debugbreak();
        }
LABEL_24:
        Sys_LeaveCriticalSection(CRITSECT_MEMFIRSTFIT);
    }
}

