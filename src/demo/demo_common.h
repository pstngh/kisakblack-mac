#pragma once
#include <qcommon/msg_mp.h>
#include <ddl/ddl_api.h>
#include <client_mp/client_mp.h>
#include <bgame/bg_local.h>
#include <cstring>
#include <cgame_mp/cg_scoreboard_mp.h>


struct demoConnectedPlayersInfo // sizeof=0x18
{                                                                             // XREF: demoMetaInfo/r
        unsigned __int8 playerIndex;                // XREF: Demo_InfoPlayerConnected(int)+21/w
                                                                                // Demo_InfoPlayerDisconnected(int)+42/r
        unsigned __int8 clientNum;                    // XREF: Demo_SetDefaultClient(int,int)+CC/r
                                                                                // Demo_InfoPlayerConnected(int)+30/w ...
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        unsigned __int64 xuid;                            // XREF: Demo_TagPlayers+55/r
                                                                                // Demo_TagPlayers+5C/r ...
        int connectTime;                                        // XREF: Demo_SetDefaultClient(int,int)+115/r
                                                                                // Demo_InfoPlayerConnected(int)+41/w ...
        int disconnectTime;                                 // XREF: Demo_InfoPlayerDisconnected(int)+AF/w
                                                                                // Demo_GetConnectionTimeInfoForPlayer(int,int *,int *)+6F/r ...
};

struct __declspec(align(4)) demoDefaultBookmarkInfo // sizeof=0xC
{                                                                             // XREF: demoMetaInfo/r
        unsigned __int8 type;                             // XREF: Demo_AddBookmark(int,int,int,int)+23/w
                                                                                // Demo_AddBookmark(int,int,int,int)+5C/r ...
        // padding byte
        // padding byte
        // padding byte
        int time;                                                     // XREF: Demo_AddBookmark(int,int,int,int)+32/w
                                                                                // Demo_GetNextDefaultBookmarkForPlayer(int,int,int *,int *,float (*)[4])+7B/r ...
        unsigned __int8 clientNum1;                 // XREF: Demo_AddBookmark(int,int,int,int)+41/w
                                                                                // Demo_AddBookmark(int,int,int,int)+6E/r ...
        unsigned __int8 clientNum2;                 // XREF: Demo_AddBookmark(int,int,int,int)+50/w
                                                                                // Demo_AddBookmark(int,int,int,int)+80/r ...
        // padding byte
        // padding byte
};

struct demoHeliPatchesInfo // sizeof=0x18
{                                                                             // XREF: demoMetaInfo/r
        int brushmodel;
        float origin[3];
        unsigned __int16 targetname;
        // padding byte
        // padding byte
        int enabled;
};

