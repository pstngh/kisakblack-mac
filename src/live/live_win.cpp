#include "live_win.h"
#include <win32/win_tasks.h>
#include "live_steam.h"
#include <DW/dwLogOn_pc.h>
#include "live_friends_pc.h"
#include <qcommon/com_clients.h>
#include <client/client.h>
#include <client_mp/cl_main_pc_mp.h>
#include <game_mp/g_main_mp.h>
#include "live_sessions_win.h"
#include "live_groups_dw.h"
#include "live_pcache.h"
#include "live_meetplayer.h"
#include "live_leaderboard.h"
#include "live_counter.h"
#include <DW/dwUtils_pc.h>
#include "live_steam_achievements.h"
#include <universal/com_files.h>
#include <win32/win_net.h>
#include <DW/dwStats.h>
#include <win32/win_shared.h>
#include <win32/win_gamerprofile.h>
#include "live_storage_win.h"
#include <DW/dwUtils.h>
#include <client/splitscreen.h>
#include <server_mp/sv_main_mp.h>
#include <gfx_d3d/r_rendercmds.h>

const char *bot_difficulties[5] = { "easy", "normal", "hard", "fu", NULL };

PrivateProfileInfo s_profileInfo;
favourite_t s_favourites[42];
recentServer_t s_recentServers[30];
unsigned __int8 s_recentServersBuf[480];
unsigned int s_blockedListCount;
unsigned __int64 s_blockedList[50];

bool g_shouldComError;
const char *g_comErrorString;

bool g_presenceSecKeyAndIDRegistered;
bool s_updatePerformanceValues;
int s_performanceValueTimer;
int s_uploadBitsPerSec;

MatchMakingInfo *g_matchmakingInfo;
overlappedTask overlappedTasks_3[32];
XenonUserData xenonUserData[1];
unsigned __int64 s_lastInvite;

unsigned __int64 s_selectedPlayerXUID;
unsigned __int64 s_selectedMetPlayerXUID;

int s_signInRequirement[1];

bool g_shouldWeHost = true;



const dvar_t *live_service;
const dvar_t *dw_loggedin;
const dvar_t *dw_active;
const dvar_t *pc_newversionavailable;
const dvar_t *dw_dupe_key;
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
const dvar_t *party_simulateLongQoS;
const dvar_t *xblive_clanListChanged;
const dvar_t *teamsplitter_verbose;
const dvar_t *xblive_matchEndingSoon;
const dvar_t *ui_isClanMember;
const dvar_t *dw_numaccounts;
const dvar_t *xenon_maxVoicePacketsPerSec;
const dvar_t *xenon_maxVoicePacketsPerSecForServer;
const dvar_t *bandwidth_retry_interval;
const dvar_t *xblive_mappacks;
const dvar_t *bot_friends;
const dvar_t *bot_enemies;
const dvar_t *invite_waitPeriod;
const dvar_t *steamid;
const dvar_t *dw_popup;
const dvar_t *inviteText;
const dvar_t *scr_bot_difficulty;
const dvar_t *clancard_clanid;
//const dvar_t *clanName;
const dvar_t *bot_difficulty;
const dvar_t *dw_usernames[5];



void __cdecl Live_ClearDWOverlappedTasks()
{
    TaskManager_ClearOverlappedTasks(overlappedTasks_3);
}

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

