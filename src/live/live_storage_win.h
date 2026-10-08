#pragma once
#include "live_storage.h"


int __cdecl SystemTimeToInt();
void __cdecl LiveStorage_ResetStats(unsigned __int8 *buffer);
void __cdecl LiveStorage_ReadStats(int __formal, bool validate, bool silent);
void LiveStorage_InitCustomClassesNames();
void __cdecl LiveStorage_ReadStatsIfDirChanged();
void __cdecl LiveStorage_UploadStats();
void __cdecl LiveStorage_UploadStatsForController();
void __cdecl CL_GetXP_f();
void __cdecl LiveStorage_Init_Platform();
void __cdecl LiveStorage_FetchPlaylistsLocal(int controllerIndex);
bool __cdecl SV_GetStatFromBlob(char *buffer, const char *stat, int *outInt);
bool __cdecl SV_GetIntClientStatByGameMode(
                unsigned int clientNum,
                int *outInt,
                const char *gameMode,
                const char *statName);
bool __cdecl SV_GetClientDDLStat(unsigned int clientnum, const char *stat, int *outInt);
int __cdecl LiveStorage_GetMatchesPlayed(const char *gameModePrefix, int resetPeriod);
char __cdecl SV_GetIntClientStatMatchDeltaByGameMode(
                int clientNum,
                int *outInt,
                const char *gameMode,
                const char *statName);
int __cdecl SV_GetTotalMatchesPlayedByGameModeForClient(unsigned int clientNum, const char *gameModePrefix);
void __cdecl SV_SetPlaylistFetchedTime();
int __cdecl SV_GetPlaylistFetchedTime();
void __cdecl SV_SetShouldMapRotate(bool should);
bool __cdecl SV_ShouldMapRotate();
bool __cdecl LiveStorage_FirstTimeRunning();
void __cdecl LiveStorage_SetFirstTimeRunning(bool running);


extern const dvar_t *stats_backup;
extern const dvar_t *presell;
extern const dvar_t *sv_playlistFetchInterval;