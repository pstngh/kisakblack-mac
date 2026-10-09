#include "live_storage.h"
#include <qcommon/com_gamemodes.h>
#include "live_stats.h"
#include "live_storage_pub.h"
#include <win32/win_shared.h>
#include <qcommon/com_clients.h>
#include <universal/mem_largelocal.h>
#include <server_mp/sv_main_pc_mp.h>
#include <time.h>
#include "live_win.h"
#include "live_combatrecord.h"
#include <client_mp/cl_main_pc_mp.h>
#include "live_storage_win.h"
#include "live_contracts.h"
#include <ui_mp/ui_main_mp.h>
#include <qcommon/threads.h>

cmd_function_s LiveStorage_RestoreStatsFromBackupCmd_VAR;

const dvar_t *stat_version;
const dvar_t *stats_version_check;
const dvar_t *waitOnStatsTimeout;
const dvar_t *basicTrainingFatal;

persistentStats s_otherPlayerStats;
playerNetworkData controllerNetworkData[1];

persistentStats *__cdecl LiveStorage_GetStatsBuffer(
                int controllerIndex,
                statsLocation playerStatsLocation,
                bool verifyLocation)
{
    return LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, verifyLocation);
}

persistentStats *__cdecl LiveStorage_GetPersStatsBuffer(
                int controllerIndex,
                statsLocation playerStatsLocation,
                bool verifyLocation)
{
    persistentStats *stats = NULL; // [esp+4h] [ebp-4h]

    if ( playerStatsLocation >= STATS_LOCATION_COUNT
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    317,
                    0,
                    "%s",
                    "playerStatsLocation < STATS_LOCATION_COUNT") )
    {
        __debugbreak();
    }
    if ( playerStatsLocation < STATS_LOCATION_NORMAL
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    318,
                    0,
                    "%s",
                    "playerStatsLocation >= STATS_LOCATION_NORMAL") )
    {
        __debugbreak();
    }
    switch ( playerStatsLocation )
    {
        case STATS_LOCATION_NORMAL:
        case STATS_LOCATION_FORCE_NORMAL:
            stats = (persistentStats *)&controllerNetworkData[controllerIndex];
            break;
        case STATS_LOCATION_BACKUP:
            stats = (persistentStats *)controllerNetworkData[controllerIndex].playerStatsBackup;
            break;
        case STATS_LOCATION_STABLE:
            stats = (persistentStats *)controllerNetworkData[controllerIndex].stableStatsBuffer;
            break;
        case STATS_LOCATION_OTHERPLAYER:
            stats = &s_otherPlayerStats;
            break;
        case STATS_LOCATION_BASICTRAINING:
            stats = (persistentStats *)controllerNetworkData[controllerIndex].basicTrainingStats;
            break;
        case STATS_LOCATION_GLOBAL:
            stats = (persistentStats *)controllerNetworkData[controllerIndex].globalplayerStats;
            break;
        case STATS_LOCATION_GLOBALSTABLE:
            stats = (persistentStats *)controllerNetworkData[controllerIndex].globalStablePlayerStats;
            break;
        default:
            break;
    }
    if ( (playerStatsLocation == STATS_LOCATION_NORMAL || playerStatsLocation == STATS_LOCATION_GLOBAL)
        && xblive_basictraining->current.enabled )
    {
        stats = (persistentStats *)controllerNetworkData[controllerIndex].basicTrainingStats;
    }
    if ( verifyLocation )
        LiveStorage_VerifyCorrectStats(stats, playerStatsLocation);
    return stats;
}

void __cdecl LiveStorage_VerifyCorrectStats(persistentStats *stats, statsLocation location)
{
    char *v2; // eax
    char *v3; // eax
    bool onlineRanked; // [esp+2h] [ebp-2h]
    bool basicTraining; // [esp+3h] [ebp-1h]

    basicTraining = LiveStats_GetBasicTrainingState(stats->statsBuffer);
    onlineRanked = LiveStats_GetOnlineRankedState(stats->statsBuffer);
    if ( location != STATS_LOCATION_STABLE && location != STATS_LOCATION_BACKUP && location != STATS_LOCATION_OTHERPLAYER )
    {
        if ( location == STATS_LOCATION_FORCE_NORMAL )
        {
            if ( basicTraining )
                LiveStorage_CorrectStatsError((char*)"Basic Training Stats found, expecting Online Ranked stats.\n");
        }
        else if ( location == STATS_LOCATION_BASICTRAINING )
        {
            if ( onlineRanked )
                LiveStorage_CorrectStatsError((char *)"Online Ranked Stats found, expecting Basic Training stats.\n");
        }
        else
        {
            if ( xblive_basictraining->current.enabled && onlineRanked )
            {
                v2 = va("Online Ranked Stats used during a Basic Training game. Stats location: %d.\n", location);
                LiveStorage_CorrectStatsError(v2);
            }
            if ( !xblive_basictraining->current.enabled && basicTraining )
            {
                v3 = va("Basic Training Stats used during a non-Basic Training game. Stats location: %d.\n", location);
                LiveStorage_CorrectStatsError(v3);
            }
        }
    }
}

