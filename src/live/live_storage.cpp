#include "live_storage.h"
#include <qcommon/com_gamemodes.h>
#include "live_stats.h"
#include "live_storage_pub.h"
#include <universal/com_tasks.h>
#include <win32/win_shared.h>
#include <qcommon/com_clients.h>
#include <universal/mem_largelocal.h>
#include <server_mp/sv_main_pc_mp.h>
#include <time.h>
#include "live_meetplayer.h"
#include "live_win.h"
#include "live_combatrecord.h"
#include <client_mp/cl_main_pc_mp.h>
#include "live_storage_win.h"
#include "live_contracts.h"
#include "live_fileshare.h"
#include "live_fileshare_cache.h"
#include "live_fileshare_search.h"
#include <DW/MatchRecorder.h>
#include <ui_mp/ui_main_mp.h>
#include "live_counter.h"
#include <qcommon/threads.h>

cmd_function_s LiveStorage_FakeComErrorCmd_VAR;
cmd_function_s LiveStorage_ReadStatsBackupCmd_VAR;
cmd_function_s LiveStorage_GetFriendStatsCmd_VAR;
cmd_function_s LiveStorage_WriteBackupStatsCmd_VAR;
cmd_function_s LiveStorage_RestoreStatsFromBackupCmd_VAR;
cmd_function_s LiveStorage_AssertDWConnectionCmd_VAR;
cmd_function_s LiveStorage_FileShare_DownloadFile_f_VAR;
cmd_function_s LiveStorage_ListCustomGameTypesForUser_f_VAR;
cmd_function_s LiveStorage_RefetchOnlineWAD_VAR;
cmd_function_s LiveStorage_GeneratePopulationDataCmd_VAR;

const dvar_t *stat_version;
const dvar_t *stats_version_check;
const dvar_t *maxStatsBackupInterval;
const dvar_t *dwFileFetchTryIntervalBase;
const dvar_t *dwFileFetchTryIntervalMax;
const dvar_t *dwFileFetchTryMaxAttempts;
const dvar_t *clanMessageLastFetchTime;
const dvar_t *eventMessageLastFetchTime;
const dvar_t *codMessageLastFetchTime;
const dvar_t *waitOnStatsTimeout;
const dvar_t *heatMapLoadDelay;
const dvar_t *minDelayForOtherPlayerStatsFetch;
const dvar_t *fshSearchTaskDelay;
const dvar_t *basicTrainingFatal;

persistentStats s_otherPlayerStats;
playerNetworkData controllerNetworkData[1];
fileSharePrivateData s_fileSharePrimary;
fileSharePrivateData s_fileShareSecondary;

playerFileOperations controllerFileOps[1];

unsigned __int64 s_tempXuid;
unsigned __int64 s_XuidOfOtherPlayer;
int s_lastStatsUpdateTimeForOtherPlayer;
unsigned __int8 s_tempStatsBuffer[40168];
char s_matchRecordBinaryData[66560];

const TaskDefinition task_LiveDeleteUserFile[1] =
{
  {
    8uLL,
    "LiveDeleteUserFile",
    0,
    &LiveStorage_DeleteUserFileSuccess,
    &LiveStorage_DeleteUserFileFailure,
    NULL
  }
};

const TaskDefinition task_LiveGetUserFile[1] =
{
  {
    8uLL,
    "LiveGetUserFile",
    0,
    &LiveStorage_GetUserFileSuccess,
    &LiveStorage_GetUserFileFailure,
    NULL
  }
};

const TaskDefinition task_LiveWriteUserFile[1] =
{
  {
    8uLL,
    "LiveWriteUserFile",
    0,
    &LiveStorage_WriteUserFileSuccess,
    &LiveStorage_WriteUserFileFailure,
    NULL
  }
};

const TaskDefinition task_LiveFetchMetPlayerList[1] =
{ { 8uLL, "LiveFetchMetPlayerList", 0, NULL, NULL, NULL } };

const TaskDefinition task_LiveSaveMetPLayerList[1] =
{ { 8uLL, "LiveSaveMetPlayerList", 0, NULL, NULL, NULL } };

const TaskDefinition task_LiveGetOtherPlayerStatsData[1] =
{ { 8uLL, "LiveGetOtherPlayerStatsData", 0, NULL, NULL, NULL } };

const TaskDefinition task_LiveGetPlayerStatsData[1] =
{ { 8uLL, "LiveGetPlayerStatsData", 0, NULL, NULL, NULL } };

const TaskDefinition task_LiveSavePlayerStats[1] =
{
  {
    8uLL,
    "LiveSavePlayerStats",
    0,
    NULL,
    &LiveStorage_WritePlayerStatsFailure,
    NULL
  }
};

const TaskDefinition task_LiveGetBasicTrainingStatsData[1] =
{ { 8uLL, "LiveGetBasicTrainingStatsData", 0, NULL, NULL, NULL } };

const TaskDefinition task_LiveSaveBasicTrainingStats[1] =
{
  {
    8uLL,
    "LiveSaveBasicTrainingStats",
    0,
    NULL,
    &LiveStorage_WriteBasicTrainingStatsFailure,
    NULL
  }
};

const TaskDefinition task_LiveReadStatsBackup[1] =
{ { 8uLL, "LiveReadStatsBackup", 0, NULL, NULL, NULL } };

const TaskDefinition task_LiveBackupStats[1] =
{ { 8uLL, "LiveBackupStats", 0, NULL, NULL, NULL } };

const TaskDefinition task_LiveBackupCorruptedStats[1] =
{ { 8uLL, "LiveBackupCorruptedStats", 0, NULL, NULL, NULL } };

const TaskDefinition task_LiveGetServerTime[1] =
{
  {
    2uLL,
    "LiveGetServerTime",
    4,
    LiveStorage_GetServerTimeComplete,
    LiveStorage_GetServerTimeFailed,
    NULL
  }
};

