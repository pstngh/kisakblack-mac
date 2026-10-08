#pragma once

#include <demo/demo_common.h>

enum statsLocation : __int32
{                                       // XREF: ?LiveStorage_GetStatsBuffer@@YAPAEHW4statsLocation@@_N@Z/r
                                        // LiveStorage_GetPersStatsBuffer/r ...
    STATS_LOCATION_NORMAL        = 0x0,
    STATS_LOCATION_FORCE_NORMAL  = 0x1,
    STATS_LOCATION_BACKUP        = 0x2,
    STATS_LOCATION_STABLE        = 0x3,
    STATS_LOCATION_OTHERPLAYER   = 0x4,
    STATS_LOCATION_BASICTRAINING = 0x5,
    STATS_LOCATION_GLOBAL        = 0x6,
    STATS_LOCATION_GLOBALSTABLE  = 0x7,
    STATS_LOCATION_COUNT         = 0x8,
};

struct persistentStats // sizeof=0x996C
{                                       // XREF: .data:s_otherPlayerStats/r
    unsigned __int8 statsBuffer[39272];
    bool isChecksumValid;
    bool statsWriteNeeded;
    bool statsValidatedWithDDL;
    bool statsFetched;
};

struct playerNetworkData // sizeof=0x3AD90
{                                       // XREF: .data:controllerNetworkData/r
    _BYTE playerStats[40172];
    _BYTE playerStatsBackup[40172];     // XREF: LiveStorage_GetPersStatsBuffer+81/o
    _BYTE stableStatsBuffer[40172];     // XREF: LiveStorage_GetPersStatsBuffer+94/o
    _BYTE basicTrainingStats[40172];    // XREF: LiveStorage_GetPersStatsBuffer+B1/o
                                        // LiveStorage_GetPersStatsBuffer+117/o
    _BYTE globalplayerStats[40172];     // XREF: LiveStorage_GetPersStatsBuffer+C5/o
    _BYTE globalStablePlayerStats[40172];
                                        // XREF: LiveStorage_GetPersStatsBuffer+D8/o
    bool firstTimeRunning;              // XREF: LiveStorage_PlayerStatsFileNotFound+49/w
                                        // LiveStorage_ReadPlayerStatsSuccessful+79/w ...
    // padding byte
    // padding byte
};

persistentStats *__cdecl LiveStorage_GetStatsBuffer(
                int controllerIndex,
                statsLocation playerStatsLocation,
                bool verifyLocation);
persistentStats *__cdecl LiveStorage_GetPersStatsBuffer(
                int controllerIndex,
                statsLocation playerStatsLocation,
                bool verifyLocation);
void __cdecl LiveStorage_VerifyCorrectStats(persistentStats *stats, statsLocation location);
void __cdecl LiveStorage_CorrectStatsError(char *msg);
int __cdecl LiveStorage_GetStatsBufferSize();
unsigned __int8 __cdecl LiveStorage_GetStatsChecksumValid(int controllerIndex, statsLocation playerStatsLocation);
void __cdecl LiveStorage_SetStatsChecksumValid(int controllerIndex, statsLocation playerStatsLocation, bool isValid);
bool __cdecl LiveStorage_GetStatsWriteNeeded(int controllerIndex, statsLocation location);
void __cdecl LiveStorage_SetStatsWriteNeeded(int controllerIndex, bool isWriteNeeded, statsLocation location);
int __cdecl LiveStorage_ValidateWithDDL(int controllerIndex, statsLocation location);
unsigned __int8 __cdecl LiveStorage_AreStatsDDLValidated(int controllerIndex, statsLocation playerStatsLocation);
void __cdecl LiveStorage_SetStatsDDLValidated(
                int controllerIndex,
                statsLocation playerStatsLocation,
                bool statsValidatedWithDDL);
unsigned __int8 __cdecl LiveStorage_DoWeHaveStats(int controllerIndex, statsLocation playerStatsLocation);
unsigned __int8 __cdecl LiveStorage_DoWeHaveCurrentStats(int controllerIndex);
bool __cdecl LiveStorage_DoWeHaveAllStats(int controllerIndex);
void __cdecl LiveStorage_SetStatsFetched(int localControllerIndex, statsLocation playerStatsLocation, bool isFetched);
void __cdecl LiveStorage_RestoreStatsFromBackup(int localControllerIndex);
__int64 __cdecl LiveStorage_GetUTC();
int __cdecl LiveStorage_GetUTCOffset();
bool __cdecl LiveStorage_IsTimeSynced();
void __cdecl LiveStorage_SetAllStatsNotFetched(int controllerIndex);
void __cdecl LiveStorage_NewUser(int controllerIndex);
void __cdecl LiveStorage_ClearPlayerStats(int controllerIndex);
char __cdecl LiveStorage_Init();
void __cdecl LiveStorage_RestoreStatsFromBackupCmd();


extern const dvar_t *stat_version;
extern const dvar_t *stats_version_check;
extern const dvar_t *waitOnStatsTimeout;
