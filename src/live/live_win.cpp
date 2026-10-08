#include "live_sessions_win.h"
#include "live_win.h"
#include "live_steam.h"
#include "live_friends_pc.h"
#include <qcommon/com_clients.h>
#include <client/client.h>
#include <client_mp/cl_main_pc_mp.h>
#include <game_mp/g_main_mp.h>
#include "live_pcache.h"
#include "live_steam_achievements.h"
#include <universal/com_files.h>
#include <win32/win_net.h>
#include <win32/win_shared.h>
#include <win32/win_gamerprofile.h>
#include "live_storage_win.h"
#include <client/splitscreen.h>
#include <server_mp/sv_main_mp.h>
#include <gfx_d3d/r_rendercmds.h>

const char *bot_difficulties[5] = { "easy", "normal", "hard", "fu", NULL };

unsigned int s_blockedListCount;
unsigned __int64 s_blockedList[50];

bool g_shouldComError;
const char *g_comErrorString;

bool g_presenceSecKeyAndIDRegistered;

XenonUserData xenonUserData[1];

unsigned __int64 s_selectedPlayerXUID;
unsigned __int64 s_selectedMetPlayerXUID;

int s_signInRequirement[1];

bool g_shouldWeHost = true;



const dvar_t *live_service;
const dvar_t *pc_newversionavailable;
const dvar_t *xblive_loggedin;
const dvar_t *xenon_voiceDebug;
const dvar_t *xenon_voiceDegrade;
const dvar_t *getdlcmapsfrommaindrive;
const dvar_t *session_nonblocking;
const dvar_t *systemUiActive;
const dvar_t *xblive_showmarketplace;
const dvar_t *xblive_clanmatch;
const dvar_t *xblive_theater;
const dvar_t *xblive_hostingprivateparty;
const dvar_t *xblive_privatepartyclient;
const dvar_t *xblive_wagermatch;
const dvar_t *xblive_basictraining;
const dvar_t *xblive_basictraining_popup;
const dvar_t *bot_tips;
const dvar_t *xblive_clanListChanged;
const dvar_t *teamsplitter_verbose;
const dvar_t *xblive_matchEndingSoon;
const dvar_t *ui_isClanMember;
const dvar_t *xenon_maxVoicePacketsPerSec;
const dvar_t *xenon_maxVoicePacketsPerSecForServer;
const dvar_t *xblive_mappacks;
const dvar_t *bot_friends;
const dvar_t *bot_enemies;
const dvar_t *steamid;
const dvar_t *scr_bot_difficulty;
const dvar_t *clancard_clanid;
//const dvar_t *clanName;
const dvar_t *bot_difficulty;



char __cdecl Live_ContentRatingAllowed()
{
    if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp", 603, 0, "Not implemented for PC") )
        __debugbreak();
    return 1;
}

bool __cdecl Live_IsUserSignedInToLive()
{
    return LiveSteam_IsClientSignedInOnline();
}

char __cdecl Live_RequireUserToPlayOnline()
{
    if ( LiveSteam_IsClientSignedInOnline() )
    {
        if ( !Live_IsUserSignedInToLive()
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp",
                        718,
                        0,
                        "%s",
                        "Live_IsUserSignedInToLive( controllerIndex )") )
        {
            __debugbreak();
        }
        return 1;
    }
    else
    {
        //BLOPS_NULLSUB();
        return 0;
    }
}

char *__cdecl Live_ControllerIndex_GetClientName(int controllerIndex)
{
    return (char*)"kisak"; // KISAKTODO: idc
    //if ( controllerIndex < 0
    //    && !Assert_MyHandler(
    //                "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp",
    //                809,
    //                0,
    //                "%s",
    //                (const char *)&stru_D50258.alloc) )
    //{
    //    __debugbreak();
    //}
    //if ( controllerIndex >= 1
    //    && !Assert_MyHandler(
    //                "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp",
    //                810,
    //                0,
    //                "%s",
    //                "controllerIndex < MAX_GPAD_COUNT") )
    //{
    //    __debugbreak();
    //}
    //return &byte_A61C02C[80 * controllerIndex];
}

int __cdecl CL_ControllerIndex_GetSignInState(int controllerIndex)
{
    if ( controllerIndex
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp",
                    826,
                    0,
                    "%s\n\t(controllerIndex) = %i",
                    "(controllerIndex >= 0 && controllerIndex < 1)",
                    controllerIndex) )
    {
        __debugbreak();
    }
    return xenonUserData[controllerIndex].signinState;
}

