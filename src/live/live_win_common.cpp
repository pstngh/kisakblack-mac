#include "live_sessions_win.h"
#include "live_win_common.h"
#include <qcommon/com_clients.h>
#include "live_win.h"
#include "live_news.h"
#include "live_pcache.h"
#include <client_mp/cl_main_pc_mp.h>
#include <win32/win_shared.h>
#include "live.h"
#include <qcommon/common.h>
#include <universal/assertive.h>
#include <qcommon/threads.h>

void __cdecl Live_ShowPlayerProfile(int localClientNum, unsigned __int64 playerXUID, char *playerName)
{
    int ControllerIndex; // eax
    __int64 v4; // rax
    char *v5; // eax
    uiInfo_s *uiInfo; // [esp+8h] [ebp-8h]
    menuDef_t *menu; // [esp+Ch] [ebp-4h]

    ControllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
    v4 = Live_GetXuid(ControllerIndex);
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

