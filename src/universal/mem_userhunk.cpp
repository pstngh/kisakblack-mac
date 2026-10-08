#include "mem_userhunk.h"
#include "assertive.h"
#include <qcommon/threads.h>
#include "mem_firstfit.h"
#include "com_memory.h"
#include <qcommon/mem_track.h>
#include "q_shared.h"
#include <qcommon/common.h>
#include "mem_firstfit.h"
#include "mem_fixed.h"

HunkUser *g_DebugHunkUser;

struct ALLOCATION_SCHEME_FUNCTIONS // sizeof=0x14
{
    HunkUser *(__cdecl *Init)(void *, int, HU_ALLOCATION_SCHEME, unsigned int, void *, const char *, int);
    void (__cdecl *Reset)(HunkUser *);
    void (__cdecl *Destroy)(HunkUser *);
    void *(__cdecl *Alloc)(HunkUser *, int, int, const char *);
    void (__cdecl *Free)(HunkUser *, void *);
};

// KISAKTODO: remove casts and fix prototypes
// These allocators return the block as an (encoded) int. The table wants a
// pointer, and on arm64 an int return does not fill the pointer register, so
// call them through typed adapters rather than through a cast function pointer.
static void *Hunk_UserDefaultAllocPtr(HunkUser *user, int size, int alignment, const char *name)
{
    return Ptr32_Decode(Hunk_UserDefaultAlloc((HunkUserDefault *)user, size, alignment, name));
}

static void *Hunk_UserDebugAllocPtr(HunkUser *user, int size, int alignment, const char *)
{
    return Ptr32_Decode(Hunk_UserDebugAlloc(user, size, alignment));
}

static void *Hunk_FirstFitAllocPtr(HunkUser *user, int size, int alignment, const char *)
{
    return Ptr32_Decode(Hunk_FirstFitAlloc(user, size, alignment));
}

ALLOCATION_SCHEME_FUNCTIONS g_HunkUserAllocationSchemeMap[4] =
{
  {
    (HunkUser *(*)(void*, int, HU_ALLOCATION_SCHEME, unsigned int, void*, const char *, int))Hunk_UserDefaultInit,
    (void(*)(HunkUser*))Hunk_UserDefaultReset,
     (void(*)(HunkUser *))Hunk_UserDefaultDestroy,
    Hunk_UserDefaultAllocPtr,
    Hunk_UserDefaultFree
  },
  {
    (HunkUser *(*)(void *, int, HU_ALLOCATION_SCHEME, unsigned int, void *, const char *, int))Hunk_UserDebugInit,
    Hunk_UserDebugReset,
    Hunk_UserDebugDestroy,
    Hunk_UserDebugAllocPtr,
    (void(__cdecl *)(HunkUser *, void *))Hunk_UserDebugFree
  },
  {
    (HunkUser *(*)(void *, int, HU_ALLOCATION_SCHEME, unsigned int, void *, const char *, int))Hunk_FirstFitInit,
    Hunk_FirstFitReset,
    Hunk_FirstFitDestroy,
    Hunk_FirstFitAllocPtr,
    (void(__cdecl *)(HunkUser *, void *))Hunk_FirstFitFree
  },
  {
    (HunkUser *(*)(void *, int, HU_ALLOCATION_SCHEME, unsigned int, void *, const char *, int))Hunk_FixedInit,
    Hunk_FixedReset,
    Hunk_FixedDestroy,
    (void *(*)(HunkUser *, int, int, const char *))Hunk_FixedAlloc,
    (void(__cdecl *)(HunkUser *, void *))Hunk_FixedFree
  }
};



// The debug scheme's header: the user, then the first-fit user that does the
// work (20 bytes on 32-bit builds, where this was written as raw words).
struct HunkUserDebug
{
    HunkUser user;
    HunkUser *firstFit;
};