void __cdecl Live_ToggleMute_f()
{
    const char *v1; // eax
    int LocalClientNum; // eax
    clientActive_t *LocalClientGlobals; // [esp+14h] [ebp-1Ch]
    unsigned int playerIndex; // [esp+18h] [ebp-18h]
    unsigned __int64 playerXuid; // [esp+20h] [ebp-10h]
    int i; // [esp+28h] [ebp-8h]

    if (Cmd_Argc() == 2)
    {
        v1 = Cmd_Argv(1);
        playerXuid = I_atoi64(v1);
        playerIndex = -1;
        LocalClientNum = Com_ControllerIndex_GetLocalClientNum(0);
        LocalClientGlobals = CL_GetLocalClientGlobals(LocalClientNum);
        if (LocalClientGlobals->snap.valid)
        {
            for (i = 0; i < LocalClientGlobals->snap.numClients; ++i)
            {
                if (LocalClientGlobals->parseClients[((_WORD)i + (unsigned __int16)LocalClientGlobals->snap.parseClientsNum)
                    & 0x7FF].xuid == playerXuid)
                {
                    playerIndex = LocalClientGlobals->parseClients[((_WORD)i
                        + (unsigned __int16)LocalClientGlobals->snap.parseClientsNum)
                        & 0x7FF].clientIndex;
                    break;
                }
            }
        }
        if (playerXuid)
            CL_MutePlayer(0, playerIndex);
        else
            Com_Printf(30, "party_mutePlayer: Invalid player xuid sent.\n");
    }
    else
    {
        Com_Printf(30, "USAGE: mp_mutePlayer <selected player xuid>\n");
    }
}

cmd_function_s Live_ToggleMute_f_VAR;