void __cdecl LiveStorage_CorrectStatsError(char *msg)
{
    if ( basicTrainingFatal->current.enabled )
    {
        if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 241, 0, "%s\n\t%s", "0", msg) )
            __debugbreak();
        Com_Error(ERR_DROP, msg);
    }
    else
    {
        Com_PrintError(16, msg);
    }
}

int __cdecl LiveStorage_GetStatsBufferSize()
{
    return STATS_BUFFER_SIZE;
}

unsigned __int8 __cdecl LiveStorage_GetStatsChecksumValid(int controllerIndex, statsLocation playerStatsLocation)
{
    return LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 1)->isChecksumValid;
}

void __cdecl LiveStorage_SetStatsChecksumValid(int controllerIndex, statsLocation playerStatsLocation, bool isValid)
{
    LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 1)->isChecksumValid = isValid;
}

bool __cdecl LiveStorage_GetStatsWriteNeeded(int controllerIndex, statsLocation location)
{
    persistentStats *StatsBuffer; // eax

    StatsBuffer = LiveStorage_GetStatsBuffer(controllerIndex, location, 1);
    return DDL_GetUserFlag((char *)StatsBuffer, 0);
}

void __cdecl LiveStorage_SetStatsWriteNeeded(int controllerIndex, bool isWriteNeeded, statsLocation location)
{
    persistentStats *StatsBuffer; // eax

    StatsBuffer = LiveStorage_GetStatsBuffer(controllerIndex, location, 1);
    DDL_SetUserFlag((char *)StatsBuffer, 0, isWriteNeeded);
}

int __cdecl LiveStorage_ValidateWithDDL(int controllerIndex, statsLocation location)
{
    char backupBuffer[sizeof(persistentStats)]; // [esp+0h] [ebp-9CF8h] BYREF
    char *buffer; // [esp+9CF0h] [ebp-8h]
    int bufferSize; // [esp+9CF4h] [ebp-4h]

    if ( !g_statsDDL
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 403, 0, "%s", "g_statsDDL") )
    {
        __debugbreak();
    }
    buffer = (char *)LiveStorage_GetStatsBuffer(controllerIndex, location, 1);
    bufferSize = LiveStorage_GetStatsBufferSize();
    if ( DDL_AssociateBuffer(buffer, bufferSize, g_statsDDL) )
    {
        LiveStorage_SetStatsDDLValidated(controllerIndex, location, 1);
        return 1;
    }
    else if ( DDL_FixBufferVersion(buffer, g_statsDDL, "ddl_mp/stats.ddl", backupBuffer, STATS_BUFFER_SIZE)
                 || DDL_FixBufferVersion(buffer, g_statsDDL, "ddl_mp/stats_archive.ddl", backupBuffer, STATS_BUFFER_SIZE) )
    {
        DDL_NoCheckPrintWarning(
            "DDL: Stats buffer updated to version %d for controller index %d.\n",
            g_statsDDL->version,
            controllerIndex);
        LiveStorage_SetStatsDDLValidated(controllerIndex, location, 1);
        return 1;
    }
    else
    {
        return 0;
    }
}

unsigned __int8 __cdecl LiveStorage_AreStatsDDLValidated(int controllerIndex, statsLocation playerStatsLocation)
{
    return LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 1)->statsValidatedWithDDL;
}

void __cdecl LiveStorage_SetStatsDDLValidated(
                int controllerIndex,
                statsLocation playerStatsLocation,
                bool statsValidatedWithDDL)
{
    LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 1)->statsValidatedWithDDL = statsValidatedWithDDL;
}

unsigned __int8 __cdecl LiveStorage_DoWeHaveStats(int controllerIndex, statsLocation playerStatsLocation)
{
    return LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 0)->statsFetched;
}

unsigned __int8 __cdecl LiveStorage_DoWeHaveCurrentStats(int controllerIndex)
{
    return LiveStorage_GetPersStatsBuffer(controllerIndex, STATS_LOCATION_NORMAL, 0)->statsFetched;
}

