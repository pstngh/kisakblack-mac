#pragma once
#include <client_mp/cl_main_mp.h>

struct XSESSION_INFO // sizeof=0x31
{                                       // XREF: SessionData_s/r
                                        // LaunchData/r ...
    XNKID sessionID;
    XNADDR hostAddress;
    XNKEY keyExchangeKey;
};

struct RegisteredUser // sizeof=0x28
{                                       // XREF: SessionData_s/r
    bool active;
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    unsigned __int64 xuid;
    netadr_t addr;
    bool privateSlot;
    // padding byte
    // padding byte
    // padding byte
    int performanceValue;
};

struct __declspec(align(4)) RemoteTalkerInfo // sizeof=0x20
{                                       // XREF: SessionData_s/r
    bool registered;
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    unsigned __int64 xuid;
    int natType;
    int voiceConnectivityBits;
    int lastConnectivityTestTime[1];
    bool muted;
    // padding byte
    // padding byte
    // padding byte
};

struct LocalTalkerInfo // sizeof=0x1
{                                       // XREF: SessionData_s/r
    bool enabled;
};

struct __declspec(align(8)) SessionData_s // sizeof=0x978
{                                       // XREF: .data:SessionData_s g_serverSession/r
                                        // .data:SessionData_s g_partySession/r ...
    char *sessionName;                  // XREF: Session_Init(void)+38/w
    bool sessionHandle;                 // XREF: SV_SpawnServer(int,char const *,int,int)+690/r
                                        // Live_IsInLiveGame(void)+4/r
    XSESSION_INFO sessionInfo;          // XREF: _dynamic_initializer_for__g_serverSession__+4/o
                                        // _dynamic_initializer_for__g_partySession__+4/o ...
    bool keysGenerated;
    bool sessionStartCalled;
    bool registerUsersWithVoice;        // XREF: Session_Init(void)+42/w
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    unsigned __int64 sessionNonce;
    int privateSlots;                   // XREF: Live_Frame_MP(int,int)+1FB/r
    int publicSlots;                    // XREF: Live_Frame_MP(int,int)+202/r
    int flags;
    struct PS3VoiceEngine *voiceEngine;
    bool qosListenEnabled;
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    RegisteredUser registeredUsers[32];
    RemoteTalkerInfo remoteTalkers[32];
    LocalTalkerInfo localTalkers[4];
    int voiceConnectivityBits;
    int sessionCreateController;
    int ackedMeetBits;
    int lastMeetSendTime;
    // padding byte
    // padding byte
    // padding byte
    // padding byte
};

int __cdecl Session_FindRegisteredUser(const SessionData_s *session, unsigned __int64 xuid);
void __cdecl Session_EndGameSession(SessionData_s *session);
void __cdecl Session_DeleteSession(SessionData_s *session);
void __cdecl Session_DeleteHandle(bool *sessionHandle);
void __cdecl Session_UnregisterAllUsersFromVoice(SessionData_s *session);
void __cdecl Session_StartHost(SessionData_s *session, int sessionFlags, int numPrivateSlots, int numPublicSlots);
void __cdecl Session_Modify(
                int localControllerIndex,
                SessionData_s *session,
                int flags,
                int publicSlots,
                int privateSlots);
void __cdecl Session_Init();

extern SessionData_s g_serverSession;