const TaskDefinition task_LiveFileShareGetListing[1] =
{
  {
    8uLL,
    "LiveFileShareGetListing",
    0,
    &LiveStorage_FileShare_ReadListingSuccess,
    &LiveStorage_FileShare_ReadListingFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareSearch[1] =
{
  {
    8uLL,
    "LiveFileShareSearch",
    0,
    &LiveStorage_FileShare_SearchSuccess,
    &LiveStorage_FileShare_SearchFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareLoadBlock[1] =
{
  {
    8uLL,
    "LiveFileShareLoadBlock",
    0,
    &LiveStorage_FileShare_LoadBlockSuccess,
    &LiveStorage_FileShare_LoadBlockFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareGetMetaByID[1] =
{
  {
    8uLL,
    "LiveFileShareGetMetaByID",
    0,
    &LiveStorage_FileShare_ReadMetaDataByIDSuccess,
    &LiveStorage_FileShare_ReadMetaDataByIDFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareReadFile[1] =
{
  {
    8uLL,
    "LiveFileShareReadFile",
    0,
    &LiveStorage_FileShare_ReadFileSuccess,
    &LiveStorage_FileShare_ReadFileFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareWriteFile[1] =
{
  {
    8uLL,
    "LiveFileShareWriteFile",
    0,
    &LiveStorage_FileShare_WriteFileSuccess,
    &LiveStorage_FileShare_WriteFileFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareRemoveFile[1] =
{
  {
    8uLL,
    "LiveFileShareRemoveFile",
    0,
    &LiveStorage_FileShare_RemoveFileSuccess,
    LiveStorage_FileShare_RemoveFileFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareSetTag[1] =
{
  {
    8uLL,
    "LiveFileShareSetTag",
    0,
    &LiveStorage_FileShare_WriteTagsSuccess,
    &LiveStorage_FileShare_WriteTagsFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareTransfer[1] =
{
  {
    8uLL,
    "LiveFileShareTransfer",
    0,
    &LiveStorage_FileShare_TransferFileSuccess,
    &LiveStorage_FileShare_TransferFileFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareSetSummary[1] =
{
  {
    8uLL,
    "LiveFileSetShareSummary",
    0,
    &LiveStorage_FileShare_WriteSummarySuccess,
    LiveStorage_FileShare_WriteSummaryFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareGetSummary[1] =
{
  {
    8uLL,
    "LiveFileGetShareSummary",
    0,
    &LiveStorage_FileShare_ReadSummarySuccess,
    &LiveStorage_FileShare_ReadSummaryFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareSubmitRating[1] =
{
  {
    8uLL,
    "LiveFileSubmitRating",
    0,
    LiveStorage_FileShare_WriteRatingSuccess,
    LiveStorage_FileShare_WriteRatingFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareGetRating[1] =
{
  {
    8uLL,
    "LiveFileGetRating",
    0,
    &LiveStorage_FileShare_ReadRatingSuccess,
    &LiveStorage_FileShare_ReadRatingFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareGetTopRating[1] =
{
  {
    8uLL,
    "LiveFileGetTopRated",
    0,
    &LiveStorage_FileShare_ReadTopRatedSuccess,
    &LiveStorage_FileShare_ReadTopRatedFailure,
    NULL
  }
};

const TaskDefinition task_LiveFileShareGetFilmRating[1] =
{
  {
    8uLL,
    "LiveFileShareGetFilmRating",
    0,
    &LiveStorage_FileShare_ReadFileRatingSuccess,
    NULL,
    NULL
  }
};

const TaskDefinition task_LiveFileShareGetClipRating[1] =
{
  {
    8uLL,
    "LiveFileShareGetClipRating",
    0,
    &LiveStorage_FileShare_ReadFileRatingSuccess,
    NULL,
    NULL
  }
};

const TaskDefinition task_LiveFileShareGetScreenshotRating[1] =
{
  {
    8uLL,
    "LiveFileShareGetScreenshotRating",
    0,
    &LiveStorage_FileShare_ReadFileRatingSuccess,
    NULL,
    NULL
  }
};

const TaskDefinition task_LiveFileShareGetCustomGameRating[1] =
{
  {
    8uLL,
    "LiveFileShareGetCustomGameRating",
    0,
    &LiveStorage_FileShare_ReadFileRatingSuccess,
    NULL,
    NULL
  }
};

int s_UTCOffset;
bool s_UTCSynced;

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
    return 40168;
}

unsigned __int8 __cdecl LiveStorage_GetStatsChecksumValid(int controllerIndex, statsLocation playerStatsLocation)
{
    return LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 1)[1].statsBuffer[892];
}

void __cdecl LiveStorage_SetStatsChecksumValid(int controllerIndex, statsLocation playerStatsLocation, bool isValid)
{
    LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 1)[1].statsBuffer[892] = isValid;
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
    char backupBuffer[40172]; // [esp+0h] [ebp-9CF8h] BYREF
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
    else if ( DDL_FixBufferVersion(buffer, g_statsDDL, "ddl_mp/stats.ddl", backupBuffer, 40168)
                 || DDL_FixBufferVersion(buffer, g_statsDDL, "ddl_mp/stats_archive.ddl", backupBuffer, 40168) )
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
    return LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 1)[1].statsBuffer[894];
}

void __cdecl LiveStorage_SetStatsDDLValidated(
                int controllerIndex,
                statsLocation playerStatsLocation,
                bool statsValidatedWithDDL)
{
    LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 1)[1].statsBuffer[894] = statsValidatedWithDDL;
}

void __cdecl LiveStorage_StatsBackupFetchCompleted(int controllerIndex)
{
    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    447,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    controllerNetworkData[controllerIndex].fileOps->statsBackupFileInfo.fetchCompleted = 1;
}

unsigned __int8 __cdecl LiveStorage_DoWeHaveStats(int controllerIndex, statsLocation playerStatsLocation)
{
    return LiveStorage_GetPersStatsBuffer(controllerIndex, playerStatsLocation, 0)[1].statsBuffer[895];
}

unsigned __int8 __cdecl LiveStorage_DoWeHaveCurrentStats(int controllerIndex)
{
    return LiveStorage_GetPersStatsBuffer(controllerIndex, STATS_LOCATION_NORMAL, 0)[1].statsBuffer[895];
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
    LiveStorage_GetPersStatsBuffer(localControllerIndex, playerStatsLocation, 0)[1].statsBuffer[895] = isFetched;
}

fileSharePrivateData *__cdecl LiveStorage_FileShare_GetFileShareData(fileShareBufferLocation bufferLocation)
{
    const char *v2; // eax

    if ( bufferLocation == FILESHARE_BUFFER_PRIMARY )
        return &s_fileSharePrimary;
    if ( bufferLocation == FILESHARE_BUFFER_SECONDARY )
        return &s_fileShareSecondary;
    v2 = va("Invalid file share location %d\n", bufferLocation);
    if ( !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 505, 0, v2) )
        __debugbreak();
    return 0;
}

void __cdecl LiveStorage_FetchRequiredFiles(int controllerIndex)
{
    if ( !LiveStorage_DoWeHaveOnlineWAD() )
        LiveStorage_FetchOnlineWAD(controllerIndex);
}

TaskRecord *__cdecl LiveStorage_SetupNestedTask(
                const TaskDefinition *taskdef,
                int controllerIndex,
                TaskRecord *nestedTask,
                void *payload)
{
    TaskRecord *task; // [esp+0h] [ebp-4h]

    task = TaskManager2_CreateTask(taskdef, controllerIndex, nestedTask, 0);
    if ( task )
    {
        TaskManager2_StartTask(task);
        if ( payload )
            task->payload = payload;
    }
    return task;
}

void __cdecl LiveStorage_ResetFileInfo(dwFileOperationInfo *fileInfo)
{
    fileInfo->taskType = 0;
    fileInfo->bufferSize = 0;
    fileInfo->writeCompleted = 0;
    fileInfo->isCompressedFile = 0;
    fileInfo->fileBuffer = 0;
    fileInfo->fileNotFoundFunction = 0;
    fileInfo->fileOperationSucessFunction = 0;
    fileInfo->alreadyUploaded = 0;
    fileInfo->fileTask.m_buffer = 0;
    fileInfo->fileTask.m_bufferSize = 0;
    fileInfo->fileTask.m_fileData.m_fileData = 0;
    fileInfo->fileTask.m_fileData.m_fileSize = 0;
    fileInfo->fileTask.m_fileID = 0;
    fileInfo->fileTask.m_fileInfo.m_ownerID = 0;
    fileInfo->fileTask.m_fileInfo.m_fileSize = 0;
    if ( fileInfo->menuDef && *fileInfo->menuDef )
        fileInfo->menuDef = "";
    fileInfo->fileOperationFailureFunction = 0;
    fileInfo->ownerID = 0;
}

void __cdecl LiveStorage_DeleteUserFileFailure(TaskRecord *task)
{
    unsigned int v1; // eax
    dwFileOperationInfo *fileInfo; // [esp+0h] [ebp-4h]

    if ( (!task || !task->payload)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    700,
                    0,
                    "%s",
                    "task && task->payload") )
    {
        __debugbreak();
    }
    if ( task && task->payload )
    {
        fileInfo = (dwFileOperationInfo *)task->payload;
        v1 = Sys_Milliseconds();
        Com_Printf(16, "Failed to remove %s from dw. (%dms)\n", fileInfo->fileTask.m_filename, v1 - task->startMS);
        if ( fileInfo->fileOperationFailureFunction )
            fileInfo->fileOperationFailureFunction(task->controllerIndex, task->payload);
    }
}

void __cdecl LiveStorage_DeleteUserFileSuccess(TaskRecord *task)
{
    unsigned int v1; // eax
    dwFileOperationInfo *fileInfo; // [esp+0h] [ebp-4h]

    if ( (!task || !task->payload)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    716,
                    0,
                    "%s",
                    "task && task->payload") )
    {
        __debugbreak();
    }
    if ( task && task->payload )
    {
        fileInfo = (dwFileOperationInfo *)task->payload;
        fileInfo->writeCompleted = 1;
        fileInfo->retryInfo.retryCount = 0;
        v1 = Sys_Milliseconds();
        Com_Printf(16, "Successfully removed %s from dw. (%dms)\n", fileInfo->fileTask.m_filename, v1 - task->startMS);
        if ( fileInfo->fileOperationSucessFunction )
            fileInfo->fileOperationSucessFunction(task->controllerIndex, fileInfo);
    }
}

TaskRecord *__cdecl LiveStorage_DeleteDWUserFile(int controllerIndex, dwFileOperationInfo *fileInfo)
{
    return 0;
}

void __cdecl LiveStorage_WriteUserFileFailure(TaskRecord *task)
{
    unsigned int v1; // eax
    int LocalClientNum; // eax
    dwFileOperationInfo *fileInfo; // [esp+0h] [ebp-4h]

    if ( (!task || !task->payload)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    761,
                    0,
                    "%s",
                    "task && task->payload") )
    {
        __debugbreak();
    }
    if ( task && task->payload )
    {
        fileInfo = (dwFileOperationInfo *)task->payload;
        v1 = Sys_Milliseconds();
        Com_PrintError(16, "Unable to write %s to dw. (%dms)\n", fileInfo->fileTask.m_filename, v1 - task->startMS);
        LocalClientNum = Com_ControllerIndex_GetLocalClientNum(task->controllerIndex);
        UI_CloseMenu(LocalClientNum, "popup_uploadingfile");
        if ( fileInfo->fileOperationFailureFunction )
            fileInfo->fileOperationFailureFunction(task->controllerIndex, fileInfo);
    }
}

void __cdecl LiveStorage_WriteUserFileSuccess(TaskRecord *task)
{
    unsigned int v1; // eax
    dwFileOperationInfo *fileInfo; // [esp+0h] [ebp-4h]

    if ( (!task || !task->payload)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    778,
                    0,
                    "%s",
                    "task && task->payload") )
    {
        __debugbreak();
    }
    if ( task && task->payload )
    {
        fileInfo = (dwFileOperationInfo *)task->payload;
        fileInfo->writeCompleted = 1;
        fileInfo->retryInfo.retryCount = 0;
        v1 = Sys_Milliseconds();
        Com_Printf(16, "Successfully wrote %s to dw. (%dms)\n", fileInfo->fileTask.m_filename, v1 - task->startMS);
        if ( fileInfo->fileOperationSucessFunction )
            fileInfo->fileOperationSucessFunction(task->controllerIndex, fileInfo);
    }
}

TaskRecord *__cdecl LiveStorage_WriteDWUserFile(
                int controllerIndex,
                dwFileOperationInfo *fileInfo,
                unsigned __int64 uid)
{
    return 0;
}

void __cdecl LiveStorage_GetUserFileFailure(TaskRecord *task)
{
}

void __cdecl LiveStorage_GetUserFileSuccess(TaskRecord *task)
{
}

TaskRecord *__cdecl LiveStorage_ReadDWFile(int controllerIndex, dwFileOperationInfo *fileInfo)
{
    return 0;
}

char __cdecl LiveStorage_TryFile(fileRetryInfo *retryInfo)
{
    int v1; // esi

    if ( !retryInfo
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 686, 0, "%s", "retryInfo") )
    {
        __debugbreak();
    }
    if ( retryInfo->retryCount > dwFileFetchTryMaxAttempts->current.integer )
        return 0;
    v1 = retryInfo->lastAttemptInterval + retryInfo->lastAttemptTime;
    if ( v1 > (int)Sys_Milliseconds() )
        return 0;
    LiveStorage_UpdateFileTryData(retryInfo);
    return 1;
}

void __cdecl LiveStorage_UpdateFileTryData(fileRetryInfo *retryInfo)
{
    int v1; // edx

    if ( !retryInfo
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 663, 0, "%s", "retryInfo") )
    {
        __debugbreak();
    }
    retryInfo->lastAttemptTime = Sys_Milliseconds();
    if ( retryInfo->lastAttemptInterval )
        v1 = 2 * retryInfo->lastAttemptInterval;
    else
        v1 = 1000 * dwFileFetchTryIntervalBase->current.integer;
    retryInfo->lastAttemptInterval = v1;
    if ( retryInfo->lastAttemptInterval > 1000 * dwFileFetchTryIntervalMax->current.integer )
        retryInfo->lastAttemptInterval = 1000 * dwFileFetchTryIntervalMax->current.integer;
    ++retryInfo->retryCount;
}

TaskRecord *__cdecl LiveStorage_ReadDWFileByUserID(
                int controllerIndex,
                dwFileOperationInfo *fileInfo,
                unsigned __int64 xuidLocal)
{
    return 0;
}

void __cdecl LiveStorage_RestoreStatsFromBackup(int localControllerIndex)
{
    persistentStats *v1; // eax
    persistentStats *StatsBuffer; // [esp-8h] [ebp-8h]
    unsigned int StatsBufferSize; // [esp-4h] [ebp-4h]

    LiveStorage_BackupCorruptedStats(localControllerIndex, (char*)"badmpdata");
    StatsBufferSize = LiveStorage_GetStatsBufferSize();
    StatsBuffer = LiveStorage_GetStatsBuffer(localControllerIndex, STATS_LOCATION_BACKUP, 1);
    v1 = LiveStorage_GetStatsBuffer(localControllerIndex, STATS_LOCATION_FORCE_NORMAL, 0);
    memcpy(v1->statsBuffer, StatsBuffer->statsBuffer, StatsBufferSize);
}

TaskRecord *__cdecl LiveStorage_ReadStatsBackup(int controllerIndex)
{
    TaskRecord *nestedTask; // [esp+0h] [ebp-Ch]
    playerFileOperations *fileInfo; // [esp+8h] [ebp-4h]

    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1374,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    if ( TaskManager2_TaskIsInProgressForController(task_LiveBackupStats, controllerIndex) )
        return 0;
    fileInfo = controllerNetworkData[controllerIndex].fileOps;
    fileInfo->statsBackupFileInfo.isUserFile = 1;
    fileInfo->statsBackupFileInfo.isCompressedFile = 1;
    fileInfo->statsBackupFileInfo.fileTask.m_filename = (char*)"mpdatabk0000";
    fileInfo->statsBackupFileInfo.fileBuffer = (unsigned __int8 *)LiveStorage_GetStatsBuffer(
                                                                                                                                    controllerIndex,
                                                                                                                                    STATS_LOCATION_BACKUP,
                                                                                                                                    1);
    fileInfo->statsBackupFileInfo.bufferSize = 40172;
    fileInfo->statsBackupFileInfo.fileOperationSucessFunction = (void (__cdecl *)(const int, void *))LiveStorage_StatsBackupReadSuccessful;
    fileInfo->statsBackupFileInfo.fileNotFoundFunction = (taskCompleteResults (__cdecl *)(const int, void *))LiveStorage_StatsBackupFileNotFound;
    fileInfo->statsBackupFileInfo.fileTask.m_optional = 1;
    nestedTask = LiveStorage_ReadDWFile(controllerIndex, &fileInfo->statsBackupFileInfo);
    return LiveStorage_SetupNestedTask(task_LiveReadStatsBackup, controllerIndex, nestedTask, fileInfo);
}

void __cdecl LiveStorage_StatsBackupReadSuccessful(int controllerIndex)
{
    persistentStats *StatsBuffer; // eax

    StatsBuffer = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_BACKUP, 1);
    if ( LiveStats_GetDDLHeaderVersion(StatsBuffer->statsBuffer)
        && LiveStorage_ValidateWithDDL(controllerIndex, STATS_LOCATION_BACKUP)
        && LiveStats_ValidateStatsBackup(controllerIndex) )
    {
        LiveStats_ValidateStats(controllerIndex);
    }
    else
    {
        if ( !controllerNetworkData[controllerIndex].fileOps
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                        1017,
                        0,
                        "%s",
                        "controllerNetworkData[ controllerIndex ].fileOps") )
        {
            __debugbreak();
        }
        controllerNetworkData[controllerIndex].fileOps->statsBackupFileInfo.fetchCompleted = 1;
        Com_Printf(16, "Stats backup didn't pass validation - making a new copy\n");
        LiveStorage_WriteBackupStats(controllerIndex);
    }
}

int __cdecl LiveStorage_StatsBackupFileNotFound(int controllerIndex, const char **data)
{
    if ( !data && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 1052, 0, "%s", "data") )
        __debugbreak();
    if ( !*((_BYTE *)data + 242)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1055,
                    0,
                    "%s",
                    "fileInfo->isUserFile") )
    {
        __debugbreak();
    }
    if ( *((_BYTE *)data + 242) )
    {
        data[56] = 0;
        data[57] = 0;
        data[58] = 0;
        Com_Printf(16, "No %s file found, creating a new one\n", *data);
        LiveStorage_WriteBackupStats(controllerIndex);
        return 1;
    }
    else
    {
        Com_Printf(16, "%s is not a user file - cannot create new file\n", *data);
        return 2;
    }
}

TaskRecord *__cdecl LiveStorage_SyncTime(int controllerIndex)
{
    return NULL;
}

void __cdecl LiveStorage_GetServerTimeComplete(TaskRecord *rec)
{
}

void __cdecl LiveStorage_GetServerTimeFailed(TaskRecord *rec)
{
    Com_PrintWarning(16, "DW GetServerTime() failed!\n");
}

__int64 __cdecl LiveStorage_GetUTC()
{
    return _time64(0) + s_UTCOffset;
}

int __cdecl LiveStorage_GetUTCOffset()
{
    return s_UTCOffset;
}

bool __cdecl LiveStorage_IsTimeSynced()
{
    return s_UTCSynced;
}

TaskRecord *__cdecl LiveStorage_ReadMetPlayerList(
                int controllerIndex,
                char *fileName,
                unsigned __int8 *buffer,
                int sizeOfBuffer)
{
    playerFileOperations *fileOps; // edx
    TaskRecord *nestedTask; // [esp+0h] [ebp-Ch]
    dwFileOperationInfo *fileInfo; // [esp+8h] [ebp-4h]

    if ( !LiveStorage_NeedToDownloadMetPlayerList(controllerIndex) )
        return 0;
    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1580,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    if ( TaskManager2_TaskIsInProgressForController(task_LiveFetchMetPlayerList, controllerIndex) )
        return 0;
    fileOps = controllerNetworkData[controllerIndex].fileOps;
    fileInfo = &fileOps->fetchMetPlayerListFileInfo;
    fileOps->fetchMetPlayerListFileInfo.isUserFile = 1;
    fileOps->fetchMetPlayerListFileInfo.isCompressedFile = 1;
    fileOps->fetchMetPlayerListFileInfo.fileTask.m_optional = 1;
    fileOps->fetchMetPlayerListFileInfo.fileTask.m_filename = fileName;
    fileOps->fetchMetPlayerListFileInfo.fileTask.m_optional = 1;
    fileOps->fetchMetPlayerListFileInfo.fileBuffer = buffer;
    fileOps->fetchMetPlayerListFileInfo.bufferSize = sizeOfBuffer;
    fileOps->fetchMetPlayerListFileInfo.fileOperationSucessFunction = (void (__cdecl *)(const int, void *))LiveStorage_ReadMetPlayerListSuccessful;
    fileOps->fetchMetPlayerListFileInfo.fileNotFoundFunction = (taskCompleteResults (__cdecl *)(const int, void *))LiveStorage_ReadMetPlayerListFileNotFound;
    nestedTask = LiveStorage_ReadDWFile(controllerIndex, &fileOps->fetchMetPlayerListFileInfo);
    return LiveStorage_SetupNestedTask(task_LiveFetchMetPlayerList, controllerIndex, nestedTask, fileInfo);
}

bool __cdecl LiveStorage_NeedToDownloadMetPlayerList(int controllerIndex)
{
    return false;
}

int __cdecl LiveStorage_ReadMetPlayerListFileNotFound(int controllerIndex)
{
    LiveStorage_SetLastMetPlayerListDownload(controllerIndex);
    LiveMeetPlayer_ResetData(controllerIndex);
    return 1;
}

void __cdecl LiveStorage_SetLastMetPlayerListDownload(int controllerIndex)
{
}

void __cdecl LiveStorage_ReadMetPlayerListSuccessful(int controllerIndex)
{
    LiveStorage_SetLastMetPlayerListDownload(controllerIndex);
    LiveMeetPlayer_CheckDownloadedVersionAndResetData(controllerIndex);
    Live_RequestSessionsFromRecentPlayers();
}

TaskRecord *__cdecl LiveStorage_WriteMetPlayerList(
                int controllerIndex,
                char *fileName,
                unsigned __int8 *buffer,
                int sizeOfBuffer)
{
    TaskRecord *nestedTask; // [esp+0h] [ebp-Ch]
    dwFileOperationInfo *fileInfo; // [esp+8h] [ebp-4h]

    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1616,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    if ( TaskManager2_TaskGetInProgressForController(task_LiveSaveMetPLayerList, controllerIndex) )
        return 0;
    fileInfo = &controllerNetworkData[controllerIndex].fileOps->saveMetPlayerListFileInfo;
    if ( !LiveStorage_TryFile(&controllerNetworkData[controllerIndex].fileOps->saveMetPlayerListFileInfo.retryInfo) )
        return 0;
    fileInfo->isUserFile = 1;
    fileInfo->isCompressedFile = 1;
    fileInfo->fileTask.m_filename = fileName;
    fileInfo->fileBuffer = buffer;
    fileInfo->bufferSize = sizeOfBuffer;
    fileInfo->fileOperationSucessFunction = (void (__cdecl *)(const int, void *))LiveStorage_WriteMetPlayerListSuccessful;
    nestedTask = LiveStorage_WriteDWUserFile(controllerIndex, fileInfo, 0);
    return LiveStorage_SetupNestedTask(task_LiveSaveMetPLayerList, controllerIndex, nestedTask, fileInfo);
}

void __cdecl LiveStorage_WriteMetPlayerListSuccessful(int controllerIndex)
{
    LiveMeetPlayer_ClearUploadFlags(controllerIndex);
}

TaskRecord *__cdecl LiveStorage_ReadOtherPlayerStats(int controllerIndex, unsigned __int64 playerXuid)
{
    int v3; // esi
    TaskRecord *nestedTask; // [esp+4h] [ebp-10h]
    unsigned int currTime; // [esp+Ch] [ebp-8h]
    dwFileOperationInfo *fileInfo; // [esp+10h] [ebp-4h]

    currTime = Sys_Milliseconds();
    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1674,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    if ( TaskManager2_TaskGetInProgressForController(task_LiveGetOtherPlayerStatsData, controllerIndex) )
        return 0;
    if ( playerXuid && s_XuidOfOtherPlayer && playerXuid == s_XuidOfOtherPlayer )
    {
        s_tempXuid = s_XuidOfOtherPlayer;
        v3 = currTime - s_lastStatsUpdateTimeForOtherPlayer;
        if ( v3 < Dvar_GetInt("minDelayForOtherPlayerStatsFetch") )
        {
            LiveStorage_ReadOtherPlayerStatsSuccessful(controllerIndex);
            return 0;
        }
        s_lastStatsUpdateTimeForOtherPlayer = currTime;
    }
    fileInfo = &controllerNetworkData[controllerIndex].fileOps->getOtherPlayerStatsFileInfo;
    fileInfo->retryInfo.lastAttemptTime = 0;
    fileInfo->retryInfo.lastAttemptInterval = 0;
    fileInfo->retryInfo.retryCount = 0;
    fileInfo->isUserFile = 1;
    fileInfo->isCompressedFile = 1;
    fileInfo->fileTask.m_filename = (char*)"globalstatsCompressed";
    fileInfo->fileBuffer = (unsigned __int8 *)LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_OTHERPLAYER, 1);
    fileInfo->bufferSize = 40172;
    fileInfo->fileOperationSucessFunction = (void (__cdecl *)(const int, void *))LiveStorage_ReadOtherPlayerStatsSuccessful;
    fileInfo->fileNotFoundFunction = (taskCompleteResults (__cdecl *)(const int, void *))LiveStorage_OtherPlayerStatsFileNotFound;
    fileInfo->menuDef = "popup_fetchstats";
    fileInfo->fileTask.m_optional = 1;
    if ( playerXuid )
        nestedTask = LiveStorage_ReadDWFileByUserID(controllerIndex, fileInfo, playerXuid);
    else
        nestedTask = LiveStorage_ReadDWFile(controllerIndex, fileInfo);
    return LiveStorage_SetupNestedTask(task_LiveGetOtherPlayerStatsData, controllerIndex, nestedTask, fileInfo);
}

void __cdecl LiveStorage_ReadOtherPlayerStatsSuccessful(int controllerIndex)
{
    int LocalClientNum; // eax

    LocalClientNum = Com_ControllerIndex_GetLocalClientNum(controllerIndex);
    LiveCombatRecord_OpenFriendsCombatRecord(LocalClientNum);
    s_XuidOfOtherPlayer = s_tempXuid;
}

int __cdecl LiveStorage_OtherPlayerStatsFileNotFound(int controllerIndex, const char **data)
{
    if ( !data && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 1649, 0, "%s", "data") )
        __debugbreak();
    if ( !*((_BYTE *)data + 242)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1652,
                    0,
                    "%s",
                    "fileInfo->isUserFile") )
    {
        __debugbreak();
    }
    if ( data && *((_BYTE *)data + 242) )
    {
        UI_OpenMenu(0, "stats_notfound_error_popup");
        Dvar_SetBool((dvar_s *)ui_combatOtherPlayerStatsAvailable, 0);
        return 1;
    }
    else
    {
        Com_Printf(16, "%s is not a user file\n", *data);
        return 2;
    }
}

TaskRecord *__cdecl LiveStorage_ReadPlayerStats(int controllerIndex, bool validate, bool silent)
{
    dwFileOperationInfo *fileInfo; // [esp+0h] [ebp-4h]

    if ( TaskManager2_TaskIsInProgressForController(task_LiveGetPlayerStatsData, controllerIndex) )
        return 0;
    fileInfo = &controllerNetworkData[controllerIndex].fileOps->getPlayerStatsFileInfo;
    fileInfo->fileTask.m_filename = (char*)"mpstatsCompressed";
    fileInfo->fileOperationSucessFunction = (void (__cdecl *)(const int, void *))LiveStorage_ReadPlayerStatsSuccessful;
    fileInfo->fileNotFoundFunction = (taskCompleteResults (__cdecl *)(const int, void *))LiveStorage_PlayerStatsFileNotFound;
    return LiveStorage_ReadCommonStats(
                     controllerIndex,
                     silent,
                     STATS_LOCATION_FORCE_NORMAL,
                     task_LiveGetPlayerStatsData,
                     fileInfo);
}

int __cdecl LiveStorage_PlayerStatsFileNotFound(int controllerIndex, const char **data)
{
    if ( !data && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 1072, 0, "%s", "data") )
        __debugbreak();
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 1);
    controllerNetworkData[controllerIndex].firstTimeRunning = 0;
    LiveStorage_SetStatsDDLValidated(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 0);
    UI_CloseMenu(0, "popup_gettingdata");
    UI_CloseMenu(0, "popup_connectingtodw");
    if ( !*((_BYTE *)data + 242)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1086,
                    0,
                    "%s",
                    "fileInfo->isUserFile") )
    {
        __debugbreak();
    }
    if ( *((_BYTE *)data + 242) )
    {
        data[56] = 0;
        data[57] = 0;
        data[58] = 0;
        Com_Printf(16, "No %s file found, creating a new one\n", *data);
        LiveStats_ResetStats(0, 0);
        CL_CACValidateRequest_f();
        return 1;
    }
    else
    {
        Com_Printf(16, "%s is not a user file - cannot create new file\n", *data);
        return 2;
    }
}

void __cdecl LiveStorage_ReadPlayerStatsSuccessful(int controllerIndex)
{
    persistentStats *StatsBuffer; // eax
    persistentStats *v2; // eax
    persistentStats *v3; // eax
    persistentStats *v4; // eax
    persistentStats *v5; // eax
    unsigned __int64 v6; // rax
    int v7; // [esp-4h] [ebp-24h]
    int v8; // [esp-4h] [ebp-24h]
    persistentStats *v9; // [esp-4h] [ebp-24h]
    int checksum; // [esp+0h] [ebp-20h]
    persistentStats *buffer; // [esp+4h] [ebp-1Ch]
    unsigned int currentStatsVersion; // [esp+8h] [ebp-18h]
    bool wasBasicTraining; // [esp+Eh] [ebp-12h]
    bool isCheckSumValid; // [esp+Fh] [ebp-11h]
    unsigned __int64 statsXuid; // [esp+10h] [ebp-10h]
    unsigned __int64 myXuid; // [esp+18h] [ebp-8h]

    Com_Printf(16, "Stats blob received from Demonware for controller %d.\n", controllerIndex);
    wasBasicTraining = xblive_basictraining->current.enabled;
    Dvar_SetBool((dvar_s *)xblive_basictraining, 0);
    StatsBuffer = LiveStorage_GetStatsBuffer(0, STATS_LOCATION_NORMAL, 1);
    memcpy(StatsBuffer->statsBuffer, s_tempStatsBuffer, 0x9CE8u);
    LiveStorage_SetFirstTimeRunning(0);
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 1);
    controllerNetworkData[controllerIndex].firstTimeRunning = 0;
    LiveStorage_SetStatsDDLValidated(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 0);
    UI_CloseMenu(0, "popup_gettingdata");
    v7 = LiveStorage_GetStatsBufferSize() - 4;
    v2 = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_NORMAL, 1);
    isCheckSumValid = LiveStats_ValidateChecksum(v2->statsBuffer, v7);
    LiveStorage_SetStatsChecksumValid(controllerIndex, STATS_LOCATION_FORCE_NORMAL, isCheckSumValid);
    if ( !isCheckSumValid )
    {
        v8 = LiveStorage_GetStatsBufferSize() - 4;
        v3 = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_NORMAL, 1);
        checksum = LiveStats_ChecksumGamerStats(&v3->statsBuffer[4], v8);
        Com_PrintError(16, "Stats had the wrong checksum (%u)\n", checksum);
    }
    buffer = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 1);
    if ( !LiveStats_GetDDLHeaderVersion(buffer->statsBuffer)
        || !LiveStorage_ValidateWithDDL(controllerIndex, STATS_LOCATION_FORCE_NORMAL) )
    {
        Com_PrintError(16, "Stats could not be validated vs the DDL\n");
        LiveStorage_BackupCorruptedStats(controllerIndex, (char*)"badmpdataddl");
        LiveStats_ResetStats(controllerIndex, 1);
    }
    v4 = LiveStorage_GetStatsBuffer(0, STATS_LOCATION_NORMAL, 1);
    currentStatsVersion = LiveStats_ReadVersionFromBuffer((char *)v4);
    if ( currentStatsVersion == stat_version->current.integer )
    {
        v9 = LiveStorage_GetStatsBuffer(0, STATS_LOCATION_GLOBAL, 1);
        v5 = LiveStorage_GetStatsBuffer(0, STATS_LOCATION_NORMAL, 1);
        LiveContracts_CLMergeBuffers(v5->statsBuffer, v9->statsBuffer);
    }
    else
    {
        Com_PrintError(
            16,
            "Stats had a version of %d but we expected a version of %d\n",
            currentStatsVersion,
            stat_version->current.integer);
        if ( stats_version_check->current.enabled )
            LiveStorage_DeleteGlobalStats(0);
    }
    LODWORD(v6) = Live_GetXuid(controllerIndex);
    myXuid = v6;
    statsXuid = LiveStats_ReadXUIDFromStats();
    if ( statsXuid != myXuid )
        Com_PrintError(
            16,
            "Stats were saved with an XUID of 0x%llx and the current user has XUID of 0x%llx\n",
            statsXuid,
            myXuid);
    Dvar_SetBool((dvar_s *)xblive_basictraining, wasBasicTraining);
}