void __cdecl Live_InitPlatform()
{
    memset(xenonUserData, 0, sizeof(xenonUserData));
    s_signInRequirement[0] = 0;
    live_service = _Dvar_RegisterBool("live_service", 1, 0x10u, "online service on/off");
    if ( G_ExitAfterToolComplete() )
    {
        Dvar_SetBool((dvar_s *)live_service, 0);
    }
    else
    {
        steamid = _Dvar_RegisterString("steamid", (char *)"", 0x40u, "Player's SteamID");
        pc_newversionavailable = _Dvar_RegisterBool(
                                                             "pc_newversionavailable",
                                                             0,
                                                             0x40u,
                                                             "True if new version available for download");
        PCache_Init();
        Cmd_AddCommandInternal("mp_toggleMute", Live_ToggleMute_f, &Live_ToggleMute_f_VAR);
        Session_Init();
        g_shouldWeHost = 1;
        LiveStorage_Init();
        Friends_Init();
        xblive_loggedin = _Dvar_RegisterBool("xblive_loggedin", 0, 0, "User is logged into online service");
        xenon_voiceDebug = _Dvar_RegisterBool("xenon_voiceDebug", 0, 0, "Debug voice communication");
        xenon_voiceDegrade = _Dvar_RegisterBool("xenon_voiceDegrade", 0, 0, "Degrade voice quality");
        xenon_maxVoicePacketsPerSec = _Dvar_RegisterInt(
                                                                        "xenon_maxVoicePacketsPerSec",
                                                                        100,
                                                                        0,
                                                                        0x7FFFFFFF,
                                                                        0,
                                                                        "Max voice packets per second a client will send");
        xenon_maxVoicePacketsPerSecForServer = _Dvar_RegisterInt(
                                                                                         "xenon_maxVoicePacketsPerSecForServer",
                                                                                         20,
                                                                                         0,
                                                                                         0x7FFFFFFF,
                                                                                         0,
                                                                                         "Max voice packets per second the server will send");
        getdlcmapsfrommaindrive = _Dvar_RegisterBool(
                                                                "getdlcmapsfrommaindrive",
                                                                0,
                                                                0,
                                                                "false = load the dlc maps from content packages, true = load the dlc maps from the local"
                                                                " machine harddrive");
        session_nonblocking = _Dvar_RegisterBool("session_nonblocking", 1, 0, "Non-blocking Session code");
        systemUiActive = _Dvar_RegisterBool("systemUiActive", 0, 0, "Is the system UI active");
        xblive_showmarketplace = _Dvar_RegisterBool(
                                                             "xblive_showmarketplace",
                                                             0,
                                                             0,
                                                             "true if the player doesn't have all the map packs");
        xblive_clanmatch = _Dvar_RegisterBool("xblive_clanmatch", 0, 0, "Current game is a clan match");
        xblive_theater = _Dvar_RegisterBool("xblive_theater", 0, 0, "Current game is a theater mode");
        xblive_hostingprivateparty = _Dvar_RegisterBool(
                                                                     "xblive_hostingprivateparty",
                                                                     0,
                                                                     0,
                                                                     "true only if we're hosting a party");
        xblive_privatepartyclient = _Dvar_RegisterBool(
                                                                    "xblive_privatepartyclient",
                                                                    0,
                                                                    0,
                                                                    "true only if we're in a party and not the host");
        xblive_mappacks = _Dvar_RegisterInt(
                                                "xblive_mappacks",
                                                0,
                                                0,
                                                2,
                                                0,
                                                "0 = original maps only, 1 = new maps only, 2 = both original and new");
        xblive_wagermatch = _Dvar_RegisterBool("xblive_wagermatch", 0, 4u, "Current game is a wager match");
        xblive_basictraining = _Dvar_RegisterBool("xblive_basictraining", 0, 4u, "Current game is basic training");
        xblive_basictraining_popup = _Dvar_RegisterBool(
                                                                     "xblive_basictraining_popup",
                                                                     0,
                                                                     1u,
                                                                     "The user has seen the basic training popup description");
        scr_bot_difficulty = _Dvar_RegisterString(
                                                     "scr_bot_difficulty",
                                                     (char *)"",
                                                     1u,
                                                     "Difficulty level of the basic training bots.");
        bot_friends = _Dvar_RegisterInt("bot_friends", 6, 1, 11, 0, "Number of friends allowed in basic training");
        bot_enemies = _Dvar_RegisterInt("bot_enemies", 6, 1, 11, 0, "Number of enemies allowed in basic training");
        bot_difficulty = _Dvar_RegisterEnum(
                                             "bot_difficulty",
                                             bot_difficulties,
                                             1,
                                             1u,
                                             "Difficulty level of the basic training bots");
        bot_tips = _Dvar_RegisterBool("bot_tips", 1, 1u, "Combat tips enabled in basic training");
        xblive_clanListChanged = _Dvar_RegisterBool("xblive_clanListChanged", 0, 0, "Clan list gets updated");
        teamsplitter_verbose = _Dvar_RegisterBool(
                                                         "teamsplitter_verbose",
                                                         0,
                                                         0,
                                                         "Verbose debug output while splitting teams if true.");
        xblive_matchEndingSoon = _Dvar_RegisterBool("xblive_matchEndingSoon", 0, 0, "True if the match is ending soon");
        clancard_clanid = _Dvar_RegisterString(
                                                "clancard_clanid",
                                                "0",
                                                0,
//                                                (const char *)&stru_D50258.Al);
                                                "");
        ui_isClanMember = _Dvar_RegisterBool(
                                                "ui_isClanMember",
                                                0,
                                                0,
                                                //(const char *)stru_D50258.MCU_membership);
                                                "");
        getdlcmapsfrommaindrive = _Dvar_RegisterBool(
                                                                "getdlcmapsfrommaindrive",
                                                                0,
                                                                0,
                                                                "false = load the dlc maps from content packages, true = load the dlc maps from the local"
                                                                " machine harddrive");
        clanName = _Dvar_RegisterString("clanName", (char *)"", 0, "Your clan abbreviation");
        g_presenceSecKeyAndIDRegistered = 0;
    }
}

int __cdecl Live_GetControllerFromXUID(unsigned __int64 player)
{
    int index; // [esp+4h] [ebp-4h]

    for ( index = 0; index < 1; ++index )
    {
        if (xenonUserData[index].signinState
            && __PAIR64__(HIDWORD(xenonUserData[index].xuid), xenonUserData[index].xuid) == player)
            return index;
    }
    return -1;
}

void __cdecl Live_GiveAchievement(int localControllerIndex, const char *achievementName)
{
    if ( !fs_gameDirVar || !*(_BYTE *)fs_gameDirVar->current.integer )
        LiveSteam_GiveAchievement(achievementName);
}

