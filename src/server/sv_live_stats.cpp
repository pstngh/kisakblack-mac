#include <server_mp/sv_main_mp.h>

#include "sv_live_stats.h"
#include <qcommon/common.h>

unsigned __int64 __cdecl SV_GetPlayerXuid(unsigned int clientNum)
{
    if ( clientNum >= com_maxclients->current.integer
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\server_mp\\sv_live_stats.cpp",
                    64,
                    0,
                    "clientNum doesn't index com_maxclients->current.integer\n\t%i not in [0, %i)",
                    clientNum,
                    com_maxclients->current.integer) )
    {
        __debugbreak();
    }
    return svs.clients[clientNum].userID;
}