struct demoMetaInfo // sizeof=0x3918
{                                                                             // XREF: demoPlayback/r
                                                                                // demoMain/r
        bool type;                                                    // XREF: Demo_WriteGamestateToBuffer(int,int,msg_t *,int)+F8/w
        // padding byte
        // padding byte
        // padding byte
        int startTime;                                            // XREF: Demo_StreamingSuccessCallback(int,unsigned __int64)+233/r
                                                                                // Demo_ReadInfoData(char *,int)+183/o ...
        int endTime;                                                // XREF: Demo_StreamingSuccessCallback(int,unsigned __int64)+22D/r
                                                                                // Demo_End(bool)+2F/w ...
        char gameType[256];                                 // XREF: Demo_Screenshot_f(void)+B6/o
                                                                                // Demo_ReadInfoData(char *,int)+252/o ...
        char mapName[256];                                    // XREF: Demo_Screenshot_f(void)+D5/o
                                                                                // Demo_ReadInfoData(char *,int)+277/o ...
        char author[32];                                        // XREF: Demo_ReadInfoData(char *,int)+22D/o
                                                                                // Demo_GetAuthor(void)+3/o
        char name[22];                                            // XREF: Demo_StreamingSuccessCallback(int,unsigned __int64)+264/o
                                                                                // Demo_Screenshot_f(void)+63/o ...
        char description[22];                             // XREF: Demo_StreamingSuccessCallback(int,unsigned __int64)+278/o
                                                                                // Demo_Screenshot_f(void)+8C/o ...
        bool isModifiedName;
        bool isModifiedDescription;
        // padding byte
        // padding byte
        int week;                                                     // XREF: Demo_ReadInfoData(char *,int)+2DD/o
        int month;                                                    // XREF: Demo_ReadInfoData(char *,int)+2FF/o
                                                                                // Demo_GetTimeInfo(void)+4/r
        int year;                                                     // XREF: Demo_ReadInfoData(char *,int)+321/o
                                                                                // Demo_GetTimeInfo(void)+2F/r
        int day;                                                        // XREF: Demo_ReadInfoData(char *,int)+343/o
                                                                                // Demo_GetTimeInfo(void)+35/r
        int hour;                                                     // XREF: Demo_ReadInfoData(char *,int)+365/o
                                                                                // Demo_GetTimeInfo(void)+28/r
        int min;                                                        // XREF: Demo_ReadInfoData(char *,int)+387/o
                                                                                // Demo_GetTimeInfo(void)+21/r
        unsigned int createTime;                        // XREF: Demo_StreamingSuccessCallback(int,unsigned __int64)+247/r
        int numConnectedPlayersInfoCount;     // XREF: Demo_ReadInfoData(char *,int)+1C7/o
                                                                                // Demo_ReadInfoData(char *,int)+3C1/r ...
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        demoConnectedPlayersInfo connectedPlayers[50];
                                                                                // XREF: Demo_TagPlayers+55/r
                                                                                // Demo_TagPlayers+5C/r ...
        int numDefaultBookmarksCount;             // XREF: Demo_ReadInfoData(char *,int)+1E9/o
                                                                                // Demo_ReadInfoData(char *,int)+424/r ...
        demoDefaultBookmarkInfo defaultBookmarks[1000];
                                                                                // XREF: Demo_ReadInfoData(char *,int)+43E/o
                                                                                // Demo_AddBookmark(int,int,int,int)+23/w ...
        int numHeliPatchesCount;                        // XREF: Demo_RegisterHeliPatches(void)+4/r
                                                                                // Demo_RegisterHeliPatches(void)+23/r ...
        demoHeliPatchesInfo heliPatches[32];
                                                                                // XREF: Demo_RegisterHeliPatches(void)+31/o
                                                                                // Demo_ReadInfoData(char *,int)+4A2/o
};

struct demoTagPlayers // sizeof=0xF8
{                                                                             // XREF: screenshotMetaInfo/r
                                                                                // clipSegment/r
        int count;
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        unsigned __int64 playerXuids[30];
};

struct __declspec(align(8)) screenshotMetaInfo // sizeof=0x348
{                                                                             // XREF: demoPlayback/r
        demoTagPlayers screenshotPlayers;
        char gameType[256];
        char mapName[256];
        char name[22];
        char description[22];
        bool isModifiedName;
        bool isModifiedDescription;
        // padding byte
        // padding byte
        int createTime;
        int week;
        int month;
        int year;
        int day;
        int hour;
        int min;
        // padding byte
        // padding byte
        // padding byte
        // padding byte
};

struct demoGameSettings // sizeof=0x6
{                                                                             // XREF: demoHeader/r
        bool onlinegame;                                        // XREF: Demo_StartRecord_f(void)+DF/w
                                                                                // Demo_IsPublicOnlineMatch(void):loc_9BDB33/r ...
        bool xblive_privatematch;                     // XREF: Demo_StartRecord_f(void)+F1/w
                                                                                // Demo_IsPublicOnlineMatch(void)+1E/r ...
        bool xblive_wagermatch;                         // XREF: Demo_StartRecord_f(void)+103/w
                                                                                // Demo_IsWagerMatch(void):loc_9BDBA3/r ...
        bool systemlink;                                        // XREF: Demo_StartRecord_f(void)+115/w
                                                                                // Demo_ReadHeader(void)+B0/w ...
        bool combatTraining;                                // XREF: Demo_StartRecord_f(void)+127/w
                                                                                // Demo_IsCombatTrainingMatch(void):loc_9BDB73/r
        bool customGameMode;                                // XREF: Demo_StartRecord_f(void)+131/w
                                                                                // Demo_IsCustomGameMode(void):loc_9BDBD3/r ...
};

struct __declspec(align(2)) demoHeader // sizeof=0x10
{                                                                             // XREF: demoMain/r
        int version;                                                // XREF: Demo_StartRecord_f(void)+BA/w
                                                                                // Demo_ReadHeader(void)+10/w ...
        int maxClients;                                         // XREF: Demo_StreamingSuccessCallback(int,unsigned __int64)+56/r
                                                                                // Demo_StartRecord_f(void)+CD/w ...
        bool isDemoClip;                                        // XREF: Demo_IsClipPlaying(void):loc_9BD6C3/r
                                                                                // Demo_ReadHeader(void)+50/w
        demoGameSettings settings;                    // XREF: Demo_StartRecord_f(void)+DF/w
                                                                                // Demo_StartRecord_f(void)+F1/w ...
        // padding byte
};

