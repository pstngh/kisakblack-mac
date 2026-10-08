#include "live_sessions_win.h"
#include <win32/win_tasks.h>
#include <DW/dwMatchMaking.h>
#include <client_mp/cl_main_pc_mp.h>
#include <gfx_d3d/r_rendercmds.h>
#include "live_storage_win.h"
#include "live_win.h"
#include <DW/dwUtils_pc.h>
#include "live_stats.h"
#include <game_mp/g_main_mp.h>
#include <server_mp/sv_init_mp.h>
#include "live_leaderboard.h"
#include <cgame/cg_compass.h>

SessionData_s g_serverSession;

overlappedTask overlappedTasks_2[32];
const dvar_t *matchmaking_debug;
SessionJoinData sessionJoinData[32];
SessionCreateData sessionCreateData[32];

SessionGraveYard sessionGraveYard[6];

void __cdecl Session_ClearDWOverlappedTasks()
{
    TaskManager_ClearOverlappedTasks(overlappedTasks_2);
}

void __cdecl Live_FindSessionsStart(bool reset, int servertype)
{
}

void __cdecl Live_FindSessionsPump()
{
    CL_QuickMatch_Frame();
}

void __cdecl Session_QoSListenStart(SessionData_s *session)
{
}

void __cdecl Session_QoSListenStop(SessionData_s *session)
{
}

int __cdecl Session_FindRegisteredUser(const SessionData_s *session, unsigned __int64 xuid)
{
    int slot; // [esp+8h] [ebp-4h]

    for ( slot = 0; slot < 32; ++slot )
    {
        if ( session->registeredUsers[slot].active && session->registeredUsers[slot].xuid == xuid )
            return slot;
    }
    return -1;
}

void __cdecl Session_EndGameSession(SessionData_s *session)
{
    if ( session->sessionStartCalled )
    {
        if ( session->sessionHandle )
        {
            R_BeginRemoteScreenUpdate();
            Session_EndOngoingSessionTasks(session);
            LiveStorage_UploadStats();
            R_EndRemoteScreenUpdate(0);
        }
        session->sessionStartCalled = 0;
    }
}

void __cdecl Session_DeleteSession(SessionData_s *session)
{
}

void __cdecl Session_DeleteHandle(bool *sessionHandle)
{
}

void __cdecl Session_UnregisterAllUsersFromVoice(SessionData_s *session)
{
    int slot; // [esp+0h] [ebp-4h]

    if ( session->registerUsersWithVoice )
    {
        for ( slot = 0; slot < 32; ++slot )
            ;
    }
}

int __cdecl Session_GetFreeSessionGraveYardSlot()
{
    int slot; // [esp+0h] [ebp-4h]

    for ( slot = 0; slot < 6; ++slot )
    {
        if ( !sessionGraveYard[slot].active )
            return slot;
    }
    if ( !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    364,
                    0,
                    "All session graveyard slots are full") )
        __debugbreak();
    return 0;
}

void __cdecl Session_StartHost(SessionData_s *session, int sessionFlags, int numPrivateSlots, int numPublicSlots)
{
}

int __cdecl Session_GetFreeCreateSessionSlot()
{
    int slot; // [esp+0h] [ebp-4h]

    for ( slot = 0; slot < 32; ++slot )
    {
        if ( !sessionCreateData[slot].active )
            return slot;
    }
    if ( !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    742,
                    0,
                    "All create session slots are full") )
        __debugbreak();
    return 0;
}

void __cdecl Session_Modify(
                int localControllerIndex,
                SessionData_s *session,
                int flags,
                int publicSlots,
                int privateSlots)
{
}

taskCompleteResults __cdecl Session_ModifyComplete(int slot)
{
    return TASK_COMPLETE;
}

