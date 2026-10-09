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
#include <universal/com_fileaccess.h>
#include <ddl/ddl_buffer.h>
#include <bgame/bg_weapons_attachment.h>
#include <qcommon/net_chan_mp.h>
#include <client/client.h>
#include <cstdio>

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

// Offline stats. The online service kept a player's stats in three user files:
// the client-owned "mpstatsCompressed" (custom classes, purchases), the
// server-owned "globalstatsCompressed" (rank, XP, currency, challenges) and
// "mpstatsBasicTraining". Offline they are one file in players/, read when the
// player signs in and written by LiveStorage_UploadStats. A ranked server
// validated the custom classes before it saved them, charging the player for what
// they bought (SV_ValidateClientCAC); that is done here too.
//
// A listen server takes the host's stats when the host connects
// (LiveStorage_GetHostStats); from then on its copy of the global stats is the
// newer one and every change reaches the host as the N command. Purchases made
// meanwhile are only previewed and are charged once the host has left the match.

#define OFFLINE_STATS_FILENAME "mpstats.dat"
#define OFFLINE_STATS_MAGIC 0x5453424B // "KBST"
#define OFFLINE_STATS_VERSION 1

enum offlineStatsBuffer_t : __int32
{
    OFFLINE_STATS_CAC           = 0x0,
    OFFLINE_STATS_GLOBAL        = 0x1,
    OFFLINE_STATS_BASICTRAINING = 0x2,
    OFFLINE_STATS_COUNT         = 0x3,
};

struct offlineStatsFile_t
{
    unsigned int magic;
    int version;
    int bufferSize;
    int bufferCount;
    unsigned __int8 buffers[OFFLINE_STATS_COUNT][STATS_BUFFER_SIZE];
};

static offlineStatsFile_t s_offlineStatsFile;
// The custom classes as last validated: what the online service would hand back.
static unsigned __int8 s_validatedCAC[STATS_BUFFER_SIZE];
static unsigned __int8 s_previewCAC[STATS_BUFFER_SIZE];
static unsigned __int8 s_previewGlobal[STATS_BUFFER_SIZE];

static unsigned __int8 *LiveStorage_GetOfflineStatsBuffer(int controllerIndex, int index)
{
    // Not LiveStorage_GetStatsBuffer: basic training redirects the normal and global locations.
    switch ( index )
    {
        case OFFLINE_STATS_CAC:
            return controllerNetworkData[controllerIndex].playerStats;
        case OFFLINE_STATS_GLOBAL:
            return controllerNetworkData[controllerIndex].globalplayerStats;
        default:
            return controllerNetworkData[controllerIndex].basicTrainingStats;
    }
}

static void LiveStorage_GetOfflineStatsPath(const char *suffix, char *ospath)
{
    FS_BuildOSPath((char *)fs_homepath->current.string, (char *)"players", va("%s%s", OFFLINE_STATS_FILENAME, suffix), ospath);
}

static bool LiveStorage_ReadOfflineStatsFile(const char *ospath)
{
    FILE *f; // [esp+0h] [ebp-Ch]
    int fileSize; // [esp+4h] [ebp-8h]
    int i; // [esp+8h] [ebp-4h]

    f = FS_FileOpenReadBinary(ospath);
    if ( !f )
        return 0;
    fileSize = FS_FileGetFileSize(f);
    if ( fileSize != sizeof(s_offlineStatsFile)
        || FS_FileRead(&s_offlineStatsFile, sizeof(s_offlineStatsFile), f) != sizeof(s_offlineStatsFile) )
    {
        FS_FileClose(f);
        Com_PrintError(16, "%s is %d bytes, expected %d\n", ospath, fileSize, (int)sizeof(s_offlineStatsFile));
        return 0;
    }
    FS_FileClose(f);
    if ( s_offlineStatsFile.magic != OFFLINE_STATS_MAGIC
        || s_offlineStatsFile.version != OFFLINE_STATS_VERSION
        || s_offlineStatsFile.bufferSize != STATS_BUFFER_SIZE
        || s_offlineStatsFile.bufferCount != OFFLINE_STATS_COUNT )
    {
        Com_PrintError(16, "%s is not a stats file of this version\n", ospath);
        return 0;
    }
    for ( i = 0; i < OFFLINE_STATS_COUNT; ++i )
    {
        if ( !LiveStats_ValidateChecksum(s_offlineStatsFile.buffers[i], STATS_BUFFER_SIZE - 4) )
        {
            Com_PrintError(
                16,
                "%s: stats had the wrong checksum (%u)\n",
                ospath,
                LiveStats_ChecksumGamerStats(&s_offlineStatsFile.buffers[i][4], STATS_BUFFER_SIZE - 4));
            return 0;
        }
    }
    return 1;
}

static bool LiveStorage_WriteOfflineStats(int controllerIndex, const unsigned __int8 *cacBuffer)
{
    char ospath[256]; // [esp+0h] [ebp-310h] BYREF
    char tempPath[256]; // [esp+100h] [ebp-210h] BYREF
    char backupPath[256]; // [esp+200h] [ebp-110h] BYREF
    FILE *f; // [esp+300h] [ebp-10h]
    unsigned int written; // [esp+304h] [ebp-Ch]
    int i; // [esp+308h] [ebp-8h]

    s_offlineStatsFile.magic = OFFLINE_STATS_MAGIC;
    s_offlineStatsFile.version = OFFLINE_STATS_VERSION;
    s_offlineStatsFile.bufferSize = STATS_BUFFER_SIZE;
    s_offlineStatsFile.bufferCount = OFFLINE_STATS_COUNT;
    for ( i = 0; i < OFFLINE_STATS_COUNT; ++i )
    {
        if ( i == OFFLINE_STATS_CAC )
            memcpy(s_offlineStatsFile.buffers[i], cacBuffer, STATS_BUFFER_SIZE);
        else
            memcpy(s_offlineStatsFile.buffers[i], LiveStorage_GetOfflineStatsBuffer(controllerIndex, i), STATS_BUFFER_SIZE);
        DDL_SetUserFlag((char *)s_offlineStatsFile.buffers[i], 0, 0);
        LiveStats_WriteChecksumToBuffer(s_offlineStatsFile.buffers[i], STATS_BUFFER_SIZE);
    }
    LiveStorage_GetOfflineStatsPath("", ospath);
    LiveStorage_GetOfflineStatsPath(".tmp", tempPath);
    LiveStorage_GetOfflineStatsPath(".bak", backupPath);
    if ( FS_CreatePath(tempPath) || (f = FS_FileOpenWriteBinary(tempPath)) == 0 )
    {
        Com_PrintError(16, "Couldn't write %s\n", tempPath);
        return 0;
    }
    written = FS_FileWrite(&s_offlineStatsFile, sizeof(s_offlineStatsFile), f);
    FS_FileClose(f);
    if ( written != sizeof(s_offlineStatsFile) )
    {
        Com_PrintError(16, "Couldn't write %s\n", tempPath);
        FS_Remove(tempPath);
        return 0;
    }
    // The previous save stays as the backup the next sign-in falls back to.
    FS_Remove(backupPath);
    rename(ospath, backupPath);
    if ( rename(tempPath, ospath) )
    {
        Com_PrintError(16, "Couldn't rename %s to %s\n", tempPath, ospath);
        return 0;
    }
    DDL_SetUserFlag((char *)LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_GLOBAL), 0, 0);
    DDL_SetUserFlag((char *)LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_BASICTRAINING), 0, 0);
    if ( cacBuffer == LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_CAC) )
        DDL_SetUserFlag((char *)cacBuffer, 0, 0);
    Com_Printf(16, "Stats saved to %s\n", ospath);
    return 1;
}

