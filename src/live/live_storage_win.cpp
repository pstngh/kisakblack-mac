#include "live_storage_win.h"

#include <windows.h>
#include <universal/assertive.h>
#include <qcommon/common.h>
#include <ddl/ddl_api.h>
#include "live_stats.h"
#include <server_mp/sv_main_pc_mp.h>
#include <game_mp/g_main_mp.h>
#include <stringed/stringed_hooks.h>
#include <qcommon/com_gamemodes.h>
#include "live_storage.h"
#include <client_mp/cl_main_pc_mp.h>
#include "live_storage_pub.h"
#include "live_presence_win.h"
#include <ui/ui_playlists.h>
#include <client_mp/sv_client_mp.h>
#include <server_mp/sv_main_mp.h>
#include <client/cl_rank.h>
#include <bgame/bg_unlockable_items.h>
#include <ctime>
#include <server/sv_live_stats.h>
#include <universal/mem_largelocal.h>
#include <win32/win_shared.h>
#include <bgame/bg_emblems.h>
#include "live_contracts.h"
#include <qcommon/md4.h>
#include <universal/com_files.h>
#include "live_win.h"
#include <win32/win_gamerprofile.h>

int g_svLastPlaylistFetch;
bool s_shouldMapRotate;
bool s_fetchingStats;

const int GlobalLbViewIds_0[4] =
{ 3000, 3001, 3002, 3003 };


const int lbViewIds_0[160] =
{
  1000,
  1001,
  1002,
  1003,
  1004,
  1005,
  1006,
  1007,
  1100,
  1101,
  1102,
  1103,
  1104,
  1105,
  1106,
  1107,
  1200,
  1201,
  1202,
  1203,
  1204,
  1205,
  1206,
  1207,
  1300,
  1301,
  1302,
  1303,
  1304,
  1305,
  1306,
  1307,
  1400,
  1401,
  1402,
  1403,
  1404,
  1405,
  1406,
  1407,
  1500,
  1501,
  1502,
  1503,
  1504,
  1505,
  1506,
  1507,
  1600,
  1601,
  1602,
  1603,
  1604,
  1605,
  1606,
  1607,
  1700,
  1701,
  1702,
  1703,
  1704,
  1705,
  1706,
  1707,
  1800,
  1801,
  1802,
  1803,
  1804,
  1805,
  1806,
  1807,
  1900,
  1901,
  1902,
  1903,
  1904,
  1905,
  1906,
  1907,
  2000,
  2001,
  2002,
  2003,
  2004,
  2005,
  2006,
  2007,
  2100,
  2101,
  2102,
  2103,
  2104,
  2105,
  2106,
  2107,
  2200,
  2201,
  2202,
  2203,
  2204,
  2205,
  2206,
  2207,
  2300,
  2301,
  2302,
  2303,
  2304,
  2305,
  2306,
  2307,
  2400,
  2401,
  2402,
  2403,
  2404,
  2405,
  2406,
  2407,
  2500,
  2501,
  2502,
  2503,
  2504,
  2505,
  2506,
  2507,
  2600,
  2601,
  2602,
  2603,
  2604,
  2605,
  2606,
  2607,
  2700,
  2701,
  2702,
  2703,
  2704,
  2705,
  2706,
  2707,
  2800,
  2801,
  2802,
  2803,
  2804,
  2805,
  2806,
  2807,
  2900,
  2901,
  2902,
  2903,
  2904,
  2905,
  2906,
  2907
};

bool s_firstTimeRunning = true;

const dvar_t *stats_backup;
const dvar_t *presell;
const dvar_t *sv_playlistFetchInterval;

char g_statsDir[256];

int __cdecl SystemTimeToInt()
{
    _SYSTEMTIME systemTime; // [esp+0h] [ebp-14h] BYREF

    GetSystemTime(&systemTime);
    return (systemTime.wYear << 16) | (systemTime.wMonth << 8) | systemTime.wDay;
}