TaskRecord *__cdecl LiveStorage_ReadCommonStats(
                int controllerIndex,
                bool silent,
                statsLocation location,
                const TaskDefinition *taskDef,
                dwFileOperationInfo *fileInfo)
{
    TaskRecord *nestedTask; // [esp+0h] [ebp-8h]

    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1747,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    fileInfo->retryInfo.lastAttemptTime = 0;
    fileInfo->retryInfo.lastAttemptInterval = 0;
    fileInfo->retryInfo.retryCount = 0;
    fileInfo->isUserFile = 1;
    fileInfo->isCompressedFile = 1;
    fileInfo->fileTask.m_optional = 1;
    fileInfo->fileBuffer = (unsigned __int8 *)LiveStorage_GetStatsBuffer(controllerIndex, location, 1);
    fileInfo->bufferSize = LiveStorage_GetStatsBufferSize();
    fileInfo->menuDef = "popup_fetchstats";
    if ( location != STATS_LOCATION_BASICTRAINING )
    {
        memset(s_tempStatsBuffer, 0, sizeof(s_tempStatsBuffer));
        fileInfo->fileBuffer = s_tempStatsBuffer;
        fileInfo->bufferSize = 40168;
    }
    nestedTask = LiveStorage_ReadDWFile(controllerIndex, fileInfo);
    return LiveStorage_SetupNestedTask(taskDef, controllerIndex, nestedTask, fileInfo);
}