struct demoKeyboardData // sizeof=0x14
{                                                                             // XREF: demoMain/r
        int mode;                                                     // XREF: Demo_Keyboard_f(void)+6B/w
                                                                                // Demo_Keyboard_f(void):loc_9BAE82/r ...
        const char *title;                                    // XREF: Demo_Keyboard_f(void)+118/w
                                                                                // Demo_Keyboard_f(void)+154/w ...
        const char *defaultText;                        // XREF: Demo_Keyboard_f(void)+11D/w
                                                                                // Demo_Keyboard_f(void)+159/w ...
        int textSize;                                             // XREF: Demo_Keyboard_f(void)+127/w
                                                                                // Demo_Keyboard_f(void)+163/w ...
        int currentProcessingSegmentIndex;    // XREF: Demo_Keyboard_f(void)+F3/w
                                                                                // Demo_Keyboard_f(void)+106/w ...
};

struct ArchivedMatchState // sizeof=0xC
{                                                                             // XREF: MatchState/r
        int matchUIVisibilityFlags;                 // XREF: GScr_SetMatchFlag+287/r
                                                                                // GScr_SetMatchFlag+28D/w ...
        int bombTimer[2];                                     // XREF: GScr_SetBombTimer+65/w
                                                                                // GScr_SetBombTimer+9A/w
};

struct UnarchivedMatchState // sizeof=0x30
{                                                                             // XREF: MatchState/r
        int alliesScore;                                        // XREF: G_PopulateMatchState+12/w
        int axisScore;                                            // XREF: G_PopulateMatchState+1D/w
        int scoreLimit;                                         // XREF: G_PopulateMatchState+43/w
        int matchUIVisibilityFlags;                 // XREF: GScr_SetMatchFlag+2C4/r
                                                                                // GScr_SetMatchFlag+2CA/w ...
        scoreboardColumnType_t scoreboardColumnTypes[4];
                                                                                // XREF: GScr_SetScoreboardColumns+6B/w
                                                                                // GScr_SetScoreboardColumns+75/r
        float mapCenter[3];                                 // XREF: G_PopulateMatchState+50/w
                                                                                // G_PopulateMatchState+60/w ...
        unsigned int talkFlags;                         // XREF: GScr_SetMatchTalkFlag+FC/r
                                                                                // GScr_SetMatchTalkFlag+104/w ...
};

struct __declspec(align(64)) MatchState // sizeof=0x80
{                                                                             // XREF: snapshot_s/r
                                                                                // clientActive_t/r ...
        int index;                                                    // XREF: G_PopulateMatchState+3/w
        ArchivedMatchState archivedState;     // XREF: GScr_SetBombTimer+65/w
                                                                                // GScr_SetBombTimer+9A/w ...
        UnarchivedMatchState unarchivedState;
                                                                                // XREF: G_PopulateMatchState+12/w
                                                                                // G_PopulateMatchState+1D/w ...
        unsigned int pad[1];
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
};

struct demoMemBlock // sizeof=0xC008
{                                                                             // XREF: demoMain/r
        int compressedSize;                                 // XREF: Demo_Load(void)+5C/w
                                                                                // Demo_Load(void)+B4/r ...
        int uncompressedSize;                             // XREF: Demo_Load(void)+93/w
                                                                                // Demo_Load(void)+122/o ...
        unsigned __int8 compressedMsgBuf[49152]; // XREF: Demo_Load(void)+12/o
                                                                                // Demo_Load(void)+D4/o ...
};

struct buttonHoldCmd // sizeof=0xC
{                                                                             // XREF: demoPlayback/r
        bool active;
        // padding byte
        // padding byte
        // padding byte
        int key;
        int activatedTime;
};

struct demoKeyFrame // sizeof=0x30
{                                                                             // XREF: demoPlayback/r
                                                                                // demoPlayback/r
        int keyframeBufferStart;
        int demoFileMemoryBlockStart;
        int keyframeSnapshotTime;
        int keyframeClipServerTime;
        int firstCmdSequence;
        int size;
        msg_bookmark_t msgBookmark;
};

