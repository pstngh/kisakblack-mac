#include "live_win_common.h"
#include <qcommon/com_clients.h>
#include "live_win.h"
#include <DW/dwNet.h>
#include "live_groups_dw.h"
#include <DW/MatchRecorder.h>
#include "live_leaderboard.h"
#include "live_sessions_win.h"
#include "live_news.h"
#include "live_counter.h"
#include "live_pcache.h"
#include <client_mp/cl_main_pc_mp.h>
#include <win32/win_shared.h>
#include "live.h"
#include <qcommon/common.h>
#include <universal/assertive.h>
#include <qcommon/threads.h>

#include <DW/dwLogOn_pc.h>
#include <DW/MatchMakingInfo_win32.h>

const char *s_comErrorString;
bool s_shouldComError;

bool dw_disconnect_detected;

int sessiontimer;

extern const dvar_t *live_service; // KISAKTODO: remove these later
extern const dvar_t *dw_dupe_key;
char __cdecl Live_Frame_MP(int localControllerIndex)
{
return 0;
}

extern const dvar_t *dw_popup; // KISAKTODO: remove later
void __cdecl Live_UpdateUiPopup(const char *popupname, bool reset)
{
    int v2; // eax

    if ( cls.uiStarted )
    {
        Com_Printf(16, "Live_UpdateUiPopup: %s\n", popupname);
        if ( reset )
            v2 = I_stricmp(dw_popup->current.string, "");
        else
            v2 = I_stricmp(dw_popup->current.string, popupname);
        if ( v2 )
        {
            if ( reset )
                Dvar_SetString((dvar_s *)dw_popup, "");
            else
                Dvar_SetString((dvar_s *)dw_popup, popupname);
        }
    }
    else
    {
        Com_Printf(16, "%s\n", popupname);
    }
}

void __cdecl Live_ShowPlayerProfile(int localClientNum, unsigned __int64 playerXUID, char *playerName)
{
    int ControllerIndex; // eax
    __int64 v4; // rax
    char *v5; // eax
    uiInfo_s *uiInfo; // [esp+8h] [ebp-8h]
    menuDef_t *menu; // [esp+Ch] [ebp-4h]

    ControllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
    LODWORD(v4) = Live_GetXuid(ControllerIndex);
    if ( playerXUID && playerXUID != v4 )
    {
        v5 = va("%11d", (unsigned int)playerXUID);
        Dvar_SetStringByName("selectedPlayerXuid", v5);
        Dvar_SetStringByName("selectedFriendName", playerName);
        uiInfo = UI_GetInfo(localClientNum);
        menu = Menus_FindByName(&uiInfo->uiDC, "menu_playercard");
        if ( menu )
        {
            menu->openSlideDirection = 2;
            Menus_Open(localClientNum, &uiInfo->uiDC, menu);
        }
    }
}

void __cdecl SocketRouter_EmergencyFrame()
{
    Sys_IsMainThread();
}