TaskRecord *__cdecl LiveStorage_ReadBasicTrainingStats(int controllerIndex, bool validate, bool silent)
{
    dwFileOperationInfo *fileInfo; // [esp+0h] [ebp-4h]

    if ( TaskManager2_TaskIsInProgressForController(task_LiveGetBasicTrainingStatsData, controllerIndex) )
        return 0;
    fileInfo = &controllerNetworkData[controllerIndex].fileOps->getBasicTrainingStatsFileInfo;
    fileInfo->fileTask.m_filename = (char*)"mpstatsBasicTraining";
    fileInfo->fileOperationSucessFunction = (void (__cdecl *)(const int, void *))LiveStorage_ReadBasicTrainingStatsSuccessful;
    fileInfo->fileNotFoundFunction = (taskCompleteResults (__cdecl *)(const int, void *))LiveStorage_BasicTrainingStatsFileNotFound;
    return LiveStorage_ReadCommonStats(
                     controllerIndex,
                     silent,
                     STATS_LOCATION_BASICTRAINING,
                     task_LiveGetBasicTrainingStatsData,
                     fileInfo);
}

int __cdecl LiveStorage_BasicTrainingStatsFileNotFound(int controllerIndex, const char **data)
{
    if ( !data && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 1109, 0, "%s", "data") )
        __debugbreak();
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
    controllerNetworkData[controllerIndex].firstTimeRunning = 0;
    LiveStorage_SetStatsDDLValidated(controllerIndex, STATS_LOCATION_BASICTRAINING, 0);
    UI_CloseMenu(0, "popup_gettingdata");
    UI_CloseMenu(0, "popup_connectingtodw");
    if ( !*((_BYTE *)data + 242)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1122,
                    0,
                    "%s",
                    "fileInfo->isUserFile") )
    {
        __debugbreak();
    }
    if ( *((_BYTE *)data + 242) )
    {
        data[56] = 0;
        data[57] = 0;
        data[58] = 0;
        Com_Printf(16, "No %s file found, creating a new one\n", *data);
        LiveStats_ResetBasicTrainingStats(controllerIndex);
        return 1;
    }
    else
    {
        Com_Printf(16, "%s is not a user file - cannot create new file\n", *data);
        return 2;
    }
}

void __cdecl LiveStorage_ReadBasicTrainingStatsSuccessful(int controllerIndex)
{
    persistentStats *StatsBuffer; // eax
    persistentStats *v2; // eax
    int v3; // [esp-4h] [ebp-14h]
    int v4; // [esp-4h] [ebp-14h]
    int checksum; // [esp+0h] [ebp-10h]
    persistentStats *buffer; // [esp+4h] [ebp-Ch]
    int currentStatsVersion; // [esp+8h] [ebp-8h] BYREF
    bool isCheckSumValid; // [esp+Fh] [ebp-1h]

    Com_Printf(16, "Basic Training Stats blob received from Demonware for controller %d.\n", controllerIndex);
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
    LiveStorage_SetStatsDDLValidated(controllerIndex, STATS_LOCATION_BASICTRAINING, 0);
    UI_CloseMenu(0, "popup_gettingdata");
    v3 = LiveStorage_GetStatsBufferSize() - 4;
    StatsBuffer = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
    isCheckSumValid = LiveStats_ValidateChecksum(StatsBuffer->statsBuffer, v3);
    LiveStorage_SetStatsChecksumValid(controllerIndex, STATS_LOCATION_BASICTRAINING, isCheckSumValid);
    if ( !isCheckSumValid )
    {
        v4 = LiveStorage_GetStatsBufferSize() - 4;
        v2 = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
        checksum = LiveStats_ChecksumGamerStats(&v2->statsBuffer[4], v4);
        Com_PrintError(16, "Basic Training Stats had the wrong checksum (%u)\n", checksum);
    }
    buffer = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
    if ( !LiveStats_GetDDLHeaderVersion(buffer->statsBuffer)
        || !LiveStorage_ValidateWithDDL(controllerIndex, STATS_LOCATION_BASICTRAINING) )
    {
        Com_PrintError(16, "Basic Training Stats could not be validated vs the DDL\n");
        LiveStats_ResetBasicTrainingStats(controllerIndex);
    }
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

void __cdecl LiveStorage_WritePlayerStatsFailure(TaskRecord *task)
{
    if ( !task && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 1879, 0, "%s", "task") )
        __debugbreak();
    if ( task )
        LiveStorage_SetStatsWriteNeeded(task->controllerIndex, 1, STATS_LOCATION_FORCE_NORMAL);
}

void __cdecl LiveStorage_WriteBasicTrainingStatsFailure(TaskRecord *task)
{
    if ( !task && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 1890, 0, "%s", "task") )
        __debugbreak();
    if ( task )
        LiveStorage_SetStatsWriteNeeded(task->controllerIndex, 1, STATS_LOCATION_BASICTRAINING);
}

TaskRecord *__cdecl LiveStorage_WriteStats(
                int controllerIndex,
                bool backup,
                statsLocation location,
                const TaskDefinition *taskDef,
                dwFileOperationInfo *fileInfo)
{
    persistentStats *StatsBuffer; // eax
    persistentStats *v7; // eax
    int v8; // [esp-4h] [ebp-10h]
    int checksum; // [esp+0h] [ebp-Ch]
    TaskRecord *nestedTask; // [esp+4h] [ebp-8h]
    TaskRecord *task; // [esp+8h] [ebp-4h]

    if ( location != STATS_LOCATION_BASICTRAINING )
        return 0;
    StatsBuffer = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
    if ( LiveStats_GetBasicTrainingState(StatsBuffer->statsBuffer) )
    {
        if ( !controllerNetworkData[controllerIndex].fileOps
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                        1843,
                        0,
                        "%s",
                        "controllerNetworkData[ controllerIndex ].fileOps") )
        {
            __debugbreak();
        }
        fileInfo->isUserFile = 1;
        fileInfo->isCompressedFile = 1;
        fileInfo->fileBuffer = (unsigned __int8 *)LiveStorage_GetStatsBuffer(
                                                                                                controllerIndex,
                                                                                                STATS_LOCATION_BASICTRAINING,
                                                                                                1);
        fileInfo->bufferSize = LiveStorage_GetStatsBufferSize();
        if ( backup )
            LiveStats_SetStatByKey(controllerIndex, MP_PLAYERSTATSKEY_STATSBACKUPVERSION, 40109);
        LiveStorage_SetStatsWriteNeeded(controllerIndex, 0, STATS_LOCATION_BASICTRAINING);
        v8 = LiveStorage_GetStatsBufferSize() - 4;
        v7 = LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_BASICTRAINING, 1);
        checksum = LiveStats_ChecksumGamerStats(&v7->statsBuffer[4], v8);
        *(unsigned int *)LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_BASICTRAINING, 1)->statsBuffer = checksum;
        nestedTask = LiveStorage_WriteDWUserFile(controllerIndex, fileInfo, 0);
        task = LiveStorage_SetupNestedTask(taskDef, controllerIndex, nestedTask, fileInfo);
        if ( !task )
            LiveStorage_SetStatsWriteNeeded(controllerIndex, 1, STATS_LOCATION_BASICTRAINING);
        return task;
    }
    else
    {
        Com_PrintError(14, "non-basic training stats when uploading basic training stats\n");
        return 0;
    }
}

