#pragma once

#include <win32/win_local.h>
#include <windows.h>
#include <qcommon/common.h>

bool __cdecl PC_StartWithNoSounds();
void __cdecl Sys_GetInfo(SysInfo *info);
bool __cdecl Sys_HasConfigureChecksumChanged(int checksum);
bool __cdecl Sys_ShouldUpdateForConfigChange();
void Sys_RegisterInfoDvars();
bool __cdecl Sys_HasInfoChanged();
bool __cdecl Sys_ShouldUpdateForInfoChange();
void __cdecl Sys_ArchiveInfo(int checksum);
void __cdecl    Sys_DirectXFatalError();
void __cdecl    Sys_OutOfMemErrorInternal(const char *filename, int line);
void __cdecl Sys_QuitAndStartProcess(const char *exeName);
void __cdecl Sys_OpenURL(const char *url, int doexit);
void    Sys_Error(char *error, ...);
void __cdecl    Sys_Quit();
void Sys_SpawnQuitProcess();
void __cdecl Sys_Print(char *msg);
char *__cdecl Sys_GetClipboardData();
void __cdecl Sys_QueEvent(unsigned int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr);
void Sys_ShutdownEvents();
void __cdecl Sys_LoadingKeepAlive();
sysEvent_t *__cdecl Win_GetEvent(sysEvent_t *result);
sysEvent_t *__cdecl Sys_GetEvent(sysEvent_t *result);
void __cdecl Sys_Mjpeg();
void __cdecl Sys_MjpegClose();
void __cdecl Sys_Init();
void __cdecl Sys_In_Restart_f();
void __cdecl Sys_Net_Restart_f();
int __cdecl Sys_CheckCrashOrRerun();
void    Sys_NoFreeFilesError();
int __cdecl Sys_IsGameProcess(unsigned int id);
void __cdecl Sys_NormalExit();
int __cdecl PrivateUnhandledExceptionFilter(_EXCEPTION_POINTERS *ExceptionInfo);
int __stdcall WinMain(HINSTANCE__ *hInstance, HINSTANCE__ *hPrevInstance, char *lpCmdLine, int nCmdShow);
void Sys_FindInfo();
int Sys_GetSemaphoreFileName();
void Win_RegisterClass();
void PrintWorkingDir();
char __cdecl CheckRemoteSession();
bool __cdecl StartingDedicatedServer(char *cmdline);

extern const dvar_t *sys_configureGHz;
extern const dvar_t *sys_sysMB;
extern const dvar_t *sys_gpu;
extern const dvar_t *sys_configSum;
extern const dvar_t *sys_SSE;

extern bool g_allowMature;