struct clipSegment // sizeof=0x120
{                                                                             // XREF: demoPlayback/r
        char name[22];
        // padding byte
        // padding byte
        int clipBufferIndex;
        int size;
        int transition;
        int duration;
        demoTagPlayers taggedPlayers;
};

struct __declspec(align(4)) demoPlayback // sizeof=0x12DEE0
{
        bool overridePause;
        // padding byte
        // padding byte
        // padding byte
        int transition;
        int transitionScreenTime;
        bool jumpTimeFlag;
        bool switchedPlayers;
        // padding byte
        // padding byte
        int metaInformationScreenTime;
        buttonHoldCmd holdCmd;
        int buttonPressTime[10];
        int screenshotSize;
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        screenshotMetaInfo screenshotInfo;
        bool displayPartyScoreboard;
        // padding byte
        // padding byte
        // padding byte
        msg_t keyframeMsg;
        int keyframeIndex;
        int keyframeBufferIndex;
        int lastProcessedCmd;
        bool keyframeGenerated;
        // padding byte
        // padding byte
        // padding byte
        int lastJumpedKeyframe;
        int keyframeJumpCount;
        demoKeyFrame keyFrame[250];
        int demoCmdInProgress;
        bool completed;
        bool hideGameHud;
        bool hideDemoHud;
        // padding byte
        int cameraMode;
        float clipCameraOrigin[3];
        float clipCameraAngles[3];
        float prevClipCameraOrigin[3];
        float prevClipCameraAngles[3];
        bool clipPausedState;
        // padding byte
        // padding byte
        // padding byte
        msg_t clipRecordingMsg;
        int prevClipRecordBufIndex;
        int clipRecordBufIndex;
        unsigned __int8 mainClipRecordingBuf[1048576];
        unsigned __int8 subClipRecordingBuf[49152];
        playerState_s clipRecordPS;
        unsigned __int8 clipRecordClient;
        // padding byte
        // padding byte
        // padding byte
        float clipRecordTimescale;
        unsigned __int8 clipCameraMode;
        bool clipHudHidden;
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        demoMetaInfo clipRecordInfo;
        bool forceWriteClipCommands;
        // padding byte
        // padding byte
        // padding byte
        int clipState;
        int clipTime;
        int clipServerTime;
        int prevClipTime;
        int originalFilmTime;
        bool clipModified;
        bool pendingSegmentSave;
        bool previewAllSegments;
        bool cancelPreview;
        bool forceTeleportClipCamera;
        // padding byte
        // padding byte
        // padding byte
        clipSegment segments[10];
        int segmentCount;
        int segmentGameStateSize;
        int defaultSegmentTag;
        int clipPreviewBufferIndex;
        int clipPreviewSegmentNumber;
        unsigned __int8 demoPreviewBuf[2][49152];
        demoKeyFrame demoPreview[2];
        bool demoCompleteStateForRestorationPoint[2];
        // padding byte
        // padding byte
};

struct demoProfile // sizeof=0x40
{                                                                             // XREF: demoMain/r
        int memUsed[8];                                         // XREF: Demo_RecordProfileData(int,int)+6/r
                                                                                // Demo_RecordProfileData(int,int)+13/w ...
        int count[8];                                             // XREF: Demo_RecordProfileData(int,int)+1D/r
                                                                                // Demo_RecordProfileData(int,int)+2A/w ...
};