TaskRecord *__cdecl LiveStorage_WriteBasicTrainingStats(int controllerIndex)
{
    dwFileOperationInfo *fileInfo; // [esp+0h] [ebp-4h]

    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1942,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    if ( !LiveStorage_DoWeHaveStats(controllerIndex, STATS_LOCATION_BASICTRAINING) )
        return 0;
    if ( !controllerNetworkData[controllerIndex].basicTrainingStats[40168] )
        return 0;
    if ( !controllerNetworkData[controllerIndex].basicTrainingStats[40170] )
        return 0;
    if ( !LiveStorage_GetStatsWriteNeeded(controllerIndex, STATS_LOCATION_BASICTRAINING) )
        return 0;
    if ( TaskManager2_TaskIsInProgressForController(task_LiveSaveBasicTrainingStats, controllerIndex) )
        return 0;
    fileInfo = &controllerNetworkData[controllerIndex].fileOps->saveBasicTrainingStatsFileInfo;
    if ( !LiveStorage_TryFile(&controllerNetworkData[controllerIndex].fileOps->saveBasicTrainingStatsFileInfo.retryInfo) )
        return 0;
    fileInfo->fileTask.m_filename = (char*)"mpstatsBasicTraining";
    return LiveStorage_WriteStats(
                     controllerIndex,
                     0,
                     STATS_LOCATION_BASICTRAINING,
                     task_LiveSaveBasicTrainingStats,
                     fileInfo);
}

void __cdecl LiveStorage_WriteBackupStats(int controllerIndex)
{
    int v1; // eax
    playerFileOperations *fileInfo; // [esp+4h] [ebp-4h]

    if ( LiveStorage_DoWeHaveStats(controllerIndex, STATS_LOCATION_FORCE_NORMAL)
        && !TaskManager2_TaskIsInProgressForController(task_LiveBackupStats, controllerIndex) )
    {
        if ( !controllerNetworkData[controllerIndex].fileOps
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                        2026,
                        0,
                        "%s",
                        "controllerNetworkData[ controllerIndex ].fileOps") )
        {
            __debugbreak();
        }
        fileInfo = controllerNetworkData[controllerIndex].fileOps;
        LiveStorage_ResetFileInfo(&fileInfo->statsBackupFileInfo);
        v1 = SystemTimeToInt();
        LiveStats_SetStatByKey(controllerIndex, MP_PLAYERSTATSKEY_LASTSTATSBACKUP, v1);
        fileInfo->statsBackupFileInfo.fileBuffer = (unsigned __int8 *)LiveStorage_GetStatsBuffer(
                                                                                                                                        controllerIndex,
                                                                                                                                        STATS_LOCATION_FORCE_NORMAL,
                                                                                                                                        1);
        fileInfo->statsBackupFileInfo.bufferSize = LiveStorage_GetStatsBufferSize();
        fileInfo->statsBackupFileInfo.fileTask.m_filename = (char*)"mpdatabk0000";
        *(unsigned int *)fileInfo->statsBackupFileInfo.fileBuffer = LiveStats_ChecksumGamerStats(
                                                                                                                        fileInfo->statsBackupFileInfo.fileBuffer + 4,
                                                                                                                        fileInfo->statsBackupFileInfo.bufferSize - 4);
        LiveStorage_WriteBackupStatsInternal(controllerIndex, &fileInfo->statsBackupFileInfo, task_LiveBackupStats);
    }
}

TaskRecord *__cdecl LiveStorage_WriteBackupStatsInternal(
                int controllerIndex,
                dwFileOperationInfo *fileInfo,
                const TaskDefinition *taskdef)
{
    TaskRecord *nestedTask; // [esp+0h] [ebp-8h]

    if ( !fileInfo
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 1985, 0, "%s", "fileInfo") )
    {
        __debugbreak();
    }
    if ( !fileInfo->fileBuffer
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1987,
                    0,
                    "%s",
                    "fileInfo->fileBuffer") )
    {
        __debugbreak();
    }
    if ( !fileInfo->bufferSize
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    1988,
                    0,
                    "%s",
                    "fileInfo->bufferSize") )
    {
        __debugbreak();
    }
    if ( maxStatsBackupInterval->current.integer > 0 )
    {
        if ( LiveStorage_TryFile(&fileInfo->retryInfo) )
        {
            fileInfo->isUserFile = 1;
            fileInfo->isCompressedFile = 1;
            nestedTask = LiveStorage_WriteDWUserFile(controllerIndex, fileInfo, 0);
            return LiveStorage_SetupNestedTask(taskdef, controllerIndex, nestedTask, fileInfo);
        }
        else
        {
            return 0;
        }
    }
    else
    {
        fileInfo->writeCompleted = 1;
        return 0;
    }
}

void __cdecl LiveStorage_BackupCorruptedStats(int controllerIndex, char *filename)
{
    dwFileOperationInfo *fileInfo; // [esp+0h] [ebp-4h]

    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    2049,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    fileInfo = &controllerNetworkData[controllerIndex].fileOps->corruptedStatsBackupFileInfo;
    LiveStorage_ResetFileInfo(fileInfo);
    fileInfo->fileBuffer = (unsigned __int8 *)LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 0);
    fileInfo->bufferSize = LiveStorage_GetStatsBufferSize();
    fileInfo->fileTask.m_filename = filename;
    LiveStorage_WriteBackupStatsInternal(controllerIndex, fileInfo, task_LiveBackupCorruptedStats);
}

void __cdecl LiveStorage_SetAllStatsNotFetched(int controllerIndex)
{
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_FORCE_NORMAL, 0);
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_BACKUP, 0);
    LiveStats_SetBufferInitialised(controllerIndex, 0);
    LiveStorage_SetStatsFetched(controllerIndex, STATS_LOCATION_BASICTRAINING, 0);
}

void __cdecl LiveStorage_FileShare_UpdateMetaData(char *metaData)
{
    ddlState_t state; // [esp+0h] [ebp-10h] BYREF

    if ( !DDL_MoveToName(&g_fileShareRootState, &state, "name")
        || !DDL_SetString(&state, fsSelectedFileName->current.string, metaData) )
    {
        Com_PrintError(16, "Unable to update file share meta data.\n");
    }
    if ( !DDL_MoveToName(&g_fileShareRootState, &state, "isModifiedName")
        || !DDL_SetInt(&state, fsIsSelectedFileNameModified->current.color[0], metaData) )
    {
        Com_PrintError(16, "Unable to update file share meta data.\n");
    }
    if ( !DDL_MoveToName(&g_fileShareRootState, &state, "description")
        || !DDL_SetString(&state, fsSelectedFileDescription->current.string, metaData) )
    {
        Com_PrintError(16, "Unable to update file share meta data.\n");
    }
    if ( !DDL_MoveToName(&g_fileShareRootState, &state, "isModifiedDescription")
        || !DDL_SetInt(&state, fsIsSelectedFileDescriptionModified->current.color[0], metaData) )
    {
        Com_PrintError(16, "Unable to update file share meta data.\n");
    }
}

bool __cdecl LiveStorage_FileShare_IsValidHTTPCode(int httpCode)
{
    return !httpCode || httpCode >= 200 && httpCode <= 299;
}

fileShareLocation __cdecl LiveStorage_FileShare_GetCurrentHTTPLocation(int controllerIndex)
{
    return FILESHARE_LOCATION_INVALID;
}

void __cdecl LiveStorage_FileShare_GetDownloadProgress(int controllerIndex, unsigned int *bytes, float *dataRate)
{
}

void __cdecl LiveStorage_FileShare_GetUploadProgress(int controllerIndex, unsigned int *bytes, float *dataRate)
{
}

void __cdecl LiveStorage_FileShare_AbortOperation(int controllerIndex, fileShareLocation location)
{
}

TaskRecord *__cdecl LiveStorage_FileShare_ReadListing(
                int controllerIndex,
                unsigned __int64 playerXuid,
                fileShareBufferLocation bufferLocation)
{
    return 0;
}

void __cdecl LiveStorage_FileShare_ReadListingFailure(TaskRecord *task)
{
    int LocalClientNum; // eax
    const char *menuDef; // [esp-4h] [ebp-Ch]
    fileSharePrivateData *fileShareData; // [esp+0h] [ebp-8h]
    dwFileShareListTask *fileShareListTask; // [esp+4h] [ebp-4h]

    fileShareListTask = (dwFileShareListTask *)task->payload;
    fileShareData = LiveStorage_FileShare_GetFileShareData((fileShareBufferLocation)fileShareListTask->bufferLocation);
    if ( fileShareData )
    {
        fileShareData->valid = 0;
        if ( fileShareListTask->menuDef )
        {
            if ( *fileShareListTask->menuDef )
            {
                menuDef = fileShareListTask->menuDef;
                LocalClientNum = Com_ControllerIndex_GetLocalClientNum(task->controllerIndex);
                UI_CloseMenu(LocalClientNum, menuDef);
            }
        }
        Com_PrintError(16, "Error reading user file share.\n");
    }
}

void __cdecl LiveStorage_FileShare_KickOffRatingsTask(
                int controllerIndex,
                fileSharePrivateData *fileShareData,
                int category,
                dwFileShareGetRatingTask *ratingTask,
                const TaskDefinition *sym,
                int *resultOffset)
{
    int i; // [esp+4h] [ebp-A8h]
    int numFileIDs; // [esp+8h] [ebp-A4h]
    unsigned __int64 fileIDs[20]; // [esp+Ch] [ebp-A0h] BYREF

    numFileIDs = 0;
    for ( i = 0; i < fileShareData->descriptorCount; ++i )
    {
        if ( fileShareData->descriptors[i].m_category == category )
        {
            LODWORD(fileIDs[numFileIDs]) = fileShareData->descriptors[i].m_fileID;
            HIDWORD(fileIDs[numFileIDs++]) = HIDWORD(fileShareData->descriptors[i].m_fileID);
        }
    }
    if ( numFileIDs > 0 )
    {
        ratingTask->fileIDs = fileIDs;
        ratingTask->maxFilesToFetch = numFileIDs;
        ratingTask->leaderboardID = category;
        ratingTask->outStatsInfo = &fileShareData->fileRatings[*resultOffset];
        LiveStorage_FileShare_ReadRating(controllerIndex, ratingTask, sym);
        *resultOffset += numFileIDs;
    }
}

void __cdecl LiveStorage_FileShare_ReadListingSuccess(TaskRecord *task)
{
}

bool __cdecl LiveStorage_FileShare_IsPageLoading(int controllerIndex)
{
    return TaskManager2_TaskIsInProgressForController(task_LiveFileShareLoadBlock, controllerIndex);
}

void __cdecl LiveStorage_FileShare_PublicMetaToTags(bdFileMetaData *descriptors, int descriptorCount)
{
    int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < descriptorCount; ++i )
        Live_FileShare_PublicMetaDataToTags(&descriptors[i]);
}

void __cdecl LiveStorage_FileShare_LoadBlockSuccessAction(dwFileShareDescriptorsTask *fsTask)
{
    int i; // [esp+0h] [ebp-4h]

    if ( *fsTask->descriptorCount > 0 )
    {
        if ( Live_FileShare_GetSortBy() <= 0 )
            qsort(
                fsTask->descriptors,
                *fsTask->descriptorCount,
                0x838u,
                (int (__cdecl *)(const void *, const void *))LiveStorage_FileShareSortComparator_MostRecent);
        else
            qsort(
                fsTask->descriptors,
                *fsTask->descriptorCount,
                0x838u,
                (int (__cdecl *)(const void *, const void *))LiveStorage_FileShareSortComparator_MostPopular);
        for ( i = 0; i < *fsTask->descriptorCount; ++i )
            Live_FileShareCache_AddDescriptor(fsTask->descriptors[i].m_fileID, fsTask->location, &fsTask->descriptors[i]);
        if ( fsTask->location == FILESHARE_LOCATION_POOLEDSTORAGE )
            LiveStorage_FileShare_PublicMetaToTags(fsTask->descriptors, *fsTask->descriptorCount);
    }
    if ( fsTask->successCallback )
        fsTask->successCallback();
}