void __cdecl Session_EveryoneLeaveSessionAsync(int localControllerIndex, SessionData_s *session)
{
    int v2; // ecx
    int registeredUserSlot; // [esp+Ch] [ebp-114h]
    unsigned __int64 players[32]; // [esp+10h] [ebp-110h]
    int numPlayers; // [esp+114h] [ebp-Ch]
    unsigned __int64 player; // [esp+118h] [ebp-8h]

    if ( session->sessionHandle )
    {
        Session_EndOngoingSessionTasks(session);
        numPlayers = 0;
        for ( registeredUserSlot = 0; registeredUserSlot < 32; ++registeredUserSlot )
        {
            if ( registeredUserSlot < 0
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                            1198,
                            0,
                            "%s\n\t(registeredUserSlot) = %i",
                            "(registeredUserSlot >= 0 && registeredUserSlot < 32)",
                            registeredUserSlot) )
            {
                __debugbreak();
            }
            if ( session->registeredUsers[registeredUserSlot].active )
            {
                if ( session->remoteTalkers[registeredUserSlot].xuid )
                {
                    if ( Session_JoinInProgress() )
                        Live_FinishOngoingSessionJoinTasksForXUID(session->remoteTalkers[registeredUserSlot].xuid);
                }
                player = session->registeredUsers[registeredUserSlot].xuid;
                v2 = numPlayers;
                LODWORD(players[numPlayers]) = player;
                HIDWORD(players[v2]) = HIDWORD(player);
                ++numPlayers;
                Com_Printf(
                    16,
                    "Calling XSessionLeaveRemote for slot %i in session '%s'\n",
                    registeredUserSlot,
                    session->sessionName);
                session->registeredUsers[registeredUserSlot].active = 0;
            }
        }
    }
}

char __cdecl Session_JoinInProgress()
{
    return TaskManager_TaskIsInProgress(overlappedTasks_2, 1);
}

bool __cdecl Session_SessionTasksInProgress(SessionData_s *session)
{
    bool result; // al
    const char *v2; // eax
    int tasknum; // [esp+Ch] [ebp-4h]

    for ( tasknum = 0; ; ++tasknum )
    {
        if ( tasknum >= 32 )
            return 0;
        if ( overlappedTasks_2[tasknum].active )
            break;
LABEL_2:
        ;
    }
    switch ( overlappedTasks_2[tasknum].type )
    {
        case 1:
            if ( *((SessionData_s **)TaskManager_GetTaskData(&overlappedTasks_2[tasknum]) + 6) != session )
                goto LABEL_2;
            result = 1;
            break;
        case 2:
            if ( *((SessionData_s **)TaskManager_GetTaskData(&overlappedTasks_2[tasknum]) + 1) != session )
                goto LABEL_2;
            result = 1;
            break;
        case 4:
            if ( session != &g_serverSession )
                goto LABEL_2;
            result = 1;
            break;
        case 5:
            if ( session != &g_serverSession )
                goto LABEL_2;
            result = 1;
            break;
        case 6:
            if ( session != TaskManager_GetTaskData(&overlappedTasks_2[tasknum]) )
                goto LABEL_2;
            result = 1;
            break;
        case 7:
            if ( session != TaskManager_GetTaskData(&overlappedTasks_2[tasknum]) )
                goto LABEL_2;
            result = 1;
            break;
        default:
            v2 = va("Unknown session task type %i\n", overlappedTasks_2[tasknum].type);
            if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp", 1782, 0, v2) )
                __debugbreak();
            goto LABEL_2;
    }
    return result;
}

int __cdecl Session_StartSessionComplete(int slot)
{
    if ( slot >= 32
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    1102,
                    0,
                    "%s",
                    "slot < static_cast<int>( sizeof(overlappedTasks) / sizeof(overlappedTasks[0]) )") )
    {
        __debugbreak();
    }
    if ( !overlappedTasks_2[slot].active
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    1106,
                    0,
                    "%s",
                    "overlappedTasks[slot].active") )
    {
        __debugbreak();
    }
    if ( overlappedTasks_2[slot].type != 5
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    1107,
                    0,
                    "%s",
                    "overlappedTasks[slot].type == TASK_STARTINGGAMESESSION") )
    {
        __debugbreak();
    }
    TaskManager_ClearTask(&overlappedTasks_2[slot]);
    return 1;
}