static void LiveStorage_ResetGlobalStats(int controllerIndex)
{
    unsigned __int8 *buffer; // [esp+0h] [ebp-4h]

    buffer = LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_GLOBAL);
    LiveStorage_ResetStats(buffer);
    DDL_SetUserFlag((char *)buffer, 0, 1);
}

static void LiveStorage_ReadPlayerStatsSuccessful(int controllerIndex)
{
    persistentStats *buffer; // [esp+0h] [ebp-18h]
    unsigned int currentStatsVersion; // [esp+4h] [ebp-14h]
    unsigned __int64 statsXuid; // [esp+8h] [ebp-10h]
    unsigned __int64 myXuid; // [esp+10h] [ebp-8h]

    LiveStorage_SetFirstTimeRunning(0);
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 1);
    controllerNetworkData[controllerIndex].firstTimeRunning = 0;
    LiveStorage_SetStatsDDLValidated(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 0);
    LiveStorage_SetStatsChecksumValid(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 1);
    buffer = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 1);
    if ( !LiveStats_GetDDLHeaderVersion(buffer->statsBuffer)
        || !LiveStorage_ValidateWithDDL(controllerIndex, STATS_LOCATION_FORCE_NORMAL) )
    {
        Com_PrintError(16, "Stats could not be validated vs the DDL\n");
        LiveStats_ResetStats(controllerIndex, 1);
    }
    currentStatsVersion = LiveStats_ReadVersionFromBuffer((char *)buffer);
    if ( currentStatsVersion == stat_version->current.integer )
    {
        LiveContracts_CLMergeBuffers(
            buffer->statsBuffer,
            LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_GLOBAL));
    }
    else
    {
        Com_PrintError(
            16,
            "Stats had a version of %d but we expected a version of %d\n",
            currentStatsVersion,
            stat_version->current.integer);
        if ( stats_version_check->current.enabled )
        {
            // LiveStorage_DeleteGlobalStats: every buffer starts over.
            LiveStats_ResetStats(controllerIndex, 1);
            LiveStorage_ResetGlobalStats(controllerIndex);
            LiveStats_ResetBasicTrainingStats(controllerIndex);
        }
    }
    myXuid = Live_GetXuid(controllerIndex);
    statsXuid = LiveStats_ReadXUIDFromStats();
    if ( statsXuid != myXuid )
        Com_PrintError(
            16,
            "Stats were saved with an XUID of 0x%llx and the current user has XUID of 0x%llx\n",
            statsXuid,
            myXuid);
}

static void LiveStorage_ReadGlobalStatsSuccessful(int controllerIndex)
{
    if ( !LiveStats_ValidateGlobalWithDDL(controllerIndex) )
    {
        Com_PrintError(16, "Global stats could not be validated vs the DDL\n");
        LiveStorage_ResetGlobalStats(controllerIndex);
    }
}