int __cdecl LiveStorage_FileShareSortComparator_MostRecent(unsigned int *arg0, unsigned int *arg1)
{
    if ( !arg0 && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 2644, 0, "%s", "arg0") )
        __debugbreak();
    if ( !arg1 && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 2645, 0, "%s", "arg1") )
        __debugbreak();
    return arg1[4] - arg0[4];
}

int __cdecl LiveStorage_FileShareSortComparator_MostPopular(const bdFileMetaData *arg0, const bdFileMetaData *arg1)
{
    int numRatings; // [esp+0h] [ebp-18h] BYREF
    int rank2; // [esp+4h] [ebp-14h] BYREF
    const bdFileMetaData *fileMetaData1; // [esp+8h] [ebp-10h]
    const bdFileMetaData *fileMetaData2; // [esp+Ch] [ebp-Ch]
    int rank1; // [esp+10h] [ebp-8h] BYREF
    bdVoteRankStatsInfo *ratings; // [esp+14h] [ebp-4h] BYREF

    if ( !arg0 && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 2655, 0, "%s", "arg0") )
        __debugbreak();
    if ( !arg1 && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 2656, 0, "%s", "arg1") )
        __debugbreak();
    fileMetaData1 = arg0;
    fileMetaData2 = arg1;
    Live_FileShareSearch_GetRatings(&ratings, &numRatings);
    Live_FileShare_LookupRank(fileMetaData1->m_fileID, ratings, numRatings, &rank1);
    Live_FileShare_LookupRank(fileMetaData2->m_fileID, ratings, numRatings, &rank2);
    return rank1 - rank2;
}

TaskRecord *__cdecl LiveStorage_FileShare_LoadPage(
                int controllerIndex,
                int numFiles,
                bdFileID *fileIDs,
                fileShareLocation location,
                bdFileMetaData *outDescriptors,
                int *outDescriptorCount,
                void (__cdecl *successCallback)(),
                void (__cdecl *failureCallback)())
{
    return NULL;
}

void __cdecl LiveStorage_FileShare_LoadBlockFailure(TaskRecord *task)
{
    dwFileShareDescriptorsTask *fsTask; // [esp+0h] [ebp-4h]

    Com_PrintError(16, "\tSearch: Could not get descriptors for file share search results.\n");
    fsTask = (dwFileShareDescriptorsTask *)task->payload;
    if ( fsTask->failureCallback )
        fsTask->failureCallback();
}

void __cdecl LiveStorage_FileShare_LoadBlockSuccess(TaskRecord *task)
{
}

void __cdecl LiveStorage_FileShare_DownloadFile_f()
{
    const char *v0; // eax
    const char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    const char *v4; // eax
    unsigned int v5; // [esp+0h] [ebp-174h]
    int intResult; // [esp+4Ch] [ebp-128h] BYREF
    int controllerIndex; // [esp+50h] [ebp-124h]
    int localClientNum; // [esp+54h] [ebp-120h]
    const char *stringResult; // [esp+58h] [ebp-11Ch] BYREF
    int fileSize; // [esp+5Ch] [ebp-118h]
    bool isUserFile; // [esp+63h] [ebp-111h]
    char filmName[256]; // [esp+64h] [ebp-110h] BYREF
    bool renderIt; // [esp+16Bh] [ebp-9h]
    unsigned __int64 fileId; // [esp+16Ch] [ebp-8h]

    if ( Cmd_Argc() == 6 )
    {
        localClientNum = 0;
        controllerIndex = 0;
        v0 = Cmd_Argv(1);
        isUserFile = atoi(v0) != 0;
        v1 = Cmd_Argv(2);
        fileId = I_atoi64(v1);
        v2 = Cmd_Argv(3);
        fileSize = atoi(v2);
        v5 = strlen(Cmd_Argv(4));
        v3 = Cmd_Argv(4);
        I_strncpyz(filmName, v3, v5 + 1);
        v4 = Cmd_Argv(5);
        renderIt = atoi(v4) != 0;
        Dvar_SetStringByName("ui_demoname", filmName);
        Live_FileShare_Theater_SetFile(fileId, isUserFile);
        Com_Printf(16, "Loaded file meta data for theater lobby.\n");
        if ( Live_FileShare_Theater_GetName((char **)&stringResult) && stringResult && *stringResult )
            Com_Printf(16, "\tName: %s\n", stringResult);
        if ( Live_FileShare_Theater_GetDescription((char **)&stringResult) && stringResult && *stringResult )
            Com_Printf(16, "\tDescription: %s\n", stringResult);
        if ( Live_FileShare_Theater_GetSize(&intResult) )
            Com_Printf(16, "\tSize: %d\n", intResult);
        if ( Live_FileShare_Theater_GetLength(&intResult) )
            Com_Printf(16, "\tLength: %d\n", intResult);
        if ( Live_FileShare_Theater_GetType(&intResult) )
            Com_Printf(16, "\tType: %d\n", intResult);
        if ( Live_FileShare_Theater_GetMap((char **)&stringResult) && stringResult && *stringResult )
            Com_Printf(16, "\tMap: %s\n", stringResult);
        if ( Live_FileShare_Theater_GetGameType((char **)&stringResult) && stringResult )
        {
            if ( *stringResult )
                Com_Printf(16, "\tGameType: %s\n", stringResult);
        }
        Demo_DownloadFile(controllerIndex, filmName, fileId, fileSize, isUserFile, renderIt);
    }
    else
    {
        Com_PrintError(16, "Error: Cannot call downloaddemofile when argc is not 6.\n");
    }
}

bool __cdecl LiveStorage_FileShare_IsPerformingSearch(int controllerIndex)
{
    return TaskManager2_TaskIsInProgressForController(task_LiveFileShareSearch, controllerIndex);
}

TaskRecord *__cdecl LiveStorage_FileShare_PerformSearch(int controllerIndex, fileShareSearchInfo_t *taskInfo)
{
    return 0;
}

void __cdecl LiveStorage_FileShare_SearchFailure(TaskRecord *task)
{
    dwFileShareSearchTask *fileShareSearchTask; // [esp+0h] [ebp-4h]

    Com_PrintError(16, "Could not search file share by tag.\n");
    fileShareSearchTask = (dwFileShareSearchTask *)task->payload;
    if ( fileShareSearchTask->failureCallback )
        fileShareSearchTask->failureCallback();
}

void __cdecl LiveStorage_FileShare_SearchSuccess(TaskRecord *task)
{
}

unsigned int __cdecl LiveStorage_FileShare_ReadFileData(
                void *data,
                unsigned int dataSize,
                dwFileShareReadFileTask *fileTask)
{
    fileTask->bytesDownloaded += dataSize;
    fileTask->dataCallback(data, dataSize, fileTask->bytesDownloaded, fileTask->fileSize);
    return dataSize;
}

unsigned int __cdecl LiveStorage_FileShare_GetDownloadingFileTotalSize(int controllerIndex)
{
    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    2997,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    return controllerNetworkData[controllerIndex].fileOps->fileShareReadFileTask.fileSize;
}

unsigned int __cdecl LiveStorage_FileShare_GetUploadingFileTotalSize(int controllerIndex)
{
    if ( !controllerNetworkData[controllerIndex].fileOps
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    3005,
                    0,
                    "%s",
                    "controllerNetworkData[ controllerIndex ].fileOps") )
    {
        __debugbreak();
    }
    return controllerNetworkData[controllerIndex].fileOps->fileShareWriteFileTask.fileSize;
}

TaskRecord *__cdecl LiveStorage_FileShare_ReadFile(int controllerIndex, fileShareReadFileInfo *fileInfo)
{
    return NULL;
}

unsigned int __thiscall fileShareDownloadInterceptor::handleDownload(
                void *data,
                unsigned int dataSize)
{
    return this->callback(data, dataSize, this->task);
}

void __cdecl LiveStorage_FileShare_ReadFileFailure(TaskRecord *task)
{
}

void __cdecl LiveStorage_FileShare_ReadFileSuccess(TaskRecord *task)
{
}

char *__cdecl LiveStorage_GetMatchRecordBuffer()
{
    return s_matchRecordBinaryData;
}

int __cdecl LiveStorage_GetMatchRecordBufferSize()
{
    return 66560;
}

unsigned int __cdecl LiveStorage_FileShare_WriteFileData(
                void *data,
                unsigned int dataSize,
                unsigned int bytesUploaded,
                dwFileShareWriteFileTask *fileTask)
{
    if ( !fileTask->dataCallback
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    3163,
                    0,
                    "%s",
                    "fileTask->dataCallback") )
    {
        __debugbreak();
    }
    return fileTask->dataCallback(data, dataSize, bytesUploaded);
}

TaskRecord *__cdecl LiveStorage_FileShare_WriteFile(int controllerIndex, fileShareWriteFileInfo *writeFileInfo)
{
    return NULL;
}

//unsigned int fileShareUploadInterceptor::handleUpload(
//                void *data,
//                unsigned int dataSize,
//                unsigned int bytesUploaded)
//{
//    return this->callback(data, dataSize, bytesUploaded, this->task);
//}

void __cdecl LiveStorage_FileShare_WriteFileFailure(TaskRecord *task)
{
}

void __cdecl LiveStorage_FileShare_WriteFileSuccess(TaskRecord *task)
{
}

void __cdecl LiveStorage_FileShare_GenerateHeatmap(
                int controllerIndex,
                unsigned __int64 fileID,
                unsigned __int8 *buffer,
                unsigned int bufferSize)
{
}

TaskRecord *__cdecl LiveStorage_FileShare_WriteSummary(
                int controllerIndex,
                unsigned __int64 fileID,
                fileShareLocation location,
                void *summaryData,
                unsigned int summaryDataSize,
                const void *metaData,
                unsigned int metaDataSize,
                bdTag *tags,
                unsigned int numTags,
                bool showSuccess)
{
    return 0;
}

void __cdecl LiveStorage_FileShare_WriteSummaryFailure(TaskRecord *rec)
{
    Com_PrintError(16, "Failed to write summary data.\n");
}

void __cdecl LiveStorage_FileShare_WriteSummarySuccess(TaskRecord *task)
{
    int LocalClientNum; // eax
    char *v2; // [esp-8h] [ebp-Ch]

    Com_Printf(16, "Summary data written successfully.\n");
    if ( *((_BYTE *)task->payload + 36) )
    {
        v2 = UI_SafeTranslateString("MENU_FILESHARE_UPDATED_CAPS");
        LocalClientNum = Com_ControllerIndex_GetLocalClientNum(task->controllerIndex);
        UI_OpenToastPopup(LocalClientNum, "menu_mp_killstreak_select", "", v2, 2700);
    }
}

TaskRecord *__cdecl LiveStorage_FileShare_ReadSummary(
                int controllerIndex,
                bdFileMetaData *descriptor,
                void (__cdecl *successCallback)(),
                void (__cdecl *failureCallback)())
{
    return 0;
}

void __cdecl LiveStorage_FileShare_ReadSummaryFailure(TaskRecord *task)
{
    dwFileShareSummaryTask *summaryTask; // [esp+0h] [ebp-4h]

    Com_PrintError(16, "Failed to get summary data.\n");
    summaryTask = (dwFileShareSummaryTask *)task->payload;
    if ( summaryTask->failureCallback )
        summaryTask->failureCallback();
}