bool __cdecl Live_IsInLiveGame()
{
    return g_serverSession.sessionHandle && onlinegame->current.enabled;
}

unsigned __int64 g_fakeXUID; // KISAKTODO: value?
char __cdecl XUserGetXUID(int controllerIndex, unsigned __int64 *xuid)
{
    return 1;
}

char __cdecl Live_UserGetName(int controllerIndex, char *buf, int bufsize)
{
    return 1;
}

bool __cdecl Live_UserSignedInLocally(int controllerIndex, char **disconnectMessage)
{
    unsigned int v2; // eax
    unsigned int oldState; // [esp+14h] [ebp-Ch]
    bool shouldDisconnect; // [esp+1Bh] [ebp-5h]
    unsigned int signInFunctionStartTime; // [esp+1Ch] [ebp-4h]

    Com_Printf(16, "Controller #%i signed in locally\n", controllerIndex);
    oldState = xenonUserData[controllerIndex].signinState;
    signInFunctionStartTime = Sys_Milliseconds();
    Live_UserSignedIn(controllerIndex);
    v2 = Sys_Milliseconds();
    Com_Printf(16, "Live_UserSignedIn took %ims\n", v2 - signInFunctionStartTime);
    xenonUserData[controllerIndex].signinState = 1;
    Sys_Milliseconds();
    if ( oldState < 2 || oldState == 2 && !onlinegame->current.enabled || strlen(Dvar_GetString("com_errorMessage")) )
        return 0;
    shouldDisconnect = 1;
    *disconnectMessage = (char*)"XBOXLIVE_SIGNINCHANGED";
    return shouldDisconnect;
}

void Live_UserSignedIn(int controllerIndex)
{
    int v2; // eax
    DWORD v3; // eax
    unsigned __int64 newXuid; // [esp+8h] [ebp-58h] BYREF
    char xuidStr[20]; // [esp+14h] [ebp-4Ch] BYREF
    char newGamertag[32]; // [esp+28h] [ebp-38h] BYREF
    int startTime; // [esp+4Ch] [ebp-14h]
    unsigned __int64 xuidCopy; // [esp+50h] [ebp-10h] BYREF
    bool res; // [esp+5Bh] [ebp-5h]
    int netCodeVersion; // [esp+5Ch] [ebp-4h]

    res = XUserGetXUID(controllerIndex, &newXuid);
    if (!res && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp", 2705, 0, "%s", "res"))
        __debugbreak();
    res = Live_UserGetName(controllerIndex, newGamertag, 32);
    if (!res)
        Com_Error(ERR_DROP, "XBOXLIVE_SIGNEDOUTOFLIVE");
    Dvar_SetBool((dvar_s*)xblive_loggedin, 1);
    LiveStorage_NewUser(controllerIndex);
    I_strncpyz(xenonUserData[controllerIndex].gamertag, newGamertag, 32);
    v2 = controllerIndex;
    LODWORD(xenonUserData[v2].xuid) = newXuid;
    HIDWORD(xenonUserData[v2].xuid) = HIDWORD(newXuid);
    XUIDToString(&newXuid, xenonUserData[controllerIndex].xuidString);
    xenonUserData[controllerIndex].tier = USER_TIER_NONE;
    XUIDToString(&xenonUserData[controllerIndex].xuid, xuidStr);
    StringToXUID(xuidStr, &xuidCopy);
    if (memcmp(&xuidCopy, &xenonUserData[controllerIndex].xuid, 8u)
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp",
            2728,
            0,
            "%s",
            "memcmp( &xuidCopy, &xenonUserData[controllerIndex].xuid, sizeof(XUID) ) == 0"))
    {
        __debugbreak();
    }
    Dvar_SetString((dvar_s*)name, xenonUserData[controllerIndex].gamertag);
    if (!xenonUserData[controllerIndex].signinState)
    {
        startTime = Sys_Milliseconds();
        GamerProfile_LogInProfile(controllerIndex);
        v3 = Sys_Milliseconds();
        Com_Printf(16, "GamerProfile_LogInProfile took %ims\n", v3 - startTime);
    }
    netCodeVersion = 1044;
}