HunkUser *__cdecl Hunk_UserDebugInit(
                unsigned int *buffer,
                int size,
                HU_ALLOCATION_SCHEME scheme,
                unsigned int flags,
                void *scheme_specific_data,
                const char *name,
                int type)
{
    if ( scheme != HU_SCHEME_DEBUG
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    109,
                    0,
                    "%s",
                    "scheme==HU_SCHEME_DEBUG") )
    {
        __debugbreak();
    }
    if ( !Sys_IsMainThread()
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    110,
                    0,
                    "%s",
                    "Sys_IsMainThread()") )
    {
        __debugbreak();
    }
    HunkUserDebug *debug = (HunkUserDebug *)buffer;
    memset(debug, 0, sizeof(*debug));
    debug->firstFit = Hunk_FirstFitInit((unsigned int *)((unsigned char *)buffer + sizeof(*debug)), size - sizeof(*debug), HU_SCHEME_FIRSTFIT, flags, 0, name, type);
    debug->user.scheme = scheme;
    debug->user.flags = flags;
    debug->user.name = name;
    debug->user.type = type;
    return &debug->user;
}

void __cdecl Hunk_UserDebugReset(HunkUser *_user)
{
    Hunk_FirstFitReset(((HunkUserDebug *)_user)->firstFit);
}

void __cdecl Hunk_UserDebugDestroy(HunkUser *_user)
{
    if ( _user->scheme != HU_SCHEME_DEBUG
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    136,
                    0,
                    "%s",
                    "user->hunkUser.scheme==HU_SCHEME_DEBUG") )
    {
        __debugbreak();
    }
    if ( !Sys_IsMainThread()
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    137,
                    0,
                    "%s",
                    "Sys_IsMainThread()") )
    {
        __debugbreak();
    }
    Z_VirtualFree(_user, _user->type);
}

int __cdecl Hunk_UserDebugAlloc(HunkUser *_user, int size, int alignment)
{
    if ( !_user
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp", 155, 0, "%s", "user") )
    {
        __debugbreak();
    }
    if ( _user->scheme != HU_SCHEME_DEBUG
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    156,
                    0,
                    "%s",
                    "user->hunkUser.scheme==HU_SCHEME_DEBUG") )
    {
        __debugbreak();
    }
    if ( !Sys_IsMainThread()
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    157,
                    0,
                    "%s",
                    "Sys_IsMainThread()") )
    {
        __debugbreak();
    }
    if ( (alignment & (alignment - 1)) != 0
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    158,
                    0,
                    "%s",
                    "!(alignment & (alignment - 1))") )
    {
        __debugbreak();
    }
    if ( alignment > 4096
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    159,
                    0,
                    "%s",
                    "alignment <= HUNK_MAX_ALIGNMENT") )
    {
        __debugbreak();
    }
    return Hunk_FirstFitAlloc(((HunkUserDebug *)_user)->firstFit, size, alignment);
}

void __cdecl Hunk_UserDebugFree(HunkUser *_user, unsigned int *ptr)
{
    if ( _user->scheme != HU_SCHEME_DEBUG
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    168,
                    0,
                    "%s",
                    "_user->scheme==HU_SCHEME_DEBUG") )
    {
        __debugbreak();
    }
    if ( !Sys_IsMainThread()
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    169,
                    0,
                    "%s",
                    "Sys_IsMainThread()") )
    {
        __debugbreak();
    }
    Hunk_FirstFitFree(((HunkUserDebug *)_user)->firstFit, ptr);
}