void __cdecl LiveStorage_FileShare_ReadSummarySuccess(TaskRecord *task)
{
    dwFileShareSummaryTask *summaryTask; // [esp+0h] [ebp-4h]

    summaryTask = (dwFileShareSummaryTask *)task->payload;
    LiveStorage_FileShare_GenerateHeatmap(
        task->controllerIndex,
        summaryTask->fileID,
        (unsigned __int8 *)summaryTask->summaryData,
        summaryTask->summaryDataSize);
    if ( summaryTask->successCallback )
        summaryTask->successCallback();
}

TaskRecord *__cdecl LiveStorage_FileShare_ReadMetaDataByID(
                int controllerIndex,
                bdFileID *fileIDs,
                unsigned int numFiles,
                bdFileMetaData *outDescriptors,
                fileShareLocation location)
{
    return 0;
}

void __cdecl LiveStorage_FileShare_ReadMetaDataByIDFailure(TaskRecord *task)
{
    const ddlState_t *RootDDLState; // eax
    ddlState_t localState; // [esp+0h] [ebp-10h] BYREF

    Com_PrintError(16, "Failed to get last played game summary data.\n");
    RootDDLState = LiveStats_GetRootDDLState();
    DDL_MoveTo(RootDDLState, &localState, 2, "AfterActionReportStats", "demoFileID");
    LiveStats_SetDInt64Stat(task->controllerIndex, &localState, 0);
}

void __cdecl LiveStorage_FileShare_ReadMetaDataByIDSuccess(TaskRecord *task)
{
    Com_Printf(16, "Got last played game summary.\n");
    LiveStorage_FileShare_PublicMetaToTags(*((bdFileMetaData **)task->payload + 4), *((unsigned int *)task->payload + 1));
    Live_FileShare_PopulateLastPlayedGameDvars();
}

TaskRecord *__cdecl LiveStorage_FileShare_RemoveFile(int controllerIndex, unsigned int fileSlot)
{
    return 0;
}

void __cdecl LiveStorage_FileShare_RemoveFileFailure(TaskRecord *rec)
{
    Com_PrintError(16, "Failed to removes file from the content server.\n");
}

void __cdecl LiveStorage_FileShare_RemoveFileSuccess(TaskRecord *task)
{
    unsigned __int64 v1; // rax

    Com_Printf(16, "Successfully removed file from slot %d\n", *(unsigned int *)task->payload);
    LiveStorage_FileShare_GetFileShareData(FILESHARE_BUFFER_PRIMARY)->valid = 0;
    LODWORD(v1) = Live_GetXuid(task->controllerIndex);
    LiveStorage_FileShare_ReadListing(task->controllerIndex, v1, FILESHARE_BUFFER_PRIMARY);
}

TaskRecord *__cdecl LiveStorage_FileShare_TransferLastUploaded(int controllerIndex, unsigned int userSlot)
{
    return NULL;
}

TaskRecord *__cdecl LiveStorage_FileShare_TransferFromPooled(
                int controllerIndex,
                unsigned __int64 fileID,
                unsigned int userSlot)
{
    return NULL;
}

TaskRecord *__cdecl LiveStorage_FileShare_TransferFromUser(
                int controllerIndex,
                unsigned __int64 fileID,
                unsigned int userSlot,
                bool isCommunity)
{
    return NULL;
}

TaskRecord *__cdecl LiveStorage_FileShare_TransferFile(
                int controllerIndex,
                unsigned __int64 fileID,
                unsigned int userSlot,
                fileShareLocation location,
                bool isCommunityFile)
{
    //BLOPS_NULLSUB();
    if ( TaskManager2_TaskIsInProgressForController(task_LiveFileShareTransfer, controllerIndex) )
        return 0;
    if ( location == FILESHARE_LOCATION_USERSTORAGE )
        return LiveStorage_FileShare_TransferFromUser(controllerIndex, fileID, userSlot, isCommunityFile);
    if ( location == FILESHARE_LOCATION_POOLEDSTORAGE )
        return LiveStorage_FileShare_TransferFromPooled(controllerIndex, fileID, userSlot);
    if ( !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp",
                    3742,
                    0,
                    "Invalid file share location.") )
        __debugbreak();
    return 0;
}

void __cdecl LiveStorage_FileShare_TransferFileFailure(TaskRecord *task)
{
}

void __cdecl LiveStorage_FileShare_TransferFileSuccess(TaskRecord *task)
{
}

TaskRecord *__cdecl LiveStorage_FileShare_WriteRating(
                int controllerIndex,
                unsigned __int64 fileID,
                unsigned __int8 rating)
{
    return 0;
}

void __cdecl LiveStorage_FileShare_WriteRatingFailure(TaskRecord *rec)
{
    Com_PrintError(16, "Could not submit rating.\n");
}

void __cdecl LiveStorage_FileShare_WriteRatingSuccess(TaskRecord *rec)
{
    Com_Printf(16, "Rating successfully submitted.\n");
}

bool __cdecl LiveStorage_FileShare_IsReadingTopRated(int controllerIndex)
{
    return TaskManager2_TaskIsInProgressForController(task_LiveFileShareGetTopRating, controllerIndex);
}

TaskRecord *__cdecl LiveStorage_FileShare_ReadTopRated(
                int controllerIndex,
                int fileType,
                int startIndex,
                int maxFiles,
                bdFileID *fileIDs,
                int *numFiles,
                int *totalNumFiles,
                void (__cdecl *successCallback)(),
                void (__cdecl *failureCallback)())
{
    return 0;
}

void __cdecl LiveStorage_FileShare_ReadTopRatedFailure(TaskRecord *task)
{
    dwFileShareGetTopRatedTask *fsTask; // [esp+0h] [ebp-4h]

    Com_PrintError(16, "Could not get file rating.\n");
    fsTask = (dwFileShareGetTopRatedTask *)task->payload;
    if ( fsTask->failureCallback )
        fsTask->failureCallback();
}

void __cdecl LiveStorage_FileShare_ReadTopRatedSuccess(TaskRecord *task)
{
}

bool __cdecl LiveStorage_FileShare_IsReadingRatings(int controllerIndex)
{
    return TaskManager2_TaskIsInProgressForController(task_LiveFileShareGetRating, controllerIndex);
}

int __cdecl LiveStorage_FileShare_VoteRankComparator(unsigned int *arg0, unsigned int *arg1)
{
    if ( !arg0 && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 3914, 0, "%s", "arg0") )
        __debugbreak();
    if ( !arg1 && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 3915, 0, "%s", "arg1") )
        __debugbreak();
    return arg1[2] - arg0[2];
}

void __cdecl LiveStorage_FileShare_ReadRatingSuccessAction(dwFileShareGetRatingTask *ratingTask)
{
    int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < *ratingTask->outNumRatingsReceived; ++i )
        Live_FileShareCache_AddRating(
            ratingTask->outStatsInfo[i].m_entityID,
            ratingTask->location,
            &ratingTask->outStatsInfo[i]);
    qsort(
        ratingTask->outStatsInfo,
        *ratingTask->outNumRatingsReceived,
        0xC8u,
        (int (__cdecl *)(const void *, const void *))LiveStorage_FileShare_VoteRankComparator);
    if ( ratingTask->successCallback )
        ratingTask->successCallback();
}

TaskRecord *__cdecl LiveStorage_FileShare_ReadRatingsCommunity(
                int controllerIndex,
                bdFileID *fileIDs,
                int numFileIDs,
                fileShareLocation location,
                bdVoteRankStatsInfo *outRatings,
                int *outNumRatings,
                void (__cdecl *successCallback)(),
                void (__cdecl *failureCallback)())
{
    return NULL;
}

TaskRecord *__cdecl LiveStorage_FileShare_ReadRating(
                int controllerIndex,
                dwFileShareGetRatingTask *ratingTask,
                const TaskDefinition *sym)
{
    return 0;
}

void __cdecl LiveStorage_FileShare_ReadRatingFailure(TaskRecord *task)
{
    dwFileShareGetRatingTask *ratingTask; // [esp+0h] [ebp-4h]

    Com_PrintError(16, "\tSearch: Could not get file ratings.\n");
    ratingTask = (dwFileShareGetRatingTask *)task->payload;
    if ( ratingTask->failureCallback )
        ratingTask->failureCallback();
}

void __cdecl LiveStorage_FileShare_ReadRatingSuccess(TaskRecord *task)
{
}

void __cdecl LiveStorage_FileShare_ReadFileRatingSuccess(TaskRecord *task)
{
}

void __cdecl LiveStorage_FileShare_WriteTagsFailure(TaskRecord *task)
{
    unsigned __int64 v1; // rax

    Com_PrintError(16, "Could not set tags for entity %llu\n", *((_QWORD *)task->payload + 2));
    LiveStorage_FileShare_GetFileShareData(FILESHARE_BUFFER_PRIMARY)->valid = 0;
    LODWORD(v1) = Live_GetXuid(task->controllerIndex);
    LiveStorage_FileShare_ReadListing(task->controllerIndex, v1, FILESHARE_BUFFER_PRIMARY);
}

void __cdecl LiveStorage_FileShare_WriteTagsSuccess(TaskRecord *task)
{
    unsigned __int64 v1; // rax

    Com_Printf(16, "Successfully set tags for entity %llu\n", *((_QWORD *)task->payload + 2));
    LiveStorage_FileShare_GetFileShareData(FILESHARE_BUFFER_PRIMARY)->valid = 0;
    LODWORD(v1) = Live_GetXuid(task->controllerIndex);
    LiveStorage_FileShare_ReadListing(task->controllerIndex, v1, FILESHARE_BUFFER_PRIMARY);
}

void __cdecl LiveStorage_ListCustomGameTypesForUser_f()
{
    int i; // [esp+4h] [ebp-Ch]
    int customgametypecount; // [esp+8h] [ebp-8h]
    fileSharePrivateData *fileShareData; // [esp+Ch] [ebp-4h]

    fileShareData = LiveStorage_FileShare_GetFileShareData(FILESHARE_BUFFER_SECONDARY);
    if ( fileShareData->valid )
    {
        customgametypecount = 1;
        for ( i = 0; i < fileShareData->descriptorCount; ++i )
        {
            if ( fileShareData->descriptors[i].m_category == 4 )
                Com_Printf(15, "%i\t\t%s\n", customgametypecount++, fileShareData->descriptors[i].m_fileName);
        }
    }
}

void __cdecl LiveStorage_NewUser(int controllerIndex)
{
    LiveStorage_ClearPlayerStats(controllerIndex);
    LiveStorage_ResetAllFileOps();
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
    controllerNetworkData[controllerIndex].fileOps = &controllerFileOps[controllerIndex];
}

int LiveStorage_ResetAllFileOps()
{
    int result; // eax
    int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < 1; ++i )
    {
        LiveStorage_ResetFileInfo(&controllerFileOps[i].corruptedStatsBackupFileInfo);
        LiveStorage_ResetFileInfo(&controllerFileOps[i].fetchMetPlayerListFileInfo);
        LiveStorage_ResetFileInfo(&controllerFileOps[i].getOtherPlayerStatsFileInfo);
        LiveStorage_ResetFileInfo(&controllerFileOps[i].getPlayerStatsFileInfo);
        LiveStorage_ResetFileInfo(&controllerFileOps[i].getBasicTrainingStatsFileInfo);
        LiveStorage_ResetFileInfo(&controllerFileOps[i].saveMetPlayerListFileInfo);
        LiveStorage_ResetFileInfo(&controllerFileOps[i].saveStatsFileInfo);
        LiveStorage_ResetFileInfo(&controllerFileOps[i].statsBackupFileInfo);
        result = i + 1;
    }
    return result;
}

