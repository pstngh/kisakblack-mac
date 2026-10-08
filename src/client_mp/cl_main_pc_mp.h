#pragma once
#include "cl_main_mp.h"

struct serverStatusInfoResponse_s // sizeof=0x402C
{                                       // XREF: .data:serverStatusInfoResponse_s * cl_serverStatusScoreBoardList/r
                                        // .data:serverStatusInfoResponse_s * cl_serverStatusList/r ...
    char string[16384];
    netadr_t address;                   // XREF: CL_ServerStatus(char *,char *,int)+39/w
                                        // CL_ServerStatusScoreBoard(char *,char *,int)+39/w
    int time;
    int startTime;                      // XREF: CL_GetServerStatus(XNKID *)+C6/r
                                        // CL_GetServerStatus(XNKID *)+E0/r ...
    int pending;
    int print;
    int retrieved;                      // XREF: CL_GetServerStatus(XNKID *)+73/r
                                        // CL_GetServerStatusScoreBoard(XNKID *)+73/r ...
    XNKID secId;                 // XREF: CL_GetServerStatus(XNKID *)+27/o
                                        // CL_GetServerStatusScoreBoard(XNKID *)+27/o ...
};

struct MatchMakingInfo;

netadr_t *__cdecl CL_GetLastRconAddress();
void __cdecl CL_SetServerInfo(serverInfo_t *server, char *info, __int16 ping);
void __cdecl CL_ServerInfoPacket(XNKID *secID, msg_t *msg, int time);
void __cdecl CL_Connect_f();
void __cdecl CL_InitServerInfo(serverInfo_t *server, netadr_t adr);
int __cdecl CL_RawPingSetupBuffer(
                unsigned __int8 *buffer,
                int buffersize,
                unsigned __int8 opcode,
                const XNKID *secID);
void __cdecl CL_RconInit();
void __cdecl CL_Rcon_f();
void CL_RconLogin();
void CL_RconLogout();
void CL_RconHost();
serverStatusInfoResponse_s *__cdecl CL_GetServerStatus(XNKID *secID);
serverStatusInfoResponse_s *__cdecl CL_GetServerStatusScoreBoard(XNKID *secID);
int __cdecl CL_ServerStatus(char *serversecurityID, char *serverStatusString, int maxLen);
int __cdecl CL_ServerStatusScoreBoard(char *serversecurityID, char *serverStatusString, int maxLen);
void __cdecl CL_ServerStatusScoreBoardResponse(msg_t *msg, XNKID *secID);
void __cdecl CL_ServerStatusResponse(msg_t *msg, XNKID *secID);
void __cdecl CL_ResetPlayerMuting(unsigned int muteClientIndex);
void __cdecl CL_MutePlayer(int localClientNum, unsigned int muteClientIndex);
bool __cdecl CL_IsPlayerMuted(int localClientNum, unsigned int muteClientIndex);
void __cdecl CL_ClearMutedList();
void __cdecl CL_FinishMotdDownload();
void __cdecl CL_WWWDownload();
void __cdecl CL_Platform_RegisterCommands();
void __cdecl CL_PC_RequireLiveSignin();
void __cdecl CL_Prestige_f();
char __cdecl CL_PrestigeRequest();
char *__cdecl CL_LongNameForShortName(const char *shortname);

void __cdecl CL_PC_SignIn();