static void LiveStorage_ReadBasicTrainingStatsSuccessful(int controllerIndex)
{
    persistentStats *buffer; // [esp+0h] [ebp-8h]
    int currentStatsVersion; // [esp+4h] [ebp-4h] BYREF

    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
    LiveStorage_SetStatsDDLValidated(controllerIndex, STATS_LOCATION_BASICTRAINING, 0);
    LiveStorage_SetStatsChecksumValid(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
    buffer = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
    if ( !LiveStats_GetDDLHeaderVersion(buffer->statsBuffer)
        || !LiveStorage_ValidateWithDDL(controllerIndex, STATS_LOCATION_BASICTRAINING) )
    {
        Com_PrintError(16, "Basic Training Stats could not be validated vs the DDL\n");
        LiveStats_ResetBasicTrainingStats(controllerIndex);
    }
    currentStatsVersion = 0;
    LiveStats_GetIntPlayerStatFromBase(&currentStatsVersion, "STATS_VERSION", (char *)buffer);
    if ( currentStatsVersion != stat_version->current.integer )
    {
        Com_PrintError(
            16,
            "Basic Training Stats had a version of %d but we expected a version of %d\n",
            currentStatsVersion,
            stat_version->current.integer);
        if ( stats_version_check->current.enabled )
            LiveStats_ResetBasicTrainingStats(controllerIndex);
    }
}

// The player stats (rank, XP, prestige, currency) are the server's: the client
// reads its copy of them from the custom class buffer.
static void LiveStorage_CopyPlayerStatsList(unsigned __int8 *cacBuffer, unsigned __int8 *globalBuffer)
{
    ddlState_t searchState; // [esp+0h] [ebp-20h] BYREF
    unsigned int oldValue; // [esp+10h] [ebp-10h] BYREF
    unsigned int value; // [esp+14h] [ebp-Ch] BYREF
    int bitSize; // [esp+18h] [ebp-8h]
    int bit; // [esp+1Ch] [ebp-4h]

    if ( !DDL_MoveTo(LiveStats_GetRootDDLState(), &searchState, 1, "PlayerStatsList") )
        return;
    for ( bit = 0; bit < searchState.member->size; bit += bitSize )
    {
        bitSize = searchState.member->size - bit;
        if ( bitSize > 32 )
            bitSize = 32;
        DDL_Buffer_ReadBits((char *)globalBuffer, searchState.absoluteOffset + bit, bitSize, &value);
        DDL_Buffer_ReadBits((char *)cacBuffer, searchState.absoluteOffset + bit, bitSize, &oldValue);
        if ( value != oldValue )
        {
            DDL_Buffer_WriteBits((char *)cacBuffer, searchState.absoluteOffset + bit, bitSize, value);
            DDL_SetValueChanged((char *)cacBuffer);
        }
    }
}

static void LiveStorage_SetPlayerStatIfChanged(unsigned __int8 *buffer, const char *statName, int value)
{
    int oldValue; // [esp+0h] [ebp-4h] BYREF

    oldValue = 0;
    if ( !LiveStats_GetIntPlayerStatFromBase(&oldValue, statName, (char *)buffer) || oldValue != value )
        LiveStats_SetPlayerStat("PlayerStatsList", statName, (char *)buffer, value);
}

// With everything unlocked (allItemsUnlocked, the default) the player has the top
// rank and prestige whatever they earned, so prestige mode, which resets the
// custom classes, never comes up. Their classes use the pro perks they own, as
// buying one does. The stats file then only needs what the player set up (classes,
// attachments, camos, emblem, class names).
static void LiveStorage_SetTopRank(int controllerIndex)
{
    unsigned __int8 *globalBuffer; // [esp+0h] [ebp-8h]
    int maxXP; // [esp+4h] [ebp-4h]

    if ( !BG_UnlockablesAllItemsUnlocked() )
        return;
    globalBuffer = LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_GLOBAL);
    maxXP = CL_GetMaxXP();
    LiveStorage_SetPlayerStatIfChanged(globalBuffer, "RANKXP", maxXP);
    LiveStorage_SetPlayerStatIfChanged(globalBuffer, "RANK", CL_GetRankForXp(maxXP));
    LiveStorage_SetPlayerStatIfChanged(globalBuffer, "PLEVEL", CL_GetMaxPrestige());
    LiveStorage_CopyPlayerStatsList(LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_CAC), globalBuffer);
    BG_ReplaceItemsWithPurchasedProItem(controllerIndex);
}

// CL_CACValidateRequest_f didn't ask for a validation while either was set: the
// classes hold items the stats never recorded as bought.
static bool LiveStorage_ShouldValidateCAC()
{
    return !Dvar_GetBool("allItemsUnlocked") && !Dvar_GetBool("allItemsPurchased");
}

void __cdecl LiveStorage_ReadStats(int __formal, bool validate, bool silent)
{
    char ospath[256]; // [esp+0h] [ebp-208h] BYREF
    char backupPath[256]; // [esp+100h] [ebp-108h] BYREF
    bool wasOnlineGame; // [esp+200h] [ebp-Ch]
    bool wasBasicTraining; // [esp+201h] [ebp-Bh]
    int i; // [esp+204h] [ebp-4h]

    if ( !live_service->current.enabled )
        return;
    // Resetting the basic training stats plays a basic training game, which is an
    // online one (ValidateGameModes); the online service read them when online.
    wasOnlineGame = onlinegame->current.enabled;
    Dvar_SetBool((dvar_s *)onlinegame, 1);
    wasBasicTraining = xblive_basictraining->current.enabled;
    Dvar_SetBool((dvar_s *)xblive_basictraining, 0);
    LiveStorage_GetOfflineStatsPath("", ospath);
    LiveStorage_GetOfflineStatsPath(".bak", backupPath);
    if ( LiveStorage_ReadOfflineStatsFile(ospath) || LiveStorage_ReadOfflineStatsFile(backupPath) )
    {
        Com_Printf(16, "Stats read from %s\n", ospath);
        for ( i = 0; i < OFFLINE_STATS_COUNT; ++i )
            memcpy(LiveStorage_GetOfflineStatsBuffer(0, i), s_offlineStatsFile.buffers[i], STATS_BUFFER_SIZE);
        LiveStorage_ReadGlobalStatsSuccessful(0);
        LiveStorage_ReadPlayerStatsSuccessful(0);
        LiveStorage_ReadBasicTrainingStatsSuccessful(0);
        // The file keeps the custom classes as validated, with the player stats of then.
        LiveStorage_CopyPlayerStatsList(
            LiveStorage_GetOfflineStatsBuffer(0, OFFLINE_STATS_CAC),
            LiveStorage_GetOfflineStatsBuffer(0, OFFLINE_STATS_GLOBAL));
    }
    else
    {
        if ( FS_OSFPathExists(ospath) )
        {
            // LiveStorage_BackupCorruptedStats
            FS_Remove(va("%s.bad", ospath));
            rename(ospath, va("%s.bad", ospath));
            Com_PrintError(16, "Couldn't read %s, it was renamed to %s.bad\n", ospath, ospath);
        }
        Com_Printf(16, "No %s file found, creating a new one\n", OFFLINE_STATS_FILENAME);
        LiveStorage_SetFirstTimeRunning(0);
        LiveStorage_SetStatsFetched(0, STATS_LOCATION_FORCE_NORMAL, 1);
        LiveStorage_SetStatsDDLValidated(0, STATS_LOCATION_FORCE_NORMAL, 0);
        LiveStats_ResetStats(0, 0);
        LiveStorage_ResetGlobalStats(0);
        LiveStorage_SetStatsFetched(0, STATS_LOCATION_BASICTRAINING, 1);
        LiveStorage_SetStatsDDLValidated(0, STATS_LOCATION_BASICTRAINING, 0);
        LiveStats_ResetBasicTrainingStats(0);
    }
    LiveStorage_SetTopRank(0);
    LiveStats_MakeStableStatsBuffer(0);
    LiveStats_MakeStableGlobalStatsBuffer(0);
    memcpy(s_validatedCAC, LiveStorage_GetOfflineStatsBuffer(0, OFFLINE_STATS_CAC), STATS_BUFFER_SIZE);
    Dvar_SetBool((dvar_s *)xblive_basictraining, wasBasicTraining);
    Dvar_SetBool((dvar_s *)onlinegame, wasOnlineGame);
    for ( i = 0; i < OFFLINE_STATS_COUNT; ++i )
    {
        if ( DDL_GetUserFlag((char *)LiveStorage_GetOfflineStatsBuffer(0, i), 0) )
        {
            LiveStorage_WriteOfflineStats(0, s_validatedCAC);
            break;
        }
    }
    LiveStorage_InitCustomClassesNames();
}