void __cdecl LiveStorage_ResetStats(unsigned __int8 *buffer)
{
    unsigned int Time; // eax
    unsigned int v2; // eax

    if ( !buffer
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp", 384, 0, "%s", "buffer") )
    {
        __debugbreak();
    }
    if ( !stat_version->current.integer )
        Com_Error(ERR_DROP, "stat_version is zero\n");
    if ( buffer )
    {
        Com_Printf(
            16,
            "LiveStorage_ResetStats: resetstats called - writing statversion %i to buffer\n",
            stat_version->current.integer);
        memset(buffer, 0, STATS_BUFFER_SIZE);
        DDL_AssociateBuffer((char *)buffer, STATS_BUFFER_SIZE, g_statsDDL);
        LiveStats_WriteChecksumToBuffer(buffer, STATS_BUFFER_SIZE);
        LiveStats_SetPlayerStatByKey(
            "PlayerStatsList",
            MP_PLAYERSTATSKEY_STATS_VERSION,
            stat_version->current.unsignedInt,
            buffer);
        Time = SV_GetTime();
        LiveStats_SetPlayerStatByKey("PlayerStatsList", MP_PLAYERSTATSKEY_WEEKLYTIMESTAMP, Time, buffer);
        v2 = SV_GetTime();
        LiveStats_SetPlayerStatByKey("PlayerStatsList", MP_PLAYERSTATSKEY_MONTHLYTIMESTAMP, v2, buffer);
    }
}

static void __cdecl SetDvarFromLocString(int controllerIndex, const char *dvarName, char *preLocalizedText)
{
    char *localizedText; // [esp+0h] [ebp-4h]

    if (!Dvar_IsValidName(dvarName)
        && !Assert_MyHandler(
            "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp",
            414,
            0,
            "%s",
            "Dvar_IsValidName( dvarName )"))
    {
        __debugbreak();
    }
    localizedText = SEH_LocalizeTextMessage(preLocalizedText, "dvar string", LOCMSG_NOERR);
    if (localizedText && *localizedText)
        Dvar_SetCommand(dvarName, localizedText);
    else
        Dvar_SetCommand(dvarName, preLocalizedText);
}

static void __cdecl ResetCreateAClassNames(int controllerIndex)
{
    SetDvarFromLocString(controllerIndex, "customclass1",   (char*)"CLASS_SLOT1_CAPS");
    SetDvarFromLocString(controllerIndex, "customclass2",   (char*)"CLASS_SLOT2_CAPS");
    SetDvarFromLocString(controllerIndex, "customclass3",   (char*)"CLASS_SLOT3_CAPS");
    SetDvarFromLocString(controllerIndex, "customclass4",   (char*)"CLASS_SLOT4_CAPS");
    SetDvarFromLocString(controllerIndex, "customclass5",   (char*)"CLASS_SLOT5_CAPS");
    SetDvarFromLocString(controllerIndex, "prestigeclass1", (char*)"CLASS_PRESTIGE1");
    SetDvarFromLocString(controllerIndex, "prestigeclass2", (char*)"CLASS_PRESTIGE2");
    SetDvarFromLocString(controllerIndex, "prestigeclass3", (char*)"CLASS_PRESTIGE3");
    SetDvarFromLocString(controllerIndex, "prestigeclass4", (char*)"CLASS_PRESTIGE4");
    SetDvarFromLocString(controllerIndex, "prestigeclass5", (char*)"CLASS_PRESTIGE5");
}

void __cdecl LiveStorage_ReadStats(int __formal, bool validate, bool silent)
{
}

void LiveStorage_InitCustomClassesNames()
{
    int allUnitialized; // [esp+0h] [ebp-8h]
    int i; // [esp+4h] [ebp-4h]

    if ( !customclass
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp", 448, 0, "%s", "customclass") )
    {
        __debugbreak();
    }
    allUnitialized = 1;
    for ( i = 0; i < 10; ++i )
    {
        if ( !customclass[i]
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp",
                        453,
                        0,
                        "%s",
                        "customclass[i]") )
        {
            __debugbreak();
        }
        if ( *customclass[i]->current.string )
            allUnitialized = 0;
    }
    if ( allUnitialized )
        ResetCreateAClassNames(0);
}

void __cdecl LiveStorage_ReadStatsIfDirChanged()
{
    if ( !G_ExitAfterToolComplete() )
    {
        // commented out for no effect
        //if (I_stricmp(g_statsDir, fs_gameDirVar->current.string))
        //{
        //    //BLOPS_NULLSUB();
        //}
    }
}

void __cdecl LiveStorage_UploadStats()
{
    if ( !G_ExitAfterToolComplete() )
    {
        if ( xblive_basictraining->current.enabled )
            LiveStats_MakeStableStatsBuffer(0);
    }
}

void __cdecl LiveStorage_UploadStatsForController()
{
    LiveStorage_UploadStats();
}

void __cdecl CL_GetXP_f()
{
    int xp; // [esp+0h] [ebp-4h] BYREF

    xp = 0;
    LiveStats_GetIntPlayerStatByKey(0, &xp, MP_PLAYERSTATSKEY_RANKXP);
    Com_Printf(14, "clientside xp is %i\n", xp);
}

cmd_function_s CL_GetXP_f_VAR;
void __cdecl LiveStorage_Init_Platform()
{
    stats_backup = _Dvar_RegisterBool("stats_backup", 1, 1u, "Backup stats file every time the stats file is saved");
    collectors = _Dvar_RegisterBool("collectors", 0, 0x40u, "Set to true if the player has the collector's edition");
    presell = _Dvar_RegisterBool("presell", 0, 0x40u, "Set to true if the player has preordered");
    primaryWeaponOffset = _Dvar_RegisterInt(
                                                    "primaryWeaponOffset",
                                                    0,
                                                    0,
                                                    7,
                                                    0x40u,
                                                    "Primary Weapon Offset for CE and Presell");
    sv_playlistFetchInterval = _Dvar_RegisterInt(
                                                             "sv_playlistFetchInterval",
                                                             3600,
                                                             600,
                                                             0x7FFFFFFF,
                                                             0,
                                                             "Interval in seconds between playlist fetches");
    g_statsDir[0] = 0;
    Cmd_AddCommandInternal("getxp", CL_GetXP_f, &CL_GetXP_f_VAR);
}

void __cdecl LiveStorage_FetchPlaylistsLocal(int controllerIndex)
{
    if ( Playlist_ReadFromDisk() )
    {
        LiveStorage_SetHavePlaylists(1);
        Live_SetPlaylistVersion(controllerIndex);
        Playlist_ValidatePlaylistNum();
    }
}

bool __cdecl SV_GetStatFromBlob(char *buffer, const char *stat, int *outInt)
{
    ddlState_t searchState; // [esp+4h] [ebp-14h] BYREF
    bool retval; // [esp+17h] [ebp-1h]

    if ( !stat
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp", 1373, 0, "%s", "stat") )
    {
        __debugbreak();
    }
    if ( !buffer
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp", 1374, 0, "%s", "buffer") )
    {
        __debugbreak();
    }
    searchState = *LiveStats_GetRootDDLState();
    retval = 0;
    if ( stat )
    {
        if ( !DDL_MoveToName(&searchState, &searchState, "PlayerStatsList")
            || !DDL_MoveToName(&searchState, &searchState, stat) )
        {
            Com_PrintWarning(15, "getdstat: Could not find member name.");
            return 0;
        }
        if ( searchState.member->type == 2 )
        {
            *outInt = DDL_GetInt(&searchState, buffer);
            return 1;
        }
        else
        {
            Com_PrintWarning(15, "called SV_GetClientDDLStat and got a non-int stat. Ask Ewan");
        }
    }
    else
    {
        Com_Printf(15, "SV_GetClientDDLStat called with null stat, you're doing it wrong. Ask Ewan\n");
    }
    return retval;
}

bool __cdecl SV_GetIntClientStatByGameMode(
                unsigned int clientNum,
                int *outInt,
                const char *gameMode,
                const char *statName)
{
    const ddlState_t *RootDDLState; // eax
    const char *path[3]; // [esp+0h] [ebp-20h] BYREF
    ddlState_t searchState; // [esp+Ch] [ebp-14h] BYREF
    bool retval; // [esp+1Fh] [ebp-1h]

    if ( clientNum >= 0x20
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp",
                    1416,
                    0,
                    "%s",
                    "clientNum >= 0 && clientNum < MAX_CLIENTS") )
    {
        __debugbreak();
    }
    if ( (!outInt || !gameMode || !statName)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp",
                    1417,
                    0,
                    "%s",
                    "outInt && gameMode && statName") )
    {
        __debugbreak();
    }
    retval = 0;
    if ( (clientNum & 0x80000000) == 0 && outInt && gameMode && statName )
    {
        path[0] = "PlayerStatsByGameMode";
        path[1] = gameMode;
        path[2] = statName;
        RootDDLState = LiveStats_GetRootDDLState();
        if ( !DDL_MoveToPath(RootDDLState, &searchState, 3, path) )
        {
            Com_PrintError(15, "DDL:Error getting player stat value for %s.\n", statName);
            return 0;
        }
        Com_DPrintf(15, "Attempting to get stat %s|%s for clientnum %i\n", path[1], path[2], clientNum);
        *outInt = SV_GetClientDIntStat(clientNum, &searchState);
        return 1;
    }
    return retval;
}

bool __cdecl SV_GetClientDDLStat(unsigned int clientnum, const char *stat, int *outInt)
{
    ddlState_t searchState; // [esp+4h] [ebp-14h] BYREF
    bool retval; // [esp+17h] [ebp-1h]

    if ( !stat
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp", 1330, 0, "%s", "stat") )
    {
        __debugbreak();
    }
    if ( clientnum > 0x20
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp",
                    1331,
                    0,
                    "%s",
                    "clientnum >= 0 && clientnum <= MAX_CLIENTS") )
    {
        __debugbreak();
    }
    searchState = *LiveStats_GetRootDDLState();
    retval = 0;
    if ( stat )
    {
        if ( !DDL_MoveToName(&searchState, &searchState, "PlayerStatsList")
            || !DDL_MoveToName(&searchState, &searchState, stat) )
        {
            Com_PrintWarning(15, "getdstat: Could not find member name.");
            return 0;
        }
        if ( searchState.member->type == 2 )
        {
            *outInt = SV_GetClientDIntStat(clientnum, &searchState);
            return 1;
        }
        else
        {
            Com_PrintWarning(15, "called SV_GetClientDDLStat and got a non-int stat. Ask Ewan");
        }
    }
    else
    {
        Com_Printf(15, "SV_GetClientDDLStat called with null stat, you're doing it wrong. Ask Ewan\n");
    }
    return retval;
}

int __cdecl LiveStorage_GetMatchesPlayed(const char *gameModePrefix, int resetPeriod)
{
    const ddlState_t *RootDDLState; // eax
    persistentStats *StatsBuffer; // eax
    char *v5; // [esp-4h] [ebp-28h]
    ddlState_t localState; // [esp+0h] [ebp-24h] BYREF
    int retval; // [esp+10h] [ebp-14h]
    char period[12]; // [esp+14h] [ebp-10h] BYREF

    retval = 0;
    memset(period, 0, 10);
    if ( resetPeriod == 1 )
        I_strncpyz(period, "lbWeekly", 10);
    else
        I_strncpyz(period, "lbMonthly", 10);
    v5 = va("%d", 0);
    RootDDLState = LiveStats_GetRootDDLState();
    if ( DDL_MoveTo(RootDDLState, &localState, 4, "PlayerStatsByGameMode", gameModePrefix, period, v5) )
    {
        StatsBuffer = LiveStorage_GetStatsBuffer(0, STATS_LOCATION_GLOBAL, 1);
        return DDL_GetInt(&localState, (char *)StatsBuffer);
    }
    return retval;
}

char __cdecl SV_GetIntClientStatMatchDeltaByGameMode(
                int clientNum,
                int *outInt,
                const char *gameMode,
                const char *statName)
{
    char *liveStatsBuffer; // [esp+4h] [ebp-10h]
    int valueBeforeMatch; // [esp+Ch] [ebp-8h] BYREF
    int valueAfterMatch; // [esp+10h] [ebp-4h] BYREF

    liveStatsBuffer = (char *)svs.clients[clientNum].globalStats;
    if ( LiveStats_GetIntPlayerStatByGameModeFromBase(
                 0,
                 &valueBeforeMatch,
                 gameMode,
                 statName,
                 (char *)svs.clients[clientNum].globalStatsStable)
        && LiveStats_GetIntPlayerStatByGameModeFromBase(0, &valueAfterMatch, gameMode, statName, liveStatsBuffer) )
    {
        *outInt = valueAfterMatch - valueBeforeMatch;
        return 1;
    }
    else
    {
        DDL_PrintError("DDL:Error getting player stat value for %s.\n", statName);
        return 0;
    }
}

int __cdecl SV_GetTotalMatchesPlayedByGameModeForClient(unsigned int clientNum, const char *gameModePrefix)
{
    int losses; // [esp+0h] [ebp-Ch] BYREF
    int wins; // [esp+4h] [ebp-8h] BYREF
    int ties; // [esp+8h] [ebp-4h] BYREF

    SV_GetIntClientStatByGameMode(clientNum, &wins, gameModePrefix, "WINS");
    SV_GetIntClientStatByGameMode(clientNum, &losses, gameModePrefix, "LOSSES");
    SV_GetIntClientStatByGameMode(clientNum, &ties, gameModePrefix, "TIES");
    return ties + losses + wins;
}


void __cdecl SV_SetPlaylistFetchedTime()
{
    g_svLastPlaylistFetch = Sys_Milliseconds();
}

int __cdecl SV_GetPlaylistFetchedTime()
{
    return g_svLastPlaylistFetch;
}

void __cdecl SV_SetShouldMapRotate(bool should)
{
    s_shouldMapRotate = should;
}

bool __cdecl SV_ShouldMapRotate()
{
    return s_shouldMapRotate;
}

bool __cdecl LiveStorage_FirstTimeRunning()
{
    return s_firstTimeRunning;
}

void __cdecl LiveStorage_SetFirstTimeRunning(bool running)
{
    s_firstTimeRunning = running;
}