bool __cdecl LiveStorage_DoWeHaveAllStats(int controllerIndex)
{
    unsigned __int8 playerStatsFetched; // [esp+6h] [ebp-2h]
    unsigned __int8 basicTrainingStatsFetched; // [esp+7h] [ebp-1h]

    playerStatsFetched = LiveStorage_DoWeHaveStats(controllerIndex, STATS_LOCATION_FORCE_NORMAL);
    basicTrainingStatsFetched = LiveStorage_DoWeHaveStats(controllerIndex, STATS_LOCATION_BASICTRAINING);
    return playerStatsFetched && basicTrainingStatsFetched;
}

void __cdecl LiveStorage_SetStatsFetched(int localControllerIndex, statsLocation playerStatsLocation, bool isFetched)
{
    LiveStorage_GetPersStatsBuffer(localControllerIndex, playerStatsLocation, 0)->statsFetched = isFetched;
}

void __cdecl LiveStorage_RestoreStatsFromBackup(int localControllerIndex)
{
    persistentStats *v1; // eax
    persistentStats *StatsBuffer; // [esp-8h] [ebp-8h]
    unsigned int StatsBufferSize; // [esp-4h] [ebp-4h]

    StatsBufferSize = LiveStorage_GetStatsBufferSize();
    StatsBuffer = LiveStorage_GetStatsBuffer(localControllerIndex, STATS_LOCATION_BACKUP, 1);
    v1 = LiveStorage_GetStatsBuffer(localControllerIndex, STATS_LOCATION_FORCE_NORMAL, 0);
    memcpy(v1->statsBuffer, StatsBuffer->statsBuffer, StatsBufferSize);
}

__int64 __cdecl LiveStorage_GetUTC()
{
    return _time64(0);
}

int __cdecl LiveStorage_GetUTCOffset()
{
    return 0;
}

// There is no time server to sync against; the local clock is all we have.
bool __cdecl LiveStorage_IsTimeSynced()
{
    return false;
}

void __cdecl LiveStorage_SetAllStatsNotFetched(int controllerIndex)
{
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 0);
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_BACKUP, 0);
    LiveStats_SetBufferInitialised(controllerIndex, 0);
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_BASICTRAINING, 0);
}

void __cdecl LiveStorage_NewUser(int controllerIndex)
{
    LiveStorage_ClearPlayerStats(controllerIndex);
    LiveStorage_SetAllStatsNotFetched(controllerIndex);
}

void __cdecl LiveStorage_ClearPlayerStats(int controllerIndex)
{
    if ( controllerIndex
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    4212,
                    0,
                    "%s\n\t(controllerIndex) = %i",
                    "(controllerIndex >= 0 && controllerIndex < 1)",
                    controllerIndex) )
    {
        __debugbreak();
    }
    memset(controllerNetworkData[controllerIndex].playerStats, 0, sizeof(playerNetworkData));
}

char __cdecl LiveStorage_Init()
{
    int i; // [esp+0h] [ebp-8h]

    if ( !live_service->current.enabled )
        return 0;

    LiveStats_Init();
    LiveCombatRecord_Init();

    for ( i = 0; i < 1; ++i )
        LiveStorage_ClearPlayerStats(i);

    Cmd_AddCommandInternal(
        "restoreStatsFromBackup",
        LiveStorage_RestoreStatsFromBackupCmd,
        &LiveStorage_RestoreStatsFromBackupCmd_VAR);

    stat_version = _Dvar_RegisterInt("stat_version", 10, 0, 0x7FFFFFFF, 0, "Stats version number");
    stats_version_check = _Dvar_RegisterBool("stats_version_check", 1, 0, "Reset stats if version numbers do not match");
    waitOnStatsTimeout = _Dvar_RegisterInt(
                                                 "waitOnStatsTimeout",
                                                 15,
                                                 0,
                                                 0x7FFFFFFF,
                                                 0,
                                                 "Time in seconds to wait for stats to be fetched while dev mapping.");
    basicTrainingFatal = _Dvar_RegisterBool(
                                                 "basicTrainingFatal",
                                                 1,
                                                 0,
                                                 "If true, a basic training stats error will cause the game to end, if false a warning is printed"
                                                 " to the console and the game continues");
    LiveStorage_Init_Platform();
    return 1;
}

void __cdecl LiveStorage_RestoreStatsFromBackupCmd()
{
    LiveStorage_RestoreStatsFromBackup(0);
}

