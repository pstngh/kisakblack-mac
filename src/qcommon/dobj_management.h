#pragma once
#include <xanim/dobj.h>

DObj *__cdecl Com_GetServerDObj(unsigned int handle);
DObj *__cdecl Com_ClientDObjCreate(
                DObjModel_s *dobjModels,
                unsigned __int16 numModels,
                XAnimTree_s *tree,
                unsigned int handle,
                int localClientNum);
int __cdecl Com_GetFreeDObjIndex();
void __cdecl Com_ClientDObjClearAllSkel();
DObj *__cdecl Com_ServerDObjCreate(
                DObjModel_s *dobjModels,
                unsigned __int16 numModels,
                XAnimTree_s *tree,
                unsigned int handle);
void __cdecl Com_SafeClientDObjFree(unsigned int handle, int localClientNum);
void __cdecl Com_SafeServerDObjFree(unsigned int handle);
void __cdecl Com_InitDObj();
void __cdecl Com_ShutdownDObj();
void __cdecl DB_SaveDObjs();
void __cdecl DB_LoadDObjs();
DObj *__cdecl Com_GetClientDObj(unsigned int handle, int localClientNum);

// One DObj per slot (31 ints on 32-bit builds; DObj is larger on 64-bit).
#define DOBJ_SLOT_WORDS (sizeof(DObj) / sizeof(unsigned int))
extern unsigned int objBuf[3072][DOBJ_SLOT_WORDS];