HunkUser *__cdecl Hunk_UserDefaultInit(
                unsigned __int8 *buffer,
                int size,
                HU_ALLOCATION_SCHEME scheme,
                unsigned int flags,
                void *scheme_specific_data,
                const char *name,
                int type)
{
    unsigned int v7; // ecx

    if ( scheme
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    179,
                    0,
                    "%s",
                    "scheme==HU_SCHEME_DEFAULT") )
    {
        __debugbreak();
    }
    // Written by member: the decompiled version stored words at 32-bit offsets.
    HunkUserDefault *user = (HunkUserDefault *)buffer;
    memset(user, 0, sizeof(*user));
    user->end = (int)Ptr32_Encode(&buffer[size]);
    user->pos = (int)Ptr32_Encode(user->buf);
    user->pos = (user->pos + 31) & 0xFFFFFFE0;
    if ( (user->pos & 0x1F) != 0
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    187,
                    0,
                    "%s\n\t(user->pos) = %i",
                    "(!(user->pos & 31))",
                    user->pos) )
    {
        __debugbreak();
    }
    if ( (flags & 2) != 0 )
        v7 = size + user->pos;
    else
        v7 = (user->pos + 4095) & 0xFFFFF000;
    user->locked = v7;
    user->maxSize = size;
    user->current = user;
    user->hunkUser.scheme = scheme;
    user->hunkUser.flags = flags;
    user->hunkUser.name = name;
    user->hunkUser.type = type;
    if ( user->next
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp", 200, 0, "%s", "!user->next") )
    {
        __debugbreak();
    }
    return &user->hunkUser;
}

void __cdecl Hunk_UserDefaultReset(HunkUserDefault *_user)
{
    char *pos; // [esp+0h] [ebp-8h]

    if (_user->hunkUser.scheme
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
            210,
            0,
            "%s",
            "user->hunkUser.scheme==HU_SCHEME_DEFAULT"))
    {
        __debugbreak();
    }
    if (_user->next)
    {
        //Hunk_UserDefaultDestroy(&_user->next->hunkUser);
        Hunk_UserDefaultDestroy(_user->next);
        _user->current = _user;
        _user->next = 0;
    }
    if ((_user->hunkUser.flags & 2) != 0)
    {
        _user->pos = (int)Ptr32_Encode(_user->buf);
        _user->locked = _user->maxSize + _user->pos;
    }
    else
    {
        // First page boundary past the header: &_user[93].buf[3] (_user + 4135) on 32-bit.
        pos = (char *)Ptr32_Decode(((unsigned int)Ptr32_Encode(_user) + 4135) & 0xFFFFF000);
        if (pos != (char *)Ptr32_Decode((_user->pos + 4095) & 0xFFFFF000))
        {
            if (_user->pos - (int)Ptr32_Encode(pos) <= 0
                && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    229,
                    0,
                    "%s",
                    "user->pos - pos > 0"))
            {
                __debugbreak();
            }
            Z_VirtualDecommit(pos, _user->pos - (_DWORD)Ptr32_Encode(pos), _user->hunkUser.type);
        }
        _user->pos = (int)Ptr32_Encode(_user->buf);
        _user->locked = (_user->pos + 4095) & 0xFFFFF000;
        memset(_user->buf, 0, 0xFD8u);
    }
}

void __cdecl Hunk_UserDefaultDestroy(HunkUserDefault *_user)
{
    HunkUserDefault *current; // [esp+4h] [ebp-8h]
    HunkUserDefault *newCurrent; // [esp+8h] [ebp-4h]

    if (_user->hunkUser.scheme
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
            251,
            0,
            "%s",
            "user->hunkUser.scheme==HU_SCHEME_DEFAULT"))
    {
        __debugbreak();
    }
    if ((_user->hunkUser.flags & 8) != 0)
        track_userhunk_freerange(_user->buf, _user->maxSize);
    if ((_user->hunkUser.flags & 2) == 0)
    {
        for (current = _user->next; current; current = newCurrent)
        {
            newCurrent = current->next;
            Z_VirtualFree(current, _user->hunkUser.type);
        }
        Z_VirtualFree(_user, _user->hunkUser.type);
    }
}