char __cdecl LiveStorage_Init()
{
    int i; // [esp+0h] [ebp-8h]
    int controllerIndex; // [esp+4h] [ebp-4h]

    if ( !live_service->current.enabled )
        return 0;

    LiveStats_Init();
    LiveCombatRecord_Init();

    for ( controllerIndex = 0; controllerIndex < 1; ++controllerIndex )
        LiveMeetPlayer_Init();

    for ( i = 0; i < 1; ++i )
        LiveStorage_ClearPlayerStats(i);

    Cmd_AddCommandInternal("fakeComError", LiveStorage_FakeComErrorCmd, &LiveStorage_FakeComErrorCmd_VAR);
    Cmd_AddCommandInternal("readStatsBackup", LiveStorage_ReadStatsBackupCmd, &LiveStorage_ReadStatsBackupCmd_VAR);
    Cmd_AddCommandInternal("getServiceRecord", LiveStorage_GetFriendStatsCmd, &LiveStorage_GetFriendStatsCmd_VAR);
    Cmd_AddCommandInternal("backupStats", LiveStorage_WriteBackupStatsCmd, &LiveStorage_WriteBackupStatsCmd_VAR);
    Cmd_AddCommandInternal(
        "restoreStatsFromBackup",
        LiveStorage_RestoreStatsFromBackupCmd,
        &LiveStorage_RestoreStatsFromBackupCmd_VAR);
    Cmd_AddCommandInternal("assertDWConnection", CL_PC_SignIn, &LiveStorage_AssertDWConnectionCmd_VAR);
    Cmd_AddCommandInternal(
        "downloaddemofile",
        LiveStorage_FileShare_DownloadFile_f,
        &LiveStorage_FileShare_DownloadFile_f_VAR);
    Cmd_AddCommandInternal(
        "listcustomgametypes",
        LiveStorage_ListCustomGameTypesForUser_f,
        &LiveStorage_ListCustomGameTypesForUser_f_VAR);
    Cmd_AddCommandInternal("refetchWAD", LiveStorage_RefetchOnlineWAD, &LiveStorage_RefetchOnlineWAD_VAR);
    Cmd_AddCommandInternal("generatePlaylistPopulation", BLOPS_NULLSUB, &LiveStorage_GeneratePopulationDataCmd_VAR);

    stat_version = _Dvar_RegisterInt("stat_version", 10, 0, 0x7FFFFFFF, 0, "Stats version number");
    stats_version_check = _Dvar_RegisterBool("stats_version_check", 1, 0, "Reset stats if version numbers do not match");
    maxStatsBackupInterval = _Dvar_RegisterInt(
                                                         "maxStatsBackupInterval",
                                                         1,
                                                         -1,
                                                         0xFFFF,
                                                         0,
                                                         "Maximum number of days before performing stats backup to Demonware User Storage");
    dwFileFetchTryIntervalBase = _Dvar_RegisterInt(
                                                                 "dwFileFetchTryIntervalBase",
                                                                 2,
                                                                 0,
                                                                 0xFFFF,
                                                                 0,
                                                                 "Delay in seconds after file fetch fails to the next retry.    Exponential growth each retry.");
    dwFileFetchTryIntervalMax = _Dvar_RegisterInt(
                                                                "dwFileFetchTryIntervalMax",
                                                                30,
                                                                0,
                                                                0xFFFF,
                                                                0,
                                                                "Max delay in seconds between file fetch tries.");
    dwFileFetchTryMaxAttempts = _Dvar_RegisterInt(
                                                                "dwFileFetchTryMaxAttempts",
                                                                30,
                                                                0,
                                                                0xFFFF,
                                                                0,
                                                                "Max retry attempts before stopping altogether.");
    clanMessageLastFetchTime = _Dvar_RegisterInt(
                                                             "clanMessageLastFetchTime",
                                                             0,
                                                             0,
                                                             0x7FFFFFFF,
                                                             0,
                                                             "Time in milliseconds of the last clan message file fetch operation");
    eventMessageLastFetchTime = _Dvar_RegisterInt(
                                                                "eventMessageLastFetchTime",
                                                                0,
                                                                0,
                                                                0x7FFFFFFF,
                                                                0,
                                                                "Time in milliseconds of the last event message file fetch operation");
    codMessageLastFetchTime = _Dvar_RegisterInt(
                                                            "codMessageLastFetchTime",
                                                            0,
                                                            0,
                                                            0x7FFFFFFF,
                                                            0,
                                                            "Time in milliseconds of the last cod message file fetch operation");
    waitOnStatsTimeout = _Dvar_RegisterInt(
                                                 "waitOnStatsTimeout",
                                                 15,
                                                 0,
                                                 0x7FFFFFFF,
                                                 0,
                                                 "Time in seconds to wait for stats to be fetched while dev mapping.");
    heatMapLoadDelay = _Dvar_RegisterInt(
                                             "heatMapLoadDelay",
                                             2000,
                                             0,
                                             0x7FFFFFFF,
                                             0,
                                             "Time in milliseconds to delay the loading of the heatmap.");
    minDelayForOtherPlayerStatsFetch = _Dvar_RegisterInt(
                                                                             "minDelayForOtherPlayerStatsFetch",
                                                                             300000,
                                                                             0,
                                                                             0x7FFFFFFF,
                                                                             0,
                                                                             "Delay in milliseconds to refetch the stats of a player.");
    fshSearchTaskDelay = _Dvar_RegisterInt(
                                                 "fshSearchTaskDelay",
                                                 1200,
                                                 0,
                                                 0x7FFFFFFF,
                                                 0,
                                                 "Gap in milliseconds between file share search tasks.");
    Sys_InitDemoStreamingEvent();
    basicTrainingFatal = _Dvar_RegisterBool(
                                                 "basicTrainingFatal",
                                                 1,
                                                 0,
                                                 "If true, a basic training stats error will cause the game to end, if false a warning is printed"
                                                 " to the console and the game continues");
    LiveStorage_Init_Platform();
    Live_FileShare_Init();
    return 1;
}

void __cdecl LiveStorage_WriteBackupStatsCmd()
{
    LiveStorage_WriteBackupStats(0);
}

void __cdecl LiveStorage_ReadStatsBackupCmd()
{
    LiveStorage_ReadStatsBackup(0);
}

void __cdecl LiveStorage_GetFriendStatsCmd()
{
    const char *v0; // eax
    __int64 playerXuid; // [esp+18h] [ebp-8h]

    if ( Cmd_Argc() != 2
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\live\\live_storage.cpp", 4154, 0, "%s", "Cmd_Argc()==2") )
    {
        __debugbreak();
    }
    if ( Cmd_Argc() == 2 )
    {
        v0 = Cmd_Argv(1);
        playerXuid = I_atoi64(v0);
        LiveStorage_ReadOtherPlayerStats(0, playerXuid);
    }
}

void __cdecl LiveStorage_RestoreStatsFromBackupCmd()
{
    LiveStorage_RestoreStatsFromBackup(0);
}

void __cdecl LiveStorage_FakeComErrorCmd()
{
    Live_DelayedComError("No Error");
}

void __cdecl LiveStorage_CheckOngoingSessionTasks()
{
    LiveStorage_CheckOngoingSessionTasks_Platform();
}

#if 0
fileSharePrivateData *__thiscall fileSharePrivateData::fileSharePrivateData(fileSharePrivateData *this)
{
    int v3; // [esp+4h] [ebp-10h]
    bdVoteRankStatsInfo *j; // [esp+8h] [ebp-Ch]
    int v5; // [esp+Ch] [ebp-8h]
    bdFileMetaData *i; // [esp+10h] [ebp-4h]

    v5 = 20;
    for ( i = this->descriptors; --v5 >= 0; ++i )
        bdFileMetaData::bdFileMetaData(i);
    v3 = 20;
    for ( j = this->fileRatings; --v3 >= 0; ++j )
        bdVoteRankStatsInfo::bdVoteRankStatsInfo(j);
    return this;
}

playerFileOperations *__thiscall playerFileOperations::playerFileOperations(playerFileOperations *this)
{
    int v3; // [esp+8h] [ebp-68h]
    bdVoteRankStatsInfo *j; // [esp+Ch] [ebp-64h]
    int v5; // [esp+14h] [ebp-5Ch]
    dwFileShareSubmitRatingTask *i; // [esp+18h] [ebp-58h]

    dwFileTask::dwFileTask(&this->statsBackupFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->readDWEmailFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->saveDWEmailFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->squadMessagesFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->codMessagesFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->eventMessagesFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->hiddenOfferIdsFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->fbStatusFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->clanMOTDInfo.fileTask);
    dwFileTask::dwFileTask(&this->corruptedStatsBackupFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->fetchMetPlayerListFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->saveMetPlayerListFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->getOtherPlayerStatsFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->getPlayerStatsFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->getBasicTrainingStatsFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->saveStatsFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->saveBasicTrainingStatsFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->contractsFileInfo.fileTask);
    dwFileTask::dwFileTask(&this->demoFileInfo.fileTask);
    bdFileMetaData::bdFileMetaData(&this->fileShareReadFileTask.descriptor);
    bdFileID::bdFileID(&this->fileShareWriteFileTask.outFileID);
    bdFileID::bdFileID(&this->fileShareTransferTask.outFileID);
    v5 = 1;
    for ( i = &this->fileShareSubmitRatingTask; --v5 >= 0; i = (dwFileShareSubmitRatingTask *)((char *)i + 24) )
        bdRatingInfo::bdRatingInfo(i->rating);
    v3 = 100;
    for ( j = this->fileShareGetTopRatedTask.statsInfo; --v3 >= 0; ++j )
        bdVoteRankStatsInfo::bdVoteRankStatsInfo(j);
    return this;
}

void __thiscall fileSharePrivateData::~fileSharePrivateData(fileSharePrivateData *this)
{
    int v2; // [esp+4h] [ebp-10h]
    bdVoteRankStatsInfo *j; // [esp+8h] [ebp-Ch]
    int v4; // [esp+Ch] [ebp-8h]
    int *i; // [esp+10h] [ebp-4h]

    v4 = 20;
    for ( i = &this->descriptorCount; --v4 >= 0; bdVoteRankStatsInfo::~bdVoteRankStatsInfo((bdStatsInfo *)i) )
        i -= 50;
    v2 = 20;
    for ( j = this->fileRatings; --v2 >= 0; //bdFileMetaData::~bdFileMetaData(j) )
        j = (bdVoteRankStatsInfo *)((char *)j - 2104);
}

void __thiscall playerFileOperations::~playerFileOperations(playerFileOperations *this)
{
    `vector destructor iterator'(
        (char *)this->fileShareGetTopRatedTask.statsInfo,
        0xC8u,
        100,
        (void (__thiscall *)(void *))bdVoteRankStatsInfo::~bdVoteRankStatsInfo);
    `vector destructor iterator'(
        (char *)&this->fileShareSubmitRatingTask,
        0x18u,
        1,
        (void (__thiscall *)(void *))bdRatingInfo::~bdRatingInfo);
    bdFileID::~bdFileID(&this->fileShareTransferTask.outFileID);
    bdFileID::~bdFileID(&this->fileShareWriteFileTask.outFileID);
    //bdFileMetaData::~bdFileMetaData(&this->fileShareReadFileTask.descriptor);
    bdFileInfo::~bdFileInfo(&this->demoFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->demoFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->contractsFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->contractsFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->saveBasicTrainingStatsFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->saveBasicTrainingStatsFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->saveStatsFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->saveStatsFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->getBasicTrainingStatsFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->getBasicTrainingStatsFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->getPlayerStatsFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->getPlayerStatsFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->getOtherPlayerStatsFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->getOtherPlayerStatsFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->saveMetPlayerListFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->saveMetPlayerListFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->fetchMetPlayerListFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->fetchMetPlayerListFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->corruptedStatsBackupFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->corruptedStatsBackupFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->clanMOTDInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->clanMOTDInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->fbStatusFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->fbStatusFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->hiddenOfferIdsFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->hiddenOfferIdsFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->eventMessagesFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->eventMessagesFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->codMessagesFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->codMessagesFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->squadMessagesFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->squadMessagesFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->saveDWEmailFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->saveDWEmailFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->readDWEmailFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->readDWEmailFileInfo.fileTask.m_fileData);
    bdFileInfo::~bdFileInfo(&this->statsBackupFileInfo.fileTask.m_fileInfo);
    bdFileData::~bdFileData(&this->statsBackupFileInfo.fileTask.m_fileData);
}

#endif