struct __declspec(align(128)) demoMain // sizeof=0xB0B00
{                                                                             // XREF: .data:demoMain demo/r
        char demoName[128];
        int demoFileHandle;                                 // XREF: Demo_Forward_f(void)+329/r
                                                                                // Demo_Back_f(void)+2F2/r ...
        int state;                                                    // XREF: Demo_SetDemoState(int)+6/w
                                                                                // Demo_GetDemoState(void)+3/r
        msg_t msg;                                                    // XREF: Demo_StartRecord_f(void)+1D0/o
                                                                                // Demo_Forward_f(void)+351/o ...
        unsigned __int8 msgBuf0[49152];         // XREF: Demo_Init(void)+78/o
                                                                                // Demo_Load(void)+24/o ...
        unsigned __int8 msgBuf1[49152];         // XREF: Demo_Forward_f(void)+39C/o
                                                                                // Demo_Forward_f(void)+3DB/o ...
        bool msgBufFlag;                                        // XREF: Demo_StartSaveProcess(void)+56/r
                                                                                // Demo_GetActiveMemoryBuffer(void)+4/r ...
        // padding byte
        // padding byte
        // padding byte
        int totalSize;                                            // XREF: Demo_Forward_f(void)+30E/w
                                                                                // Demo_Back_f(void)+2D7/w ...
        int sessionFlags;
        demoHeader header;                                    // XREF: Demo_StreamingSuccessCallback(int,unsigned __int64)+56/r
                                                                                // Demo_StartRecord_f(void)+BA/w ...
        demoKeyboardData keyboard;                    // XREF: Demo_Keyboard_f(void)+6B/w
                                                                                // Demo_Keyboard_f(void):loc_9BAE82/r ...
        bool shutdownInProgress;                        // XREF: Demo_End(bool)+C/w
                                                                                // Demo_End(bool):loc_9BB9B2/w ...
        // padding byte
        // padding byte
        // padding byte
        int lastProcessedTime;                            // XREF: Demo_Init(void)+B9/w
                                                                                // Demo_ShouldGenerateKeyFrameSnapshot(int,int):loc_9C3C83/r ...
        int lastProcessedMsgNum;                        // XREF: Demo_Init(void)+C3/w
                                                                                // Demo_ReadDemoMessage(int)+68B/w ...
        int lastReliableCommandRecorded[32];
                                                                                // XREF: Demo_StartRecord_f(void)+24A/w
                                                                                // Demo_ClientConnected(int)+6/w ...
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        MatchState matchState;                            // XREF: Demo_ResetSnapshotData(void)+1E/o
                                                                                // Demo_ParseSnapshot(int,msg_t *)+1E4/o ...
        playerState_s ps[32];                             // XREF: Demo_SwitchPlayer_f(void)+181/o
                                                                                // Demo_SwitchPlayer(int,bool,int,bool)+151/r ...
        entityState_s ent[1024];                        // XREF: Demo_Init(void)+91/o
                                                                                // Demo_HasClientSpawnedIn(int)+C/r ...
        int entClientMask[1024][2];                 // XREF: Demo_ShouldEntityBeVisibleToClient(int)+28/r
                                                                                // Demo_ResetSnapshotData(void)+46/o ...
        int prevNumEntities;                                // XREF: Demo_ReadPacketEntities(int,msg_t *,int,int)+8F/w
                                                                                // Demo_GenerateUncompressedSnapshot(int,msg_t *,int,int,int,int,int,bool)+366/r ...
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        clientState_s client[32];                     // XREF: Demo_Init(void)+A5/o
                                                                                // Demo_SwitchPlayer(int,bool,int,bool)+B9/r ...
        int prevNumClients;                                 // XREF: Demo_ReadPacketClients(int,msg_t *,int,int)+88/w
                                                                                // Demo_GenerateUncompressedSnapshot(int,msg_t *,int,int,int,int,int,bool)+550/r ...
        int prevNumParsedEntities;                    // XREF: Demo_ResetSnapshotData(void)+67/w
                                                                                // Demo_ReadPacketEntities(int,msg_t *,int,int)+5D/r ...
        int prevNumParsedClients;                     // XREF: Demo_ResetSnapshotData(void)+71/w
                                                                                // Demo_ReadPacketClients(int,msg_t *,int,int)+56/r ...
        int playerSessionState[32];                 // XREF: Demo_RecordPlayerSessionState(int,int)+9/w
                                                                                // Demo_IsPlayerSpectating(int)+7/r ...
        float intermissionPointOrigin[3];     // XREF: Demo_SetIntermissionPoint(float * const,float * const)+A/w
                                                                                // Demo_SetIntermissionPoint(float * const,float * const)+1A/w ...
        float intermissionPointAngles[3];     // XREF: Demo_SetIntermissionPoint(float * const,float * const)+39/w
                                                                                // Demo_SetIntermissionPoint(float * const,float * const)+49/w ...
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        demoMetaInfo info;                                    // XREF: Demo_StreamingSuccessCallback(int,unsigned __int64)+1C1/o
                                                                                // Demo_StreamingSuccessCallback(int,unsigned __int64)+22D/r ...
        bool renderingMovie;                                // XREF: Demo_Play_f(void)+1E2/w
                                                                                // Demo_End(bool)+BD/w ...
        // padding byte
        // padding byte
        // padding byte
        demoMemBlock memBlock;                            // XREF: Demo_Load(void)+12/o
                                                                                // Demo_Load(void)+5C/w ...
        demoPlayback *playback;
        bool playbackInited;                                // XREF: Demo_End(bool)+F9/w
                                                                                // Demo_IsPlaybackInited(void)+3/r ...
        // padding byte
        // padding byte
        // padding byte
        demoProfile profile;                                // XREF: Demo_RecordProfileData(int,int)+6/r
                                                                                // Demo_RecordProfileData(int,int)+13/w ...
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
        // padding byte
};