// While a match runs, the player stats in the custom class buffer follow the
// server's changes to the global ones (the N command) by the same amounts: they
// show what was earned and keep what was spent meanwhile (LiveStats_SpendCurrency).
static unsigned int s_playerStatsBeforeChange[512];
static int s_playerStatsChangeCount;

void __cdecl LiveStorage_BeginGlobalStatsChange(int controllerIndex)
{
    ddlState_t searchState; // [esp+0h] [ebp-18h] BYREF
    unsigned __int8 *globalBuffer; // [esp+10h] [ebp-8h]
    int i; // [esp+14h] [ebp-4h]

    s_playerStatsChangeCount = 0;
    if ( xblive_basictraining->current.enabled
        || !DDL_MoveTo(LiveStats_GetRootDDLState(), &searchState, 1, "PlayerStatsList")
        || searchState.member->arraySize > ARRAY_COUNT(s_playerStatsBeforeChange) )
    {
        return;
    }
    globalBuffer = LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_GLOBAL);
    s_playerStatsChangeCount = searchState.member->arraySize;
    for ( i = 0; i < s_playerStatsChangeCount; ++i )
    {
        DDL_Buffer_ReadBits(
            (char *)globalBuffer,
            searchState.absoluteOffset + i * (searchState.member->size / searchState.member->arraySize),
            searchState.member->size / searchState.member->arraySize,
            &s_playerStatsBeforeChange[i]);
    }
}

void __cdecl LiveStorage_EndGlobalStatsChange(int controllerIndex)
{
    ddlState_t searchState; // [esp+0h] [ebp-24h] BYREF
    unsigned __int8 *cacBuffer; // [esp+10h] [ebp-14h]
    unsigned __int8 *globalBuffer; // [esp+14h] [ebp-10h]
    unsigned int newValue; // [esp+18h] [ebp-Ch] BYREF
    unsigned int value; // [esp+1Ch] [ebp-8h] BYREF
    int bitSize; // [esp+20h] [ebp-4h]
    int i; // [esp+24h] [ebp-0h]

    if ( !s_playerStatsChangeCount || !DDL_MoveTo(LiveStats_GetRootDDLState(), &searchState, 1, "PlayerStatsList") )
        return;
    cacBuffer = LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_CAC);
    globalBuffer = LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_GLOBAL);
    bitSize = searchState.member->size / searchState.member->arraySize;
    for ( i = 0; i < s_playerStatsChangeCount; ++i )
    {
        DDL_Buffer_ReadBits((char *)globalBuffer, searchState.absoluteOffset + i * bitSize, bitSize, &newValue);
        if ( newValue != s_playerStatsBeforeChange[i] )
        {
            DDL_Buffer_ReadBits((char *)cacBuffer, searchState.absoluteOffset + i * bitSize, bitSize, &value);
            DDL_Buffer_WriteBits(
                (char *)cacBuffer,
                searchState.absoluteOffset + i * bitSize,
                bitSize,
                value + newValue - s_playerStatsBeforeChange[i]);
            DDL_SetValueChanged((char *)cacBuffer);
        }
    }
    s_playerStatsChangeCount = 0;
}

// A listen server holds the host's global stats from the moment the host
// connects to it until the host leaves.
static bool LiveStorage_IsInLocalMatch()
{
    return com_sv_running->current.enabled && CL_GetLocalClientConnectionState(0) >= CA_CONNECTED;
}

static void LiveStorage_CommitOfflineStats(int controllerIndex, bool matchOver)
{
    unsigned __int8 *cacBuffer; // [esp+0h] [ebp-Ch]
    unsigned __int8 *globalBuffer; // [esp+4h] [ebp-8h]
    int i; // [esp+8h] [ebp-4h]

    if ( !LiveStorage_DoWeHaveAllStats(controllerIndex) )
        return;
    cacBuffer = LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_CAC);
    globalBuffer = LiveStorage_GetOfflineStatsBuffer(controllerIndex, OFFLINE_STATS_GLOBAL);
    if ( xblive_basictraining->current.enabled )
    {
        // Basic training keeps everything in its own buffer, saved as it is.
    }
    else if ( matchOver )
    {
        // CL_CACValidateRequest_f: a ranked server validated the custom classes and
        // charged the purchases to the global stats, or rejected them.
        if ( LiveStorage_ShouldValidateCAC()
            && !SV_ValidateClientCAC(
                  s_validatedCAC,
                  STATS_BUFFER_SIZE,
                  cacBuffer,
                  STATS_BUFFER_SIZE,
                  globalBuffer,
                  STATS_BUFFER_SIZE,
                  Live_GetXuid(controllerIndex)) )
        {
            Com_PrintError(16, "CACValidate REJECTED\n");
            memcpy(cacBuffer, s_validatedCAC, STATS_BUFFER_SIZE);
            DDL_SetValueChanged((char *)cacBuffer);
        }
        LiveStorage_CopyPlayerStatsList(cacBuffer, globalBuffer);
        LiveContracts_CLMergeBuffers(cacBuffer, globalBuffer);
        memcpy(s_validatedCAC, cacBuffer, STATS_BUFFER_SIZE);
    }
    else if ( !LiveStorage_ShouldValidateCAC() )
    {
        // Nothing to charge: the classes are valid as they are, even mid-match.
        memcpy(s_validatedCAC, cacBuffer, STATS_BUFFER_SIZE);
    }
    else
    {
        // The local server has the global stats; show the player what they would
        // have once the purchases are charged.
        memcpy(s_previewCAC, cacBuffer, STATS_BUFFER_SIZE);
        memcpy(s_previewGlobal, globalBuffer, STATS_BUFFER_SIZE);
        if ( SV_ValidateClientCAC(
                 s_validatedCAC,
                 STATS_BUFFER_SIZE,
                 s_previewCAC,
                 STATS_BUFFER_SIZE,
                 s_previewGlobal,
                 STATS_BUFFER_SIZE,
                 Live_GetXuid(controllerIndex)) )
        {
            LiveStorage_CopyPlayerStatsList(cacBuffer, s_previewGlobal);
            // Nothing to charge (a changed class, no purchase): the classes are valid now.
            if ( !memcmp(s_previewGlobal, globalBuffer, STATS_BUFFER_SIZE) )
                memcpy(s_validatedCAC, cacBuffer, STATS_BUFFER_SIZE);
        }
    }
    for ( i = 0; i < OFFLINE_STATS_COUNT; ++i )
    {
        if ( DDL_GetUserFlag((char *)LiveStorage_GetOfflineStatsBuffer(controllerIndex, i), 0) )
        {
            LiveStorage_WriteOfflineStats(controllerIndex, matchOver ? cacBuffer : s_validatedCAC);
            return;
        }
    }
}

