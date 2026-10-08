#pragma once
#include "demo_common.h"
#include "../../tl/jobqueue/jobqueue_all.h"
#include <qcommon/sv_msg_write_mp.h>

struct __declspec(align(4)) demoSaveCmd // sizeof=0xC
{                                       // XREF: ?Demo_Save@@YAXPAEH_N@Z/r
    unsigned __int8 *data;              // XREF: Demo_Save(uchar *,int,bool)+16/w
    int size;                           // XREF: Demo_Save(uchar *,int,bool)+1C/w
    bool writeInfoFile;                 // XREF: Demo_Save(uchar *,int,bool)+22/w
    // padding byte
    // padding byte
    // padding byte
};

void __cdecl Demo_StartSaveProcess();
void __cdecl Demo_Save(unsigned __int8 *data, int size, bool writeInfoFile);
void __cdecl Demo_SaveInternal(unsigned __int8 *data, int size, bool writeInfoFile);
int __cdecl Demo_SaveCallback(jqBatch *batch);
unsigned __int8 *__cdecl Demo_GetActiveMemoryBuffer();
unsigned __int8 *__cdecl Demo_SwapActiveMemoryBuffer();
void __cdecl Demo_SetIntermissionPoint(float *origin, float *angles);
void __cdecl Demo_GetIntermissionPointOrigin(float (*origin)[3]);
void __cdecl Demo_GetIntermissionPointAngles(float (*angles)[3]);
void __cdecl Demo_RecordPlayerSessionState(int clientNum, int state);
bool __cdecl Demo_IsPlayerSpectating(int clientNum);
bool __cdecl Demo_IsInFinalKillcam();
bool __cdecl Demo_IsPlayerDead(int clientNum);
void __cdecl Demo_ClientConnected(int clientNum);
int __cdecl Demo_GetDemoClientIndex();
void __cdecl Demo_SetDemoClientIndex(int index);
bool __cdecl Demo_ShouldWriteToDemoFile();
bool __cdecl Demo_ShouldBuildDemoSnapshot();
void __cdecl Demo_WriteHeader(msg_t *msg, bool isDemoClip);
void __cdecl Demo_BuildDemoSnapshot();
void __cdecl Demo_WriteSnapshotStartData(
                int localClientNum,
                msg_t *msg,
                int msgSequence,
                int cmdSequence,
                int serverTime,
                int physicsTime,
                unsigned __int8 lastFrame,
                unsigned __int8 snapFlags);
void __cdecl Demo_WriteGamestateToBuffer(int clientNum, int demoMsgType, msg_t *msg, int msgSequence);
bool __cdecl Demo_ShouldFilterServerCommand(int clientNum, const char *cmd);
void __cdecl Demo_WriteServerCommands(msg_t *msg);
void __cdecl Demo_WriteMatchState(msg_t *msg);
void __cdecl Demo_WritePlayerStates(msg_t *msg);
void __cdecl Demo_WritePacketEntities(msg_t *msg);
void __cdecl Demo_WritePacketClients(msg_t *msg);
void __cdecl Demo_PopulateStaticInfoData(demoMetaInfo *info);
unsigned int __cdecl Demo_WriteToBuffer(
                unsigned __int8 *to,
                unsigned __int8 *from,
                unsigned int size,
                int bytesWritten,
                int maxBufferSize);
unsigned int __cdecl Demo_WriteInfoDataToBuffer(demoMetaInfo *info, unsigned __int8 *buffer, int maxBufferSize);
void __cdecl Demo_WriteInfoData(int handle, demoMetaInfo *info);
void __cdecl Demo_InfoPlayerConnected(int clientNum);
void __cdecl Demo_InfoPlayerDisconnected(int clientNum);
void __cdecl Demo_AddBookmark(int type, int time, int clientNum1, int clientNum2);


extern SnapshotInfo_s g_snapInfo;
extern int g_democlientindex;