int __cdecl Session_JoinSessionComplete(int slot)
{
    if ( slot >= 32
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    1142,
                    0,
                    "%s",
                    "slot < static_cast<int>( sizeof(overlappedTasks) / sizeof(overlappedTasks[0]) )") )
    {
        __debugbreak();
    }
    if ( !overlappedTasks_2[slot].active
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    1146,
                    0,
                    "%s",
                    "overlappedTasks[slot].active") )
    {
        __debugbreak();
    }
    if ( overlappedTasks_2[slot].type != 1
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    1147,
                    0,
                    "%s",
                    "overlappedTasks[slot].type == TASK_JOININGSESSION") )
    {
        __debugbreak();
    }
    TaskManager_ClearTask(&overlappedTasks_2[slot]);
    return 1;
}

void __cdecl Session_EndOngoingSessionTasks(SessionData_s *session)
{
    const char *v1; // eax
    taskCompleteResults started; // [esp+10h] [ebp-18h]
    bool done; // [esp+1Bh] [ebp-Dh]
    int tasknum; // [esp+24h] [ebp-4h]

    for ( tasknum = 0; tasknum < 32; ++tasknum )
    {
        if ( overlappedTasks_2[tasknum].active )
        {
            switch ( overlappedTasks_2[tasknum].type )
            {
                case 1:
                    if ( *((SessionData_s **)TaskManager_GetTaskData(&overlappedTasks_2[tasknum]) + 6) == session )
                    {
                        while ( overlappedTasks_2[tasknum].active )
                        {
                            if ( (unsigned int)Session_JoinSessionComplete(tasknum) >= 2 )
                                Com_PrintError(16, "EXE_ERROR_JOINING_SESSION");
                        }
                    }
                    break;
                case 2:
                    if ( *((SessionData_s **)TaskManager_GetTaskData(&overlappedTasks_2[tasknum]) + 1) == session )
                    {
                        done = 0;
                        while ( !done && overlappedTasks_2[tasknum].active )
                        {
                            started = Session_StartHostComplete(tasknum);
                            if ( started )
                            {
                                if ( started != TASK_COMPLETE )
                                    Com_PrintError(16, "EXE_ERROR_CREATING_SESSION");
                                done = 1;
                            }
                        }
                    }
                    break;
                case 5:
                    if ( session == &g_serverSession )
                    {
                        while ( overlappedTasks_2[tasknum].active )
                        {
                            if ( (unsigned int)Session_StartSessionComplete(tasknum) >= 2 )
                                Com_PrintError(16, "EXE_ERROR_STARTING_SESSION");
                        }
                    }
                    break;
                case 6:
                    if ( session == TaskManager_GetTaskData(&overlappedTasks_2[tasknum]) )
                    {
                        while ( overlappedTasks_2[tasknum].active )
                        {
                            if ( (unsigned int)Session_ModifyComplete(tasknum) >= TASK_ERROR )
                                Com_PrintError(16, "EXE_ERROR_MODIFYING_SESSION");
                        }
                    }
                    break;
                case 7:
                    while ( overlappedTasks_2[tasknum].active )
                    {
                        if ( (unsigned int)Session_EveryoneLeaveSessionComplete(tasknum) > 1 )
                            Com_PrintError(16, "EXE_ERROR_LEAVING_SESSION");
                    }
                    break;
                default:
                    v1 = va("Unknown session task type %i\n", overlappedTasks_2[tasknum].type);
                    if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp", 1976, 0, v1) )
                        __debugbreak();
                    break;
            }
        }
    }
    LB_EndOngoingTasks();
}

taskCompleteResults __cdecl Session_StartHostComplete(int slot)
{
    return TASK_NOTCOMPLETE;
}