void __cdecl LiveStorage_UploadStatsAfterMatch(int controllerIndex)
{
    if ( !G_ExitAfterToolComplete() )
        LiveStorage_CommitOfflineStats(controllerIndex, 1);
}

bool __cdecl LiveStorage_GetHostStats(unsigned __int8 *cacBuffer, unsigned __int8 *globalBuffer)
{
    // SV_DWReadClientCAC and SV_DWReadClientStats read them from the online service.
    if ( !LiveStorage_DoWeHaveAllStats(0) )
        return 0;
    if ( xblive_basictraining->current.enabled )
    {
        memcpy(cacBuffer, LiveStorage_GetOfflineStatsBuffer(0, OFFLINE_STATS_BASICTRAINING), STATS_BUFFER_SIZE);
        memcpy(globalBuffer, LiveStorage_GetOfflineStatsBuffer(0, OFFLINE_STATS_BASICTRAINING), STATS_BUFFER_SIZE);
    }
    else
    {
        memcpy(cacBuffer, LiveStorage_GetOfflineStatsBuffer(0, OFFLINE_STATS_CAC), STATS_BUFFER_SIZE);
        memcpy(globalBuffer, LiveStorage_GetOfflineStatsBuffer(0, OFFLINE_STATS_GLOBAL), STATS_BUFFER_SIZE);
    }
    return 1;
}

unsigned __int8 *__cdecl LiveStorage_GetHostCACBuffer()
{
    return LiveStorage_GetOfflineStatsBuffer(0, OFFLINE_STATS_CAC);
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
        LiveStorage_CommitOfflineStats(0, !LiveStorage_IsInLocalMatch());
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
    // The global stats are the server's, updated during a match (the N command).
    Com_Printf(
        14,
        "clientside xp is %i, global xp %i (rank %i, prestige %i), codpoints %i\n",
        xp,
        LiveStats_GetXp(0),
        LiveStats_GetRank(0) + 1,
        LiveStats_GetPrestige(0),
        LiveStats_GetCurrency(0));
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
    // The online service's clock dated new stats (SV_GetTime); the local one does now.
    SV_SetTime((int)LiveStorage_GetUTC());
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

char __cdecl SV_CACValidate_SetIntStat(unsigned __int8 *buffer, const char *stat, unsigned int value)
{
    const ddlState_t *RootDDLState; // eax
    ddlState_t searchState; // [esp+0h] [ebp-10h] BYREF

    if ( (!buffer || !stat)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp",
                    2618,
                    0,
                    "%s",
                    "buffer && stat") )
    {
        __debugbreak();
    }
    RootDDLState = LiveStats_GetRootDDLState();
    if ( DDL_MoveTo(RootDDLState, &searchState, 2, "PlayerStatsList", stat) )
    {
        DDL_SetInt(&searchState, value, (char *)buffer);
        return 1;
    }
    else
    {
        DDL_PrintError("DDL: Error could not find item %s\n", stat);
        return 0;
    }
}

void __cdecl SV_PrestigeReset(char *liveStatsBuffer)
{
    const ddlState_t *RootDDLState; // eax
    const ddlState_t *v2; // eax
    const ddlState_t *v3; // eax
    const ddlState_t *v4; // eax
    char *v5; // [esp-8h] [ebp-3Ch]
    ddlState_t searchStateGroupStats; // [esp+0h] [ebp-34h] BYREF
    unsigned int itemNumber; // [esp+10h] [ebp-24h]
    ddlState_t searchStateChallenges; // [esp+14h] [ebp-20h] BYREF
    ddlState_t searchStateStats; // [esp+24h] [ebp-10h] BYREF

    for ( itemNumber = 0; itemNumber < 0x100; ++itemNumber )
    {
        if ( BG_UnlockablesIsItemValidNotNull(itemNumber) )
        {
            v5 = va("%d", itemNumber);
            RootDDLState = LiveStats_GetRootDDLState();
            if ( DDL_MoveTo(RootDDLState, &searchStateStats, 3, "ItemStats", v5, "stats")
                && DDL_IterateFirst(&searchStateStats, &searchStateStats) )
            {
                do
                {
                    LiveStats_SetItemStat(&searchStateStats, "challengeTier", liveStatsBuffer, 0);
                    LiveStats_SetItemStat(&searchStateStats, "challengeValue", liveStatsBuffer, 0);
                }
                while ( DDL_IterateNext(&searchStateStats, &searchStateStats) );
            }
            SV_CACValidate_ClearWeaponInfo(liveStatsBuffer, itemNumber);
        }
    }
    v2 = LiveStats_GetRootDDLState();
    if ( DDL_MoveTo(v2, &searchStateStats, 1, "GroupStats") && DDL_IterateFirst(&searchStateStats, &searchStateStats) )
    {
        do
        {
            if ( DDL_MoveTo(&searchStateStats, &searchStateGroupStats, 1, "stats")
                && DDL_IterateFirst(&searchStateGroupStats, &searchStateGroupStats) )
            {
                do
                {
                    LiveStats_SetItemStat(&searchStateGroupStats, "challengeTier", liveStatsBuffer, 0);
                    LiveStats_SetItemStat(&searchStateGroupStats, "challengeValue", liveStatsBuffer, 0);
                }
                while ( DDL_IterateNext(&searchStateGroupStats, &searchStateGroupStats) );
            }
        }
        while ( DDL_IterateNext(&searchStateStats, &searchStateStats) );
    }
    v3 = LiveStats_GetRootDDLState();
    if ( DDL_MoveTo(v3, &searchStateChallenges, 1, "ChallengeTier") )
    {
        v4 = LiveStats_GetRootDDLState();
        if ( DDL_MoveTo(v4, &searchStateStats, 1, "ChallengeValue") )
        {
            if ( DDL_IterateFirst(&searchStateChallenges, &searchStateChallenges)
                && DDL_IterateFirst(&searchStateStats, &searchStateStats) )
            {
                do
                {
                    DDL_SetInt(&searchStateChallenges, 0, liveStatsBuffer);
                    DDL_SetInt(&searchStateStats, 0, liveStatsBuffer);
                }
                while ( DDL_IterateNext(&searchStateStats, &searchStateStats)
                         && DDL_IterateNext(&searchStateChallenges, &searchStateChallenges) );
            }
        }
    }
    LiveStats_ResetGamemodeChallenges(liveStatsBuffer);
}

