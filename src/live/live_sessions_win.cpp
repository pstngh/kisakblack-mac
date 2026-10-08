#include "live_sessions_win.h"
#include <client_mp/cl_main_pc_mp.h>
#include <gfx_d3d/r_rendercmds.h>
#include "live_storage_win.h"
#include "live_win.h"
#include "live_stats.h"
#include <game_mp/g_main_mp.h>
#include <server_mp/sv_init_mp.h>
#include <cgame/cg_compass.h>

SessionData_s g_serverSession;

const dvar_t *matchmaking_debug;

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

void __cdecl Session_StartHost(SessionData_s *session, int sessionFlags, int numPrivateSlots, int numPublicSlots)
{
}

void __cdecl Session_Modify(
                int localControllerIndex,
                SessionData_s *session,
                int flags,
                int publicSlots,
                int privateSlots)
{
}

void __cdecl Session_Init()
{
    g_serverSession.sessionName = (char*)"gameSession";
    g_serverSession.registerUsersWithVoice = 1;
    matchmaking_debug = _Dvar_RegisterBool("matchmaking_debug", 1, 0, "Enable matchmaking debugging information");
}