int __cdecl Hunk_UserDefaultAlloc(HunkUserDefault *user, unsigned int size, int alignment, const char *name)
{
    const char *v4; // eax
    signed int range; // [esp+4h] [ebp-1Ch]
    int pos; // [esp+Ch] [ebp-14h]
    int v8; // [esp+10h] [ebp-10h]
    HunkUserDefault *current; // [esp+18h] [ebp-8h]
    HunkUserDefault *newCurrent; // [esp+1Ch] [ebp-4h]
    int alignmenta; // [esp+30h] [ebp+10h]

    if (!user
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp", 288, 0, "%s", "user"))
    {
        __debugbreak();
    }
    if (user->hunkUser.scheme
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
            289,
            0,
            "%s",
            "user->hunkUser.scheme==HU_SCHEME_DEFAULT"))
    {
        __debugbreak();
    }
    if (size > user->maxSize - 40)
    {
        v4 = va("size: %d, maxSize: %d", size, user->maxSize - 40);
        if (!Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
            290,
            0,
            "%s\n\t%s",
            "static_cast< uint >( size ) <= user->maxSize - offsetof( HunkUserDefault, buf )",
            v4))
            __debugbreak();
    }
    if ((alignment & (alignment - 1)) != 0
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
            291,
            0,
            "%s",
            "!(alignment & (alignment - 1))"))
    {
        __debugbreak();
    }
    if (alignment > 4096
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
            292,
            0,
            "%s",
            "alignment <= HUNK_MAX_ALIGNMENT"))
    {
        __debugbreak();
    }
    alignmenta = alignment - 1;
    for (current = user->current; ; current = newCurrent)
    {
        pos = current->pos;
        v8 = pos % (alignmenta + 1) ? ~alignmenta & (alignmenta + pos) : current->pos;
        if ((signed int)(size + v8) <= current->end)
            break;
        if ((user->hunkUser.flags & 4) != 0 || (user->hunkUser.flags & 2) != 0)
            Com_Error(ERR_FATAL, "Hunk_UserAlloc: out of memory");
        newCurrent = (HunkUserDefault *)Hunk_UserCreate(
            user->maxSize,
            user->hunkUser.scheme,
            user->hunkUser.flags,
            0,
            user->hunkUser.name,
            user->hunkUser.type);
        user->current = newCurrent;
        current->next = newCurrent;
    }
    current->pos = size + v8;
    if (current->pos > current->locked)
    {
        range = ((current->pos + 4096) & 0xFFFFF000) - current->locked;
        if (range <= 0
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp", 323, 0, "%s", "range > 0"))
        {
            __debugbreak();
        }
        if (range % 4096
            && !Assert_MyHandler(
                "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                324,
                0,
                "%s",
                "range % PAGE_SIZE == 0"))
        {
            __debugbreak();
        }
        Z_VirtualCommit((char *)Ptr32_Decode(current->locked), range, user->hunkUser.type);
        current->locked += range;
    }
    if ((user->hunkUser.flags & 8) != 0)
    {
        if (name)
            track_userhunk_alloc(size, v8, name, user->hunkUser.type);
        else
            track_userhunk_alloc(size, v8, user->hunkUser.name, user->hunkUser.type);
    }
    return v8;
}

void __cdecl Hunk_UserDefaultFree(HunkUser *user, void *ptr)
{
    if ( user->scheme
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    341,
                    0,
                    "%s",
                    "user->scheme==HU_SCHEME_DEFAULT") )
    {
        __debugbreak();
    }
    if ( (user->flags & 8) != 0 )
        track_userhunk_free(ptr);
}

void __cdecl Hunk_UserStartup()
{
    iassert(!g_DebugHunkUser);
    g_DebugHunkUser = Hunk_UserCreate(0x1700000, HU_SCHEME_DEBUG, 0, 0, "Hunk_InitDebugMemory", 0);
}

void __cdecl Hunk_UserShutdown()
{
    if ( !g_DebugHunkUser
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    366,
                    0,
                    "%s",
                    "g_DebugHunkUser") )
    {
        __debugbreak();
    }
    Hunk_UserDestroy(g_DebugHunkUser);
    g_DebugHunkUser = 0;
}

