#pragma once
#include "live_storage.h"

enum EUserTier : __int32
{                                       // XREF: XenonUserData/r
                                        // GetIsSuperUser/r
    USER_TIER_NONE      = 0x0,
    USER_TIER_SUPER     = 0x1,
    USER_TIER_DEVELOPER = 0x2,
    USER_TIER_FRIEND    = 0x3,
    USER_TIER_ENEMY     = 0x29A,
};

struct XenonUserData // sizeof=0x50
{
    int signinState;
    char gamertag[32];
    // padding byte
    // padding byte
    // padding byte
    // padding byte
    unsigned __int64 xuid;
    char xuidString[17];
    bool isGuestUser;
    // padding byte
    // padding byte
    EUserTier tier;
    __int64 totalGamesPlayed;
};

char __cdecl Live_ContentRatingAllowed();
bool __cdecl Live_IsUserSignedInToLive();
char __cdecl Live_RequireUserToPlayOnline();
char *__cdecl Live_ControllerIndex_GetClientName(int controllerIndex);
int __cdecl CL_ControllerIndex_GetSignInState(int controllerIndex);
void __cdecl Live_ToggleMute_f();
void __cdecl Live_InitPlatform();
int __cdecl Live_GetControllerFromXUID(unsigned __int64 player);
void __cdecl Live_GiveAchievement(int localControllerIndex, const char *achievementName);
bool __cdecl Live_IsInLiveGame();
char __cdecl XUserGetXUID(int controllerIndex, unsigned __int64 *xuid);
char __cdecl Live_UserGetName(int controllerIndex, char *buf, int bufsize);
bool __cdecl Live_UserSignedInLocally(int controllerIndex, char **disconnectMessage);
void __cdecl Live_UserSignedIn(int controllerIndex);
bool __cdecl Live_UserSignedInToLive(int controllerIndex, char **disconnectMessage);
bool __cdecl Live_IsSignedIn(int controllerIndex);
bool __cdecl Live_IsSignedInToLive();
unsigned long long __cdecl Live_GetXuid(int controllerIndex);
int __cdecl Live_GetTier(int controllerIndex);
char __cdecl Live_ShowMarketplaceUI();
void __cdecl PC_InitSigninState();
bool __cdecl Live_UserSignedOut(int controllerIndex);
void __cdecl Live_DelayedComError(const char *comErrorString);
void __cdecl Live_Frame();
char __cdecl Live_IsUserBlocked(int controllerIndex, unsigned __int64 xuid);
bool __cdecl Live_CanConsoleViewContentFromUser(unsigned __int64 xuid);
bool __cdecl Live_CanViewContentFromUser(int controllerIndex, unsigned __int64 xuid);


extern unsigned __int64 s_selectedPlayerXUID;
extern unsigned __int64 s_selectedMetPlayerXUID;


extern const dvar_t *live_service;
extern const dvar_t *pc_newversionavailable;
extern const dvar_t *xblive_loggedin;
extern const dvar_t *xenon_voiceDebug;
extern const dvar_t *xenon_voiceDegrade;
extern const dvar_t *getdlcmapsfrommaindrive;
extern const dvar_t *session_nonblocking;
extern const dvar_t *systemUiActive;
extern const dvar_t *xblive_showmarketplace;
extern const dvar_t *xblive_clanmatch;
extern const dvar_t *xblive_theater;
extern const dvar_t *xblive_hostingprivateparty;
extern const dvar_t *xblive_privatepartyclient;
extern const dvar_t *xblive_wagermatch;
extern const dvar_t *xblive_basictraining;
extern const dvar_t *xblive_basictraining_popup;
extern const dvar_t *bot_tips;
extern const dvar_t *xblive_clanListChanged;
extern const dvar_t *teamsplitter_verbose;
extern const dvar_t *xblive_matchEndingSoon;
extern const dvar_t *ui_isClanMember;
extern const dvar_t *xenon_maxVoicePacketsPerSec;
extern const dvar_t *xenon_maxVoicePacketsPerSecForServer;
extern const dvar_t *xblive_mappacks;
extern const dvar_t *bot_friends;
extern const dvar_t *bot_enemies;
extern const dvar_t *steamid;
extern const dvar_t *scr_bot_difficulty;
extern const dvar_t *clancard_clanid;
extern const dvar_t *clanName;
extern const dvar_t *bot_difficulty;