struct demoClient_s // sizeof=0x4
{                                       // XREF: .data:demoClient_s demoClient/r
    int state;                          // XREF: Demo_SetDemoClientState(int)+6/w
};

void __cdecl Demo_RegisterDvars();
void __cdecl Demo_RegisterCommands();
void __cdecl Demo_AddDemoClient_f();
void __cdecl Demo_RemoveDemoClient_f();
void __cdecl Demo_StartRecord_f();
void __cdecl Demo_StopRecord_f();
void __cdecl Demo_Play_f();
void __cdecl Demo_Stop_f();
void __cdecl Demo_Forward_f();
void __cdecl Demo_Back_f();
void __cdecl Demo_JumpToStart_f();
void __cdecl Demo_Pause_f();
void __cdecl Demo_TimeScale_f();
void __cdecl Demo_ToggleGameHud_f();
void __cdecl Demo_ToggleDemoHud_f();
void __cdecl Demo_SwitchPlayer_f();
void __cdecl Demo_SwitchCamera_f();
void __cdecl Demo_StartClipRecord_f();
void __cdecl Demo_PauseClipRecord_f();
void __cdecl Demo_PreviewClip_f();
void __cdecl Demo_DeleteClip_f();
void __cdecl Demo_SaveSegment_f();
void __cdecl Demo_MoveSegment_f();
void __cdecl Demo_DeleteSegment_f();
void __cdecl Demo_PreviewSegment_f();
void __cdecl Demo_SwitchDefaultSegmentTag_f();
void __cdecl Demo_SwitchTransition_f();
void __cdecl Demo_Keyboard_f();
void    Demo_Screenshot_f();
void __cdecl Demo_SaveScreenshot_f();
void __cdecl Demo_RenderMovie_f();
void __cdecl Demo_SwitchControls_f();
bool __cdecl Demo_IsEnabled();
void __cdecl Demo_SetDemoState(int state);
int __cdecl Demo_GetDemoState();
void __cdecl Demo_SetDemoClientState(int state);
int __cdecl Demo_GetDemoClientState();
bool __cdecl Demo_IsIdle();
bool __cdecl Demo_IsRecording();
bool __cdecl Demo_IsPlaying();
demoMain *__cdecl Demo_GetDemoName();
void __cdecl Demo_Printf(int channel, const char *string);
void __cdecl Demo_Init();
bool __cdecl Demo_InitWrite();
bool __cdecl Demo_InitRead();
void __cdecl Demo_Frame(int msec);
void __cdecl Demo_End(bool abnormalTermination);


extern const dvar_t *demo_enabled;
extern const dvar_t *demo_recordBasicTraining;
extern const dvar_t *demo_recordPrivateMatch;
extern const dvar_t *demo_filesizeLimit;
extern const dvar_t *demo_debug;
extern const dvar_t *demo_drawdebuginformation;
extern const dvar_t *demo_errortitle;
extern const dvar_t *demo_errormessage;
extern const dvar_t *demo_client;
extern const dvar_t *demo_recordingrate;
extern const dvar_t *demo_keyframerate;
extern const dvar_t *demo_pause;
extern const dvar_t *demo_usefilesystem;
extern const dvar_t *demo_save_smp;
extern const dvar_t *demo_timescale;
extern const dvar_t *demo_timeScaleRate;
extern const dvar_t *demo_cmdNum;
extern const dvar_t *demo_bookmarkEventThresholdTime;
extern const dvar_t *demo_controllerConfig;
extern const dvar_t *demo_defaultSegmentTag;
extern const dvar_t *demo_selectedSegmentIndex;
extern const dvar_t *demo_desiredTime;
extern const dvar_t *demo_desiredClient;
extern const dvar_t *demo_packetsPerSecondMin;
extern const dvar_t *demo_bytesPerSecondMin;
extern const dvar_t *demo_packetsPerSecondMax;
extern const dvar_t *demo_bytesPerSecondMax;

extern demoMain demo;

extern unsigned __int8 *g_keyframeBuf;

extern entityState_s g_defaultEntityState;
extern clientState_s g_defaultClientState;