void __cdecl SV_CACValidate_EvaluateStatsBlobs(
                bool *oldcacok,
                bool *globalok,
                char *oldcacblob,
                char *globalblob,
                int oldcacsize,
                int globalsize)
{
    char dst[sizeof(persistentStats)]; // [esp+0h] [ebp-9CF0h] BYREF

    if ( (!globalok || !oldcacok)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage_win.cpp",
                    2637,
                    0,
                    "%s",
                    "globalok && oldcacok") )
    {
        __debugbreak();
    }
    *globalok = 0;
    *oldcacok = 0;
    if ( globalsize > 0 )
    {
        if ( DDL_AssociateBuffer(globalblob, STATS_BUFFER_SIZE, g_statsDDL) )
        {
            *globalok = 1;
        }
        else
        {
            memset(dst, 0, STATS_BUFFER_SIZE);
            if ( DDL_FixBufferVersion(globalblob, g_statsDDL, "ddl_mp/stats.ddl", dst, STATS_BUFFER_SIZE)
                || DDL_FixBufferVersion(globalblob, g_statsDDL, "ddl_mp/stats_archive.ddl", dst, STATS_BUFFER_SIZE) )
            {
                DDL_NoCheckPrintWarning("CACValidate: Globalbuffer updated to version %d\n", g_statsDDL->version);
                *globalok = 1;
            }
        }
    }
    if ( oldcacsize > 0 )
    {
        if ( DDL_AssociateBuffer(oldcacblob, STATS_BUFFER_SIZE, g_statsDDL) )
        {
            *oldcacok = 1;
        }
        else
        {
            memset(dst, 0, STATS_BUFFER_SIZE);
            if ( DDL_FixBufferVersion(oldcacblob, g_statsDDL, "ddl_mp/stats.ddl", dst, STATS_BUFFER_SIZE)
                || DDL_FixBufferVersion(oldcacblob, g_statsDDL, "ddl_mp/stats_archive.ddl", dst, STATS_BUFFER_SIZE) )
            {
                DDL_NoCheckPrintWarning("CACValidate: Oldcacbuffer updated to version %d\n", g_statsDDL->version);
                *oldcacok = 1;
            }
        }
    }
}