int __cdecl Session_EveryoneLeaveSessionComplete(int slot)
{
    if ( slot >= 32
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    1164,
                    0,
                    "%s",
                    "slot < static_cast<int>( sizeof(overlappedTasks) / sizeof(overlappedTasks[0]) )") )
    {
        __debugbreak();
    }
    if ( !overlappedTasks_2[slot].active
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    1168,
                    0,
                    "%s",
                    "overlappedTasks[slot].active") )
    {
        __debugbreak();
    }
    if ( overlappedTasks_2[slot].type != 7
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp",
                    1169,
                    0,
                    "%s",
                    "overlappedTasks[slot].type == TASK_LEAVINGSESSION") )
    {
        __debugbreak();
    }
    TaskManager_ClearTask(&overlappedTasks_2[slot]);
    return 1;
}

void __cdecl Live_FinishOngoingSessionJoinTasksForXUID(unsigned __int64 player)
{
}

void __cdecl Live_CheckOngoingSessionTasks()
{
    const char *v0; // eax
    int tasknum; // [esp+18h] [ebp-4h]

    for ( tasknum = 0; tasknum < 32; ++tasknum )
    {
        if ( overlappedTasks_2[tasknum].active )
        {
            switch ( overlappedTasks_2[tasknum].type )
            {
                case 1:
                    if ( (unsigned int)Session_JoinSessionComplete(tasknum) >= 2 )
                        Com_PrintError(16, "EXE_ERROR_JOINING_SESSION");
                    break;
                case 2:
                    if ( (unsigned int)Session_StartHostComplete(tasknum) >= TASK_ERROR )
                        Com_PrintError(16, "EXE_ERROR_CREATING_SESSION");
                    break;
                case 4:
                    continue;
                case 5:
                    if ( (unsigned int)Session_StartSessionComplete(tasknum) >= 2 )
                        Com_PrintError(16, "EXE_ERROR_STARTING_SESSION");
                    break;
                case 6:
                    if ( (unsigned int)Session_ModifyComplete(tasknum) >= TASK_ERROR )
                        Com_PrintError(16, "EXE_ERROR_MODIFYING_SESSION");
                    break;
                case 7:
                    if ( (unsigned int)Session_EveryoneLeaveSessionComplete(tasknum) > 1 )
                        Com_PrintError(16, "EXE_ERROR_LEAVING_SESSION");
                    break;
                default:
                    v0 = va("Unknown session task type %i\n", overlappedTasks_2[tasknum].type);
                    if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_sessions_win.cpp", 2155, 0, v0) )
                        __debugbreak();
                    break;
            }
        }
    }
    Session_ManageGraveYard();
}

void Session_ManageGraveYard()
{
    int slot; // [esp+0h] [ebp-Ch]
    SessionData_s *session; // [esp+8h] [ebp-4h]

    if (!CG_IsShowingZombieMap())
    {
        for (slot = 0; slot < 6; ++slot)
        {
            if (sessionGraveYard[slot].active)
            {
                session = &sessionGraveYard[slot].sessionData;
                if (!Session_SessionTasksInProgress(session))
                {
                    Session_EndGameSession(session);
                    if (!Session_SessionTasksInProgress(session) && !Session_SessionTasksInProgress(session))
                    {
                        Session_DeleteHandle(&session->sessionHandle);
                        session->sessionHandle = 0;
                        sessionGraveYard[slot].active = 0;
                    }
                }
            }
        }
    }
}

void __cdecl Session_Init()
{
    memset(sessionJoinData, 0, sizeof(sessionJoinData));
    memset(sessionCreateData, 0, sizeof(sessionCreateData));
    TaskManager_ClearOverlappedTasks(overlappedTasks_2);
    g_serverSession.sessionName = (char*)"gameSession";
    g_serverSession.registerUsersWithVoice = 1;
    matchmaking_debug = _Dvar_RegisterBool("matchmaking_debug", 1, 0, "Enable matchmaking debugging information");
}