HunkUser *__cdecl Hunk_UserCreateFromBuffer(
                void *buffer,
                int size,
                HU_ALLOCATION_SCHEME scheme,
                unsigned int flags,
                void *scheme_specific_data,
                const char *name,
                int type)
{
    if ( scheme == HU_SCHEME_DEBUG
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
                    374,
                    0,
                    "%s",
                    "scheme!=HU_SCHEME_DEBUG") )
    {
        __debugbreak();
    }
    return g_HunkUserAllocationSchemeMap[scheme].Init(buffer, size, scheme, flags | 2, scheme_specific_data, name, type);
}

HunkUser *__cdecl Hunk_UserCreate(
    int size,
    HU_ALLOCATION_SCHEME scheme,
    unsigned int flags,
    void *scheme_specific_data,
    const char *name,
    int type)
{
    char *buffer; // [esp+0h] [ebp-4h]

    if ((flags & 2) != 0
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
            381,
            0,
            "%s",
            "(flags & HF_FROMBUFFER)==0"))
    {
        __debugbreak();
    }
    if ((unsigned int)scheme >= HU_SCHEME_FIRSTFIT
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
            382,
            0,
            "%s",
            "scheme == HU_SCHEME_DEFAULT || scheme == HU_SCHEME_DEBUG"))
    {
        __debugbreak();
    }
    if (size % 4096
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\universal\\mem_userhunk.cpp",
            383,
            0,
            "%s\n\t(size) = %i",
            "(!(size % (4 * 1024)))",
            size))
    {
        __debugbreak();
    }
    buffer = (char *)Z_VirtualReserve(size);
    if (scheme == HU_SCHEME_DEBUG)
    {
        Z_VirtualCommit(buffer, size, type);
        return g_HunkUserAllocationSchemeMap[1].Init(buffer, size, HU_ALLOCATION_SCHEME::HU_SCHEME_DEBUG, flags | 2, scheme_specific_data, name, type);
    }
    else
    {
        Z_VirtualCommit(buffer, 44, type);
        return g_HunkUserAllocationSchemeMap[scheme].Init(buffer, size, scheme, flags, scheme_specific_data, name, type);
    }
}

void *__cdecl Hunk_UserAlloc(HunkUser *user, int size, int alignment, const char *name)
{
    return g_HunkUserAllocationSchemeMap[user->scheme].Alloc(user, size, alignment, name);
}

void __cdecl Hunk_UserFree(HunkUser *user, void *ptr)
{
    g_HunkUserAllocationSchemeMap[user->scheme].Free(user, ptr);
}

void __cdecl Hunk_UserReset(HunkUser *user)
{
    g_HunkUserAllocationSchemeMap[user->scheme].Reset(user);
}

void __cdecl Hunk_UserDestroy(HunkUser *user)
{
    g_HunkUserAllocationSchemeMap[user->scheme].Destroy(user);
}

void __cdecl Hunk_UserSetPos(HunkUser *_user, unsigned char *pos)
{
    iassert(_user->scheme == HU_SCHEME_DEFAULT);

    HunkUserDefault *user = (HunkUserDefault *)_user;
    iassert((user->hunkUser.flags & HF_FIXEDSIZE) != 0);
    iassert(pos >= user->buf);
    iassert((psize_int)pos <= user->pos);

    user->pos = (int)Ptr32_Encode(pos);
}

char *__cdecl Hunk_CopyString(HunkUser *user, const char *in)
{
    char v3; // [esp+3h] [ebp-21h]
    char *v4; // [esp+8h] [ebp-1Ch]
    const char *v5; // [esp+Ch] [ebp-18h]
    char *out; // [esp+20h] [ebp-4h]

    out = (char *)Hunk_UserAlloc(user, strlen(in) + 1, 1, 0);
    v5 = in;
    v4 = out;
    do
    {
        v3 = *v5;
        *v4++ = *v5++;
    }
    while ( v3 );
    return out;
}