bool __cdecl SV_ValidateClientCAC(
                unsigned __int8 *oldcacblob,
                int oldcacblobsize,
                unsigned __int8 *newcacblob,
                int newcacblobsize,
                unsigned __int8 *globalblob,
                int globalblobsize,
                unsigned __int64 clientUID)
{
    char *v7; // eax
    const char *v8; // eax
    char *v9; // eax
    const char *v10; // eax
    int LayerCost; // eax
    const char *ClanTagFeature; // eax
    int v14; // eax
    const char *ItemName; // eax
    char *v16; // eax
    const char *v17; // eax
    const char *v18; // eax
    const char *v19; // eax
    int v20; // eax
    int ItemAttachmentCost; // eax
    int WeaponOptionCost; // eax
    char *ContractName; // eax
    char *v24; // eax
    char *v25; // eax
    const char *v26; // eax
    int v27; // eax
    char *v28; // eax
    char *v29; // eax
    const char *v30; // eax
    char *v31; // eax
    const char *v32; // eax
    char *v33; // eax
    const char *v34; // eax
    int v35; // [esp-4h] [ebp-A0h]
    int v36; // [esp-4h] [ebp-A0h]
    int ItemCost; // [esp-4h] [ebp-A0h]
    int v38; // [esp-4h] [ebp-A0h]
    int v39; // [esp-4h] [ebp-A0h]
    signed int PurchasedEmblemLayers; // [esp+0h] [ebp-9Ch]
    int numpurchased; // [esp+4h] [ebp-98h] BYREF
    int contractcost; // [esp+8h] [ebp-94h]
    int contractsIdx; // [esp+Ch] [ebp-90h]
    int unlockRank; // [esp+10h] [ebp-8Ch]
    int cp_spent; // [esp+14h] [ebp-88h] BYREF
    int oldindex; // [esp+18h] [ebp-84h]
    int newindex; // [esp+1Ch] [ebp-80h]
    int activeindex; // [esp+20h] [ebp-7Ch]
    int optionIdx; // [esp+24h] [ebp-78h]
    int attachNum; // [esp+28h] [ebp-74h]
    int weaponoptioncount; // [esp+2Ch] [ebp-70h]
    unsigned int itemNumber; // [esp+30h] [ebp-6Ch]
    unsigned int index; // [esp+34h] [ebp-68h]
    int k; // [esp+38h] [ebp-64h]
    int j; // [esp+3Ch] [ebp-60h]
    int i; // [esp+40h] [ebp-5Ch]
    int maxPrestige; // [esp+44h] [ebp-58h]
    int maxXP; // [esp+48h] [ebp-54h]
    int timeStarted; // [esp+4Ch] [ebp-50h]
    int startemblem; // [esp+50h] [ebp-4Ch]
    int endemblem; // [esp+54h] [ebp-48h]
    int newPrestige; // [esp+58h] [ebp-44h] BYREF
    int validateTime; // [esp+5Ch] [ebp-40h]
    int clientRank; // [esp+60h] [ebp-3Ch]
    int oldbalance; // [esp+64h] [ebp-38h] BYREF
    char from[20]; // [esp+68h] [ebp-34h] BYREF
    int clientPrestige; // [esp+80h] [ebp-1Ch] BYREF
    bool globalok; // [esp+86h] [ebp-16h] BYREF
    bool oldcacok; // [esp+87h] [ebp-15h] BYREF
    int clientXP; // [esp+88h] [ebp-14h] BYREF
    int newmoneyspent; // [esp+8Ch] [ebp-10h]
    bool ok; // [esp+93h] [ebp-9h]
    int newbalance; // [esp+94h] [ebp-8h]
    int oldmoneyspent; // [esp+98h] [ebp-4h] BYREF

    // The original ran on a ranked server (CAC_FETCHTWO: both blobs fetched); offline
    // the client validates against its own global stats (LiveStorage_CommitOfflineStats).
    ok = 1;
    globalok = 0;
    oldcacok = 0;
    clientRank = 0;
    clientPrestige = 0;
    clientXP = 0;
    oldbalance = 0;
    newbalance = 0;
    oldmoneyspent = 0;
    newmoneyspent = 0;
    memset(from, 0, 17);
    XUIDToString(&clientUID, from);
    SV_CACValidate_EvaluateStatsBlobs(
        &oldcacok,
        &globalok,
        (char *)oldcacblob,
        (char *)globalblob,
        oldcacblobsize,
        globalblobsize);
    if ( globalok )
    {
        LiveStats_GetIntPlayerStatFromBase(&clientXP, "RANKXP", (char *)globalblob);
        LiveStats_GetIntPlayerStatFromBase(&clientPrestige, "PLEVEL", (char *)globalblob);
        clientRank = CL_GetRankForXp(clientXP);
        LiveStats_GetIntPlayerStatFromBase(&oldbalance, "CODPOINTS", (char *)globalblob);
        LiveStats_GetIntPlayerStatFromBase(&oldmoneyspent, "CURRENCYSPENT", (char *)globalblob);
    }
    if ( DDL_AssociateBuffer((char *)newcacblob, newcacblobsize, g_statsDDL) )
    {
        timeStarted = Sys_Milliseconds();
        if ( !SV_CACValidate_CheckIfEquippedItemsArePurchased(newcacblob, -1, -1) )
        {
            v9 = va("rejected cac from %s, items equipped that are not purchased!\n", from);
            Com_DPrintf(15, v9);
            v10 = va("rejected cac from %s, items equipped that are not purchased!\n", from);
            SV_SysLog_LogMessage(0, v10);
            return 0;
        }
        newPrestige = 0;
        LiveStats_GetIntPlayerStatFromBase(&newPrestige, "PLEVEL", (char *)newcacblob);
        if ( newPrestige > clientPrestige )
        {
            maxXP = CL_GetMaxXP();
            maxPrestige = CL_GetMaxPrestige();
            Com_DPrintf(15, "Client claims to have gone up in prestige level!\n");
            if ( newPrestige - clientPrestige <= 1 && clientXP >= maxXP && newPrestige <= maxPrestige )
            {
                Com_DPrintf(15, "Setting client prestige to %i from %i\n", newPrestige, clientPrestige);
                SV_PrestigeReset((char *)globalblob);
                SV_PrestigeReset((char *)newcacblob);
                SV_CACValidate_SetIntStat(globalblob, "RANKXP", 0);
                SV_CACValidate_SetIntStat(globalblob, "RANK", 0);
                SV_CACValidate_SetIntStat(globalblob, "CODPOINTS", 0);
                SV_CACValidate_SetIntStat(globalblob, "PLEVEL", newPrestige);
                return 1;
            }
            else
            {
                Com_DPrintf(
                    15,
                    "Treason uncloaked! User claims to have gone up more than one prestige level, or has insufficient xp, or has ex"
                    "ceeded max prestige!\n");
                SV_SysLog_LogMessage(
                    0,
                    "Treason uncloaked! User claims to have gone up more than one prestige level, or has insufficient xp, or has ex"
                    "ceeded max prestige!\n");
                return 0;
            }
        }
        if ( oldcacok )
            PurchasedEmblemLayers = SV_CACValidate_GetPurchasedEmblemLayers(oldcacblob);
        else
            PurchasedEmblemLayers = 0;
        startemblem = PurchasedEmblemLayers;
        endemblem = SV_CACValidate_GetPurchasedEmblemLayers(newcacblob);
        if ( endemblem > PurchasedEmblemLayers )
        {
            for ( i = startemblem; i < endemblem; ++i )
            {
                LayerCost = BG_EmblemsGetLayerCost(i);
                newmoneyspent += LayerCost;
            }
        }
        for ( j = 0; j < BG_EmblemsGetTotalBackgroundCount(); ++j )
        {
            if ( SV_CACValidate_IsBackgroundPurchased(newcacblob, clientRank, j)
                && (!oldcacok || !SV_CACValidate_IsBackgroundPurchased(oldcacblob, clientRank, j)) )
            {
                newmoneyspent += BG_EmblemsGetBackgroundCost(j);
            }
        }
        for ( k = 0; k < BG_EmblemsGetIconCount(); ++k )
        {
            if ( SV_CACValidate_IsIconPurchased(newcacblob, clientRank, k)
                && (!oldcacok || !SV_CACValidate_IsIconPurchased(oldcacblob, clientRank, k)) )
            {
                newmoneyspent += BG_EmblemsGetIconCost(k);
            }
        }
        for ( index = 0; (int)index < BG_UnlockablesGetClanTagFeatureCount(); ++index )
        {
            if ( SV_CACValidate_IsClanTagFeaturePurchased(newcacblob, index, clientPrestige, clientRank)
                && (!oldcacok || !SV_CACValidate_IsClanTagFeaturePurchased(oldcacblob, index, clientPrestige, clientRank)) )
            {
                ClanTagFeature = BG_UnlockablesGetClanTagFeature(index, CLANTAG_COL_COST);
                v14 = atoi(ClanTagFeature);
                newmoneyspent += v14;
            }
        }
        for ( itemNumber = 0; itemNumber < 0x100; ++itemNumber )
        {
            if ( BG_UnlockablesIsItemValidNotNull(itemNumber)
                && SV_CacValidate_IsItemPurchased(newcacblob, itemNumber, clientPrestige, clientRank) )
            {
                if ( BG_UnlockablesIsItemLockedForRank(clientPrestige, clientRank, itemNumber) )
                {
                    v35 = clientRank;
                    ItemName = BG_UnlockablesGetItemName(itemNumber);
                    v16 = va("Rejecting cac from client %s, item %s is locked for rank %i\n", from, ItemName, v35);
                    Com_DPrintf(15, v16);
                    v36 = clientRank;
                    v17 = BG_UnlockablesGetItemName(itemNumber);
                    v18 = va("Rejecting cac from client %s, item %s is locked for rank %i\n", from, v17, v36);
                    SV_SysLog_LogMessage(0, v18);
                    ok = 0;
                    break;
                }
                if ( !oldcacok || !SV_CacValidate_IsItemPurchased(oldcacblob, itemNumber, clientPrestige, clientRank) )
                {
                    ItemCost = BG_UnlockablesGetItemCost(itemNumber);
                    v19 = BG_UnlockablesGetItemName(itemNumber);
                    Com_DPrintf(15, "CACValidate: New item bought: %s, cost %i\n", v19, ItemCost);
                    v20 = BG_UnlockablesGetItemCost(itemNumber);
                    newmoneyspent += v20;
                }
                for ( attachNum = 0; attachNum < SV_GetMaxAttachCount(); ++attachNum )
                {
                    if ( SV_CACValidate_IsItemAttachmentPurchased(newcacblob, itemNumber, attachNum, clientPrestige, clientRank)
                        && (!oldcacok
                         || !SV_CACValidate_IsItemAttachmentPurchased(oldcacblob, itemNumber, attachNum, clientPrestige, clientRank)) )
                    {
                        ItemAttachmentCost = BG_UnlockablesGetItemAttachmentCost(itemNumber, attachNum);
                        newmoneyspent += ItemAttachmentCost;
                    }
                }
                weaponoptioncount = BG_GetWeaponOptionCount();
                for ( optionIdx = 0; optionIdx < weaponoptioncount; ++optionIdx )
                {
                    if ( SV_CACValidate_IsItemOptionPurchased(newcacblob, itemNumber, optionIdx, clientPrestige, clientRank)
                        && (!oldcacok
                         || !SV_CACValidate_IsItemOptionPurchased(oldcacblob, itemNumber, optionIdx, clientPrestige, clientRank)) )
                    {
                        WeaponOptionCost = BG_GetWeaponOptionCost(optionIdx);
                        newmoneyspent += WeaponOptionCost;
                    }
                }
            }
        }
        for ( activeindex = 0; activeindex < 3; ++activeindex )
        {
            oldindex = -1;
            newindex = -1;
            if ( oldcacok )
                oldindex = SV_CACValidate_GetIndexForActiveContract(oldcacblob, activeindex);
            newindex = SV_CACValidate_GetIndexForActiveContract(newcacblob, activeindex);
            if ( oldindex != newindex && newindex != -1 )
            {
                unlockRank = LiveContracts_GetUnlockLevel(newindex);
                contractsIdx = BG_UnlockablesGetItemIndexFromName("FEATURE_CONTRACTS");
                if ( unlockRank > clientRank || BG_UnlockablesIsItemLockedForRank(clientPrestige, clientRank, contractsIdx) )
                {
                    v38 = clientRank;
                    ContractName = LiveContracts_GetContractName(newindex);
                    v24 = va(
                                    "CACValidate: Rejecting cac from client %s, contract %s is locked for rank %i\n",
                                    from,
                                    ContractName,
                                    v38);
                    Com_DPrintf(15, v24);
                    v39 = clientRank;
                    v25 = LiveContracts_GetContractName(newindex);
                    v26 = va("CACValidate: Rejecting cac from client %s, contract %s is locked for rank %i\n", from, v25, v39);
                    SV_SysLog_LogMessage(0, v26);
                    ok = 0;
                    break;
                }
                cp_spent = 0;
                numpurchased = 0;
                SV_GetStatFromBlob((char *)globalblob, "CONTRACTS_CP_SPENT", &cp_spent);
                v27 = LiveContracts_GetContractCost(newindex);
                SV_CACValidate_SetIntStat(globalblob, "CONTRACTS_CP_SPENT", cp_spent + v27);
                SV_GetStatFromBlob((char *)globalblob, "CONTRACTS_PURCHASED", &numpurchased);
                SV_CACValidate_SetIntStat(globalblob, "CONTRACTS_PURCHASED", ++numpurchased);
                v28 = LiveContracts_GetContractName(newindex);
                Com_DPrintf(15, "CACValidate: New contract: %s\n", v28);
                contractcost = LiveContracts_GetContractCost(newindex);
                newmoneyspent += contractcost;
                Com_DPrintf(15, "CACValidate: New contract: cost %i\n", contractcost);
            }
        }
        validateTime = Sys_Milliseconds() - timeStarted;
        if ( validateTime > 10 )
        {
            v29 = va("Validate took %ims\n", validateTime);
            Com_PrintWarning(15, v29);
            v30 = va("Validate took %ims\n", validateTime);
            SV_SysLog_LogMessage(1, v30);
        }
    }
    else
    {
        v7 = va("Rejecting cac from client %s, couldn't associate new buffer\n", from);
        Com_DPrintf(15, v7);
        v8 = va("Rejecting cac from client %s, couldn't associate new buffer\n", from);
        SV_SysLog_LogMessage(0, v8);
        ok = 0;
    }
    if ( oldbalance >= newmoneyspent )
    {
        if ( newmoneyspent > 0 )
        {
            newbalance = oldbalance - newmoneyspent;
            oldmoneyspent += newmoneyspent;
            if ( !SV_CACValidate_SetIntStat(globalblob, "CODPOINTS", oldbalance - newmoneyspent)
                || !SV_CACValidate_SetIntStat(globalblob, "CURRENCYSPENT", oldmoneyspent) )
            {
                v33 = va("Rejecting cac from client %s, couldn't set currency stats\n", from);
                Com_DPrintf(15, v33);
                v34 = va("Rejecting cac from client %s, couldn't set currency stats\n", from);
                SV_SysLog_LogMessage(0, v34);
                return 0;
            }
        }
    }
    else
    {
        v31 = va(
                        "CACValidate: Rejecting cac from client %s, money spent (%i) exceeds balance (%i)\n",
                        from,
                        newmoneyspent,
                        oldbalance);
        Com_DPrintf(15, v31);
        v32 = va(
                        "CACValidate: Rejecting cac from client %s, money spent (%i) exceeds balance (%i)\n",
                        from,
                        newmoneyspent,
                        oldbalance);
        SV_SysLog_LogMessage(0, v32);
        return 0;
    }
    return ok;
}