bool Live_UserSignedInToLive(int controllerIndex, char **disconnectMessage)
{
    int LocalClientNum; // eax
    int oldState; // [esp+10h] [ebp-8h]
    bool shouldDisconnect; // [esp+17h] [ebp-1h]

    Com_Printf(16, "Controller #%i signed in to live\n", controllerIndex);
    oldState = xenonUserData[controllerIndex].signinState;
    Live_UserSignedIn(controllerIndex);
    xenonUserData[controllerIndex].signinState = 2;
    xenonUserData[controllerIndex].isGuestUser = 0;
    LiveStorage_SetAllStatsNotFetched(controllerIndex);
    LiveStorage_ReadStats(controllerIndex, 0, 0);
    //BG_EvalVehicleName();
    if ((s_signInRequirement[controllerIndex] & 4) != 0)
    {
        //BG_EvalVehicleName();
        if (Live_ContentRatingAllowed())
        {
            LocalClientNum = Com_ControllerIndex_GetLocalClientNum(controllerIndex);
            Cbuf_ExecuteBuffer(LocalClientNum, controllerIndex, (char*)"xstartprivateparty");
        }
        else
        {
            Com_Printf(
                16,
                "Since the active profile doesn't have permission to play MP, so we are going to send them back to the main menu\n");
            *disconnectMessage = (char*)"XBOXLIVE_MPNOTALLOWED";
        }
    }
    if (!oldState
        || oldState == 2
        || oldState == 1 && !onlinegame->current.enabled
        || strlen(Dvar_GetString("com_errorMessage")))
    {
        return 0;
    }
    shouldDisconnect = 1;
    *disconnectMessage = (char *)"XBOXLIVE_SIGNINCHANGED";
    return shouldDisconnect;
}

bool __cdecl Live_IsSignedIn(int controllerIndex)
{
    if ( LiveSteam_IsInitialized()
        && !LiveSteam_IsClientSignedInLocally()
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp",
                    2909,
                    0,
                    "%s",
                    "!LiveSteam_IsInitialized() || LiveSteam_IsClientSignedInLocally()") )
    {
        __debugbreak();
    }
    return xenonUserData[controllerIndex].signinState > 0;
}

bool __cdecl Live_IsSignedInToLive()
{
    return LiveSteam_IsClientSignedInOnline();
}

unsigned long long __cdecl Live_GetXuid(int controllerIndex)
{
    //if ( !Live_IsSignedIn(controllerIndex)
    //    && !Assert_MyHandler(
    //                "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp",
    //                2972,
    //                0,
    //                "%s",
    //                "Live_IsSignedIn( controllerIndex )") )
    //{
    //    __debugbreak();
    //}
    return xenonUserData[controllerIndex].xuid;
}

int __cdecl Live_GetTier(int controllerIndex)
{
    if ( !Live_IsSignedIn(controllerIndex)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp",
                    2989,
                    0,
                    "%s",
                    "Live_IsSignedIn( controllerIndex )") )
    {
        __debugbreak();
    }
    return xenonUserData[controllerIndex].tier;
}

char __cdecl Live_ShowMarketplaceUI()
{
    LiveSteam_ShowStore();
    return 1;
}

void __cdecl PC_InitSigninState()
{
}

bool __cdecl Live_UserSignedOut(int controllerIndex)
{
    return 0;
}

void __cdecl Live_DelayedComError(const char *comErrorString)
{
    g_shouldComError = 1;
    g_comErrorString = comErrorString;
}

void __cdecl Live_Frame()
{
    ;
}

char __cdecl Live_IsUserBlocked(int controllerIndex, unsigned __int64 xuid)
{
    unsigned int idx; // [esp+4h] [ebp-4h]

    for ( idx = 0; idx < s_blockedListCount; ++idx )
    {
        if ( __PAIR64__(HIDWORD(s_blockedList[idx]), s_blockedList[idx]) == xuid )
            return 1;
    }
    return 0;
}

bool __cdecl Live_CanConsoleViewContentFromUser(unsigned __int64 xuid)
{
    int i; // [esp+0h] [ebp-8h]
    bool canView; // [esp+7h] [ebp-1h]

    canView = 1;
    for ( i = 0; i < 1; ++i )
    {
        if ( Live_IsUserSignedInToLive() && !Live_CanViewContentFromUser(i, xuid) )
            canView = 0;
    }
    return canView;
}

bool __cdecl Live_CanViewContentFromUser(int controllerIndex, unsigned __int64 xuid)
{
    return Live_IsUserBlocked(controllerIndex, xuid) == 0;
}