bool __cdecl Live_IsUserSignedInToDemonware(int controllerIndex)
{
    return false;
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

void __cdecl Live_InitiateDemonWareConnect_f()
{
}

void __cdecl Live_SendInvite_f()
{
    const char *friendName; // [esp+10h] [ebp-4h]

    if ( Cmd_Argc() >= 2 )
    {
        friendName = Cmd_Argv(1);
        Live_SendInvite(friendName);
    }
    else
    {
        Com_PrintError(0, (char*)"PLATFORM_STEAM_OFFLINE");
    }
}

void __cdecl Live_AcceptInvite_f()
{
    const char *v0; // eax
    const char *v1; // eax
    unsigned __int64 friendid; // [esp+18h] [ebp-8h] BYREF

    if ( Cmd_Argc() == 2 )
    {
        v0 = Cmd_Argv(1);
        if ( Friend_GetByName(0, v0, &friendid) )
        {
            Live_AcceptInvite(friendid);
        }
        else
        {
            v1 = Cmd_Argv(1);
            //Com_PrintError(23, (char *)&stru_D50258.do_fancy_upsampling, v1);
            Com_PrintError(23, "%s", v1);
        }
    }
    else
    {
        Com_PrintError(0, (char *)"(kisak) need more args"); // (char *)&stru_D50258.output_components);
    }
}

void __cdecl Live_AcceptLastInvite_f()
{
    if ( s_lastInvite )
        Live_AcceptInvite(s_lastInvite);
}

void __cdecl Live_RevokeInvite_f()
{
    const char *v0; // eax
    const char *v1; // eax
    unsigned __int64 friendid; // [esp+18h] [ebp-8h] BYREF

    if ( Cmd_Argc() == 2 )
    {
        v0 = Cmd_Argv(1);
        if ( Friend_GetByName(0, v0, &friendid) )
        {
            Live_RevokeInvite(friendid);
        }
        else
        {
            v1 = Cmd_Argv(1);
            Com_PrintError(23, (char *)"Couldn't get id for friend %s", v1);
        }
    }
    else
    {
        Com_PrintError(0, (char *)"USAGE: revokeinvite <friendname>\n");
    }
}

void __cdecl Live_JoinSessionInProgress_f()
{
    const char *v0; // eax
    unsigned __int64 v1; // rax
    const char *v2; // eax
    unsigned __int64 friendid; // [esp+20h] [ebp-8h] BYREF

    if ( Cmd_Argc() == 2 )
    {
        v0 = Cmd_Argv(1);
        if ( Friend_GetByName(0, v0, &friendid) )
        {
            Live_JoinSessionInProgress(friendid, 0);
        }
        else if ( s_selectedPlayerXUID )
        {
            Live_JoinSessionInProgress(s_selectedPlayerXUID, 0);
        }
        else if ( __PAIR64__(s_selectedMetPlayerXUID, 0) == HIDWORD(s_selectedMetPlayerXUID) )
        {
            v2 = Cmd_Argv(1);
            Com_PrintError(23, (char *)"Couldn't get id for friend %s", v2);
        }
        else
        {
            LODWORD(v1) = LiveMeetPlayer_GetPlayerSessionByID(s_selectedMetPlayerXUID);
            if ( v1 )
                Live_JoinSessionInProgress(v1, 1);
        }
    }
    else
    {
        Com_PrintError(23, (char *)"Usage: joinsession <friendName>\n");
    }
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

cmd_function_s Live_InitiateDemonWareConnect_f_VAR;
cmd_function_s Live_UpdateInfoForInGameList_f_VAR;
cmd_function_s Live_JoinSessionInProgress_f_VAR;
cmd_function_s Live_SendInvite_f_VAR;
cmd_function_s Live_AcceptInvite_f_VAR;
cmd_function_s Live_RevokeInvite_f_VAR;
cmd_function_s Live_ListInvites_f_VAR;
cmd_function_s Live_AddPlayerAsFriend_f_VAR;
cmd_function_s Live_AcceptLastInvite_f_VAR;
cmd_function_s Live_ToggleMute_f_VAR;

const char *dwUsers[5] =
{
    "dw_user0",
    "dw_user1",
    "dw_user2",
    "dw_user3",
    "dw_user4",
};

void __cdecl Live_InitPlatform()
{
    int i; // [esp+0h] [ebp-8h]
    int controllerIndex; // [esp+4h] [ebp-4h]

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
        dw_loggedin = _Dvar_RegisterBool("dw_loggedin", 0, 0x40u, "Every frame is updated with Demonware login status");
        dw_active = _Dvar_RegisterBool("dw_active", 1, 0, "Pumps Live_Frame() (and hence DW) if true");
        pc_newversionavailable = _Dvar_RegisterBool(
                                                             "pc_newversionavailable",
                                                             0,
                                                             0x40u,
                                                             "True if new version available for download");
        dw_dupe_key = _Dvar_RegisterBool("dw_dupe_key", 0, 0x40u, "True if key-in-use message from Demonware");
        dw_numaccounts = _Dvar_RegisterInt(
                                             "dw_numaccounts",
                                             -1,
                                             -1,
                                             5,
                                             0,
                                             "Number of online accounts registered for the license");
        for ( i = 5;
            i--; 
            dw_usernames[i] = _Dvar_RegisterString(dwUsers[i], (char *)"", 0, "Online user name registered for the license") )
        {
            ;
        }
        dw_popup = _Dvar_RegisterString("dw_popup", (char *)"", 0, "Online services popup");
        Live_InitFavourites();
        for ( controllerIndex = 0; controllerIndex < 1; ++controllerIndex )
            //BLOPS_NULLSUB();
        LiveGroups_Init();
        PCache_Init();
        Cmd_AddCommandInternal(
            "initiateDemonWareConnect",
            Live_InitiateDemonWareConnect_f,
            &Live_InitiateDemonWareConnect_f_VAR);
        Cmd_AddCommandInternal("updateInfoForInGameList", BLOPS_NULLSUB, &Live_UpdateInfoForInGameList_f_VAR);
        Cmd_AddCommandInternal("JoinsessionInProgress", Live_JoinSessionInProgress_f, &Live_JoinSessionInProgress_f_VAR);
        Cmd_AddCommandInternal("sendinvite", Live_SendInvite_f, &Live_SendInvite_f_VAR);
        Cmd_AddCommandInternal("acceptinvite", Live_AcceptInvite_f, &Live_AcceptInvite_f_VAR);
        Cmd_AddCommandInternal("revokeinvite", Live_RevokeInvite_f, &Live_RevokeInvite_f_VAR);
        Cmd_AddCommandInternal("listinvites", Live_ListInvites_f, &Live_ListInvites_f_VAR);
        Cmd_AddCommandInternal("xaddfriend", Live_AddPlayerAsFriend_f, &Live_AddPlayerAsFriend_f_VAR);
        Cmd_AddCommandInternal("acceptInvitation", Live_AcceptLastInvite_f, &Live_AcceptLastInvite_f_VAR);
        Cmd_AddCommandInternal("mp_toggleMute", Live_ToggleMute_f, &Live_ToggleMute_f_VAR);
        Session_Init();
        g_shouldWeHost = 1;
        LB_Init();
        LiveStorage_Init();
        Friends_Init();
        //BLOPS_NULLSUB();
        LiveCounter_Init();
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
        inviteText = _Dvar_RegisterString("inviteText", (char *)"", 0, "Text to display for the game invite");
        systemUiActive = _Dvar_RegisterBool("systemUiActive", 0, 0, "Is the system UI active");
        bandwidth_retry_interval = _Dvar_RegisterInt(
                                                                 "bandwidth_retry_interval",
                                                                 180000,
                                                                 0,
                                                                 0x7FFFFFFF,
                                                                 0,
                                                                 "Interval at which Bandwidth test will be retried");
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
        party_simulateLongQoS = _Dvar_RegisterBool(
                                                            "party_simulateLongQoS",
                                                            0,
                                                            0,
                                                            "simulate a real QoS which takes around 30 seconds");
        invite_waitPeriod = _Dvar_RegisterInt(
                                                    "invite_waitPeriod",
                                                    30000,
                                                    15000,
                                                    0x7FFFFFFF,
                                                    0,
                                                    "time in msec you have to wait between sending invites to the same friend");
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
        s_updatePerformanceValues = 0;
        s_performanceValueTimer = 0;
    }
}

bool __cdecl Live_HandleDWChallengeResponse(
                unsigned __int64 senderID,
                unsigned __int8 *message,
                unsigned int messageSize)
{
    return 1;
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

void __cdecl Session_CleanUpStatsWrites()
{
    int tasknum; // [esp+4h] [ebp-4h]

    R_BeginRemoteScreenUpdate();
    for ( tasknum = 0; tasknum < 32; ++tasknum )
    {
        if ( overlappedTasks_3[tasknum].active && overlappedTasks_3[tasknum].type == 2 )
        {
            while ( Live_SetPlayerTeamRankComplete(tasknum) == TASK_NOTCOMPLETE )
                NET_Sleep(1u);
        }
    }
    R_EndRemoteScreenUpdate(0);
}

taskCompleteResults __cdecl Live_SetPlayerTeamRankComplete(int slot)
{
    return Live_SetPlayerTeamRanksComplete(slot);
}

taskCompleteResults __cdecl Live_SetPlayerTeamRanksComplete(int slot)
{
    return TASK_COMPLETE;
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
    LiveGroups_RegisterPlayer(controllerIndex);
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
    Live_GetOurUploadBandwidth(controllerIndex);
    LiveStorage_FetchRequiredFiles(controllerIndex);
    LiveMeetPlayer_DownloadMetPlayersList(0);
    LiveCounter_Init();
    LiveCounter_SetupCounters();
    Live_GetPrivateProfile();
    Live_ReadRecentServers();
    Live_RequestSessionsFromFriends();
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
    if ( !LiveSteam_IsClientSignedInOnline() )
        return 0;
    return dw_active && dw_active->current.enabled;
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

int __cdecl Live_GetUploadSpeed()
{
    return s_uploadBitsPerSec;
}

void __cdecl Live_GetOurUploadBandwidth(int localControllerIndex)
{
}

char __cdecl Live_BandwidthTestInProgress()
{
    return TaskManager_TaskIsInProgress(overlappedTasks_3, 3);
}

void __cdecl Live_CheckOngoingTasks()
{
    const char *v0; // eax
    int tasknum; // [esp+1Ch] [ebp-4h]

    Sys_EnterCriticalSection(CRITSECT_LIVE);
    for ( tasknum = 0; tasknum < 32; ++tasknum )
    {
        if ( overlappedTasks_3[tasknum].active )
        {
            switch ( overlappedTasks_3[tasknum].type )
            {
                case 1:
                    if ( (unsigned int)Live_QoSProbeComplete(tasknum) > TASK_COMPLETE )
                        Com_PrintError(16, "Error getting player QoS results\n");
                    break;
                case 2:
                    if ( (unsigned int)Live_SetPlayerTeamRankComplete(tasknum) > TASK_COMPLETE )
                        Com_PrintError(16, "Error writing stats\n");
                    break;
                case 3:
                    if ( (unsigned int)Live_GetBandwidthTestComplete(tasknum) >= 2 )
                        Com_PrintError(16, "Error getting player bandwidth test results\n");
                    break;
                case 5:
                    if ( (unsigned int)Live_FetchPartyPerformanceValuesComplete() > 1 )
                        Com_PrintError(16, "Error fetching party performance values\n");
                    break;
                case 6:
                    if ( (unsigned int)Live_UpdatePerformanceValuesComplete(tasknum) > TASK_COMPLETE )
                        Com_PrintError(16, "Error fetching performance values\n");
                    break;
                case 7:
                    continue;
                case 8:
                    if ( (unsigned int)CL_LocalClient_GetActiveCount() > 1 )
                        Com_PrintError(16, "Error inviting friend\n");
                    break;
                default:
                    v0 = va("Unknown live task type %i\n", overlappedTasks_3[tasknum].type);
                    if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp", 3533, 0, v0) )
                        __debugbreak();
                    break;
            }
        }
    }
    Sys_LeaveCriticalSection(CRITSECT_LIVE);
}

int __cdecl Live_GetBandwidthTestComplete(int slot)
{
    return 0;
}

taskCompleteResults __cdecl Live_QoSProbeComplete(int slot)
{
    return TASK_NOTCOMPLETE;
}

bool __cdecl Live_QoSProbeEarlyComplete(dwQoSMultiProbeListener *listener)
{
    return 0;
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

int __cdecl Live_FetchPartyPerformanceValuesComplete()
{
    if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_win.cpp", 4924, 0, "PC MP not using Party") )
        __debugbreak();
    return 2;
}

taskCompleteResults __cdecl Live_UpdatePerformanceValuesComplete(int slot)
{
    return TASK_COMPLETE;
}

void Live_UpdateAveragePerformance()
{
}

int __cdecl Live_GetAveragePerformance()
{
    int avgPerformance; // [esp+0h] [ebp-20h]
    int sessionSlot; // [esp+4h] [ebp-1Ch]
    int numMembers; // [esp+8h] [ebp-18h]
    int totalPerformance; // [esp+Ch] [ebp-14h]
    int i; // [esp+10h] [ebp-10h]
    unsigned __int64 xuid; // [esp+18h] [ebp-8h]

    totalPerformance = 0;
    avgPerformance = 0;
    numMembers = 0;
    for ( i = 0; i < com_maxclients->current.integer; ++i )
    {
        xuid = 0;
        if ( svs.clients && svs.clients[i].header.state >= CS_CONNECTED )
        {
            LODWORD(xuid) = svs.xuids[i];
            HIDWORD(xuid) = LODWORD(svs.mapCenter[2 * i - 63]);
        }
        if ( xuid )
        {
            sessionSlot = Session_FindRegisteredUser(&g_serverSession, xuid);
            if ( sessionSlot >= 0 )
            {
                totalPerformance += g_serverSession.registeredUsers[sessionSlot].performanceValue;
                ++numMembers;
            }
        }
    }
    if ( numMembers > 0 )
        return totalPerformance / numMembers;
    return avgPerformance;
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

void __cdecl Live_SendInvite(const char *friendName)
{
}

void __cdecl Live_DumpFavourites()
{
    unsigned int i; // [esp+4h] [ebp-4h]

    for ( i = 0; i < 0x2A; ++i )
    {
        if ( LODWORD(s_favourites[i].uid) || HIDWORD(s_favourites[i].uid) )
            Com_DPrintf(0, "%llu\n", *(_QWORD *)s_favourites[i].addressblob);
    }
}

void __cdecl Live_FindFavouriteServersSuccess(TaskRecord *task)
{
}

bool __cdecl Live_FindFavouritesInProgress()
{
    return false;
}

void __cdecl Live_FindFavouriteServers()
{
}

void __cdecl Live_FindFriendServersSuccess(TaskRecord *task)
{
}

void __cdecl Live_FindFriendServers()
{
}

void __cdecl Live_FindRecentServersSuccess(TaskRecord *task)
{
}

void __cdecl Live_FindRecentServers()
{
}

void __cdecl Live_SaveRecentServers()
{
    unsigned __int8 *ptr; // [esp+8h] [ebp-8h]
    unsigned __int8 *ptra; // [esp+8h] [ebp-8h]
    int i; // [esp+Ch] [ebp-4h]

    ptr = s_recentServersBuf;
    for ( i = 0; i < 30 && (LODWORD(s_recentServers[i].serverID) || HIDWORD(s_recentServers[i].serverID)); ++i )
    {
        *(unsigned int *)ptr = s_recentServers[i].serverID;
        *((unsigned int *)ptr + 1) = HIDWORD(s_recentServers[i].serverID);
        ptra = ptr + 8;
        *(unsigned int *)ptra = s_recentServers[i].joinTime;
        ptr = ptra + 4;
    }
    Com_DPrintf(23, "Serialized %i (%i bytes) recent servers\n", i, ptr - s_recentServersBuf);
    LiveStorage_SaveRecentServers(s_recentServersBuf, ptr - s_recentServersBuf);
}

void __cdecl Live_ReadRecentServers()
{
    LiveStorage_ReadRecentServers(s_recentServersBuf, 480);
}

void __cdecl Live_PopulateRecentServers(unsigned __int8 *buf, int bufsize)
{
    int v2; // ecx
    int i; // [esp+0h] [ebp-Ch]
    unsigned __int8 *ptr; // [esp+8h] [ebp-4h]

    ptr = buf;
    for ( i = 0; i < 30 && ptr != &buf[bufsize]; ++i )
    {
        v2 = i;
        LODWORD(s_recentServers[v2].serverID) = *(unsigned int *)ptr;
        *(unsigned int *)(v2 * 16 + 174315068) = *((unsigned int *)ptr + 1);
        s_recentServers[i].joinTime = *((unsigned int *)ptr + 2);
        ptr += 12;
    }
}

void __cdecl Live_AddRecentServer(unsigned __int64 serveruid)
{
    recentServer_t *serverent; // [esp+4h] [ebp-Ch]
    int currentTime; // [esp+8h] [ebp-8h]
    int i; // [esp+Ch] [ebp-4h]

    currentTime = LiveStorage_GetUTC();
    for ( i = 0; i < 30; ++i )
    {
        serverent = &s_recentServers[i];
        if ( serveruid == serverent->serverID )
        {
            serverent->joinTime = currentTime;
            break;
        }
        if ( !serverent->joinTime )
        {
            serverent->serverID = serveruid;
            serverent->joinTime = currentTime;
            break;
        }
    }
    if ( i == 30 )
    {
        s_recentServers[29].joinTime = currentTime;
        s_recentServers[29].serverID = serveruid;
    }
    qsort(s_recentServers, 0x1Eu, 0x10u, (int (__cdecl *)(const void *, const void *))compareRecentServers);
    Live_SaveRecentServers();
}

int __cdecl compareRecentServers(unsigned int *server1, unsigned int *server2)
{
    return server2[2] - server1[2];
}

void __cdecl Live_GetFriendsOnServer(unsigned __int64 serverId, unsigned __int64 *friendIDs, int *numfriends)
{
}

char __cdecl Live_AddFavourite_Ingame(unsigned __int64 serverid, unsigned __int64 serveruid)
{
    return 0;
}

void __cdecl Live_AddFavourite(unsigned __int64 serverid, unsigned __int64 serveruid)
{
    favourite_t *v2; // ecx
    unsigned int v3; // ecx
    unsigned int i; // [esp+4h] [ebp-4h]

    for ( i = 0; i < 0x2A && (LODWORD(s_favourites[i].uid) || HIDWORD(s_favourites[i].uid)); ++i )
        ;
    if ( i < 0x2A )
    {
        v2 = &s_favourites[i];
        *(unsigned int *)v2->addressblob = serverid;
        *(_WORD *)&v2->addressblob[4] = WORD2(serverid);
        v3 = i;
        LODWORD(s_favourites[v3].uid) = serveruid;
        *(unsigned int *)(v3 * 16 + 174313908) = HIDWORD(serveruid);
    }
    CL_SetFavourites_f();
}

void __cdecl Live_DeleteFavourite(unsigned __int64 serverid)
{
    unsigned int v1; // eax
    unsigned int i; // [esp+4h] [ebp-4h]

    for ( i = 0; i < 0x2A && serverid != __PAIR64__(HIDWORD(s_favourites[i].uid), s_favourites[i].uid); ++i )
        ;
    if ( i < 0x2A )
    {
        v1 = i;
        LODWORD(s_favourites[v1].uid) = 0;
        *(unsigned int *)(v1 * 16 + 174313908) = 0;
        CL_SetFavourites_f();
    }
}

void __cdecl Live_ParseFavsBlobs(unsigned __int8 *addrblob, unsigned __int8 *uidblob)
{
    favourite_t *v2; // eax
    unsigned int v3; // edx
    unsigned int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < 0x2A && *uidblob; ++i )
    {
        v2 = &s_favourites[i];
        *(unsigned int *)v2->addressblob = *(unsigned int *)addrblob;
        *(_WORD *)&v2->addressblob[4] = *((_WORD *)addrblob + 2);
        v3 = i;
        LODWORD(s_favourites[v3].uid) = *(unsigned int *)uidblob;
        *(unsigned int *)(v3 * 16 + 174313908) = *((unsigned int *)uidblob + 1);
        addrblob += 6;
        uidblob += 8;
    }
}

void __cdecl Live_SetFavsBlobs(unsigned __int8 *addrblob, unsigned __int8 *uidblob)
{
    favourite_t *v2; // eax
    unsigned int i; // [esp+4h] [ebp-4h]

    memset(addrblob, 0, 0xFDu);
    memset(uidblob, 0, 0x151u);
    for ( i = 0; i < 0x2A; ++i )
    {
        if ( LODWORD(s_favourites[i].uid) || HIDWORD(s_favourites[i].uid) )
        {
            v2 = &s_favourites[i];
            *(unsigned int *)addrblob = *(unsigned int *)v2->addressblob;
            *((_WORD *)addrblob + 2) = *(_WORD *)&v2->addressblob[4];
            addrblob += 6;
            *(unsigned int *)uidblob = s_favourites[i].uid;
            *((unsigned int *)uidblob + 1) = HIDWORD(s_favourites[i].uid);
            uidblob += 8;
        }
    }
}

void __cdecl Live_GetPrivateProfileComplete()
{
    Live_ParseFavsBlobs(s_profileInfo.m_memberfavsblob, s_profileInfo.m_memberuids);
}

void __cdecl Live_GetPrivateProfileFailure()
{
    serverInfo_t *favservers; // [esp+0h] [ebp-8h] BYREF
    int *count; // [esp+4h] [ebp-4h] BYREF

    Com_PrintWarning(23, "Couldn't get favourites :(\n");
    favservers = 0;
    count = 0;
    if ( CL_GetServerList(3, &favservers, &count) )
        *count = 0;
}

void __cdecl Live_SetPrivateProfileComplete()
{
    Com_DPrintf(23, "Successfully saved favourites to DemonWare\n");
}

void __cdecl Live_SetPrivateProfileFailure()
{
    Com_PrintWarning(23, "Couldn't save favourites to DemonWare :(\n");
}

TaskRecord *__cdecl Live_GetPrivateProfile()
{
    return NULL;
}

TaskRecord *__cdecl Live_SetPrivateProfile()
{
    return NULL;
}

void __cdecl CL_GetFavourites_f()
{
    Live_GetPrivateProfile();
}

void __cdecl CL_SetFavourites_f()
{
    Live_SetFavsBlobs(s_profileInfo.m_memberfavsblob, s_profileInfo.m_memberuids);
    Live_SetPrivateProfile();
}

void __cdecl CL_AddFavourite_f()
{
    const char *v0; // eax
    const char *v1; // eax
    int v2; // eax
    unsigned __int64 v3; // [esp-8h] [ebp-18h]

    v0 = Cmd_Argv(2);
    v3 = atol(v0);
    v1 = Cmd_Argv(1);
    v2 = atol(v1);
    Live_AddFavourite(v2, v3);
}

void __cdecl CL_DeleteFavourite_f()
{
    const char *v0; // eax
    int servernum; // [esp+8h] [ebp-Ch]
    int *count; // [esp+Ch] [ebp-8h] BYREF
    serverInfo_t *servers; // [esp+10h] [ebp-4h] BYREF

    v0 = Cmd_Argv(1);
    servernum = atoi(v0);
    servers = 0;
    if ( CL_GetServerList(3, &servers, &count) )
        Live_DeleteFavourite(servers[servernum].bdUserID);
}

void __cdecl CL_DumpFavourites_f()
{
    Live_DumpFavourites();
}

void __cdecl CL_NukeFavourites_f()
{
    memset(s_profileInfo.m_memberfavsblob, 0, sizeof(s_profileInfo.m_memberfavsblob));
    memset(s_profileInfo.m_memberuids, 0, sizeof(s_profileInfo.m_memberuids));
    Live_SetPrivateProfile();
}

cmd_function_s CL_GetFavourites_f_VAR;
cmd_function_s CL_SetFavourites_f_VAR;
cmd_function_s CL_AddFavourite_f_VAR;
cmd_function_s CL_DeleteFavourite_f_VAR;
cmd_function_s CL_DumpFavourites_f_VAR;
cmd_function_s CL_NukeFavourites_f_VAR;

void __cdecl Live_InitFavourites()
{
    unsigned int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < 0x29; ++i )
    {
        s_favourites[i].uid = 0;
    }

    memset(s_profileInfo.m_memberfavsblob, 0, sizeof(s_profileInfo.m_memberfavsblob));
    Cmd_AddCommandInternal("getfavourites", CL_GetFavourites_f, &CL_GetFavourites_f_VAR);
    Cmd_AddCommandInternal("setfavourites", CL_SetFavourites_f, &CL_SetFavourites_f_VAR);
    Cmd_AddCommandInternal("addfavourite", CL_AddFavourite_f, &CL_AddFavourite_f_VAR);
    Cmd_AddCommandInternal("deletefavourite", CL_DeleteFavourite_f, &CL_DeleteFavourite_f_VAR);
    Cmd_AddCommandInternal("dumpfavourites", CL_DumpFavourites_f, &CL_DumpFavourites_f_VAR);
    Cmd_AddCommandInternal("nukfavourites", CL_NukeFavourites_f, &CL_NukeFavourites_f_VAR);
}

void __cdecl Live_AddRecentPlayers(unsigned __int64 *uids, const char **names, int numIDs)
{
}

unsigned __int64 __cdecl Live_GetServerForFriend(unsigned __int64 friendId)
{
    return 0;
}

void __cdecl Live_JoinSessionInProgressComplete(TaskRecord *task)
{
}

TaskRecord *__cdecl Live_JoinSessionInProgress(unsigned __int64 uid, bool recent)
{
    return NULL;
}

void __cdecl Live_AddFriendServer(unsigned __int64 serverID, unsigned __int64 friendID)
{
}

void __cdecl Live_OnInvite(unsigned __int64 uid, bdSessionID sessionID, const char *password)
{
}

void __cdecl Live_OnRevokeInvite(unsigned __int64 uid)
{
}

bool __cdecl Live_RevokeInvite(unsigned __int64 friendID)
{
    return false;
}

char __cdecl Live_FindInviteFromFriend(unsigned __int64 friendID, bdSessionID *sessionID, char **password)
{
    return false;
}

char __cdecl Live_HandleInviteMessage(unsigned __int64 senderID, char *message)
{
    bool handled; // [esp+7h] [ebp-5h]

    handled = 0;
    if ( *message == 9 )
    {
        Live_OnRevokeInvite(senderID);
        return 1;
    }
    else
    {
        Com_PrintWarning(23, "Unknown live message %u\n", *message);
    }
    return handled;
}

void __cdecl Live_AcceptInviteAsyncFailure()
{
    Com_PrintError(23, "couldn't accept invite :(\n");
}

void __cdecl Live_JoinWagerFromInvite()
{
}

void __cdecl Live_AcceptInviteAsyncComplete(TaskRecord *task)
{
}

TaskRecord *__cdecl Live_AcceptInviteAsync(bdSessionID sessionID)
{
    return NULL;
}

void __cdecl Live_AcceptInvite(unsigned __int64 frienduid)
{
}

int __cdecl Live_GetInvitesCount()
{
    return 0;
}

int __cdecl Live_GetInviteFriend(int index)
{
    return 0;
}

void __cdecl Live_ListInvites_f()
{
}

void __cdecl Live_AddPlayerAsFriend_f()
{
    const char *v0; // eax
    __int64 xuid; // [esp+10h] [ebp-8h]

    if ( Cmd_Argc() == 2 )
    {
        v0 = Cmd_Argv(1);
        xuid = I_atoi64(v0);
        LiveSteam_PopOverlayForSteamID(xuid);
    }
    else
    {
        Com_Printf(14, "usage: xaddfriend <xuid>\n");
    }
}

bool __cdecl Live_ShouldBroadcastNewServer()
{
    return !Demo_IsPlaying();
}

void __cdecl Live_RespondToSessionRequest(unsigned __int64 from, unsigned __int8 flags)
{
}

void __cdecl Live_RequestSessionsFromFriends()
{
}

void __cdecl Live_RequestSessionsFromRecentPlayers()
{
}

void __cdecl Live_DispatchP2PMessage(unsigned __int8 *message, unsigned int messagesize, unsigned __int64 from)
{
}

void __cdecl Live_BroadcastSessionToFriends(unsigned __int64 sessionUID, unsigned __int8 flags)
{
}

void __cdecl Live_BroadcastSessionToRecentPlayers(unsigned __int64 sessionUID, unsigned __int8 flags)
{
}

void __cdecl Live_BroadcastSessionIfNeeded()
{
}

