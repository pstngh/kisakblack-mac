#include "live_storage_pub.h"
#include <stringed/stringed_hooks.h>
#include <cgame_mp/cg_main_mp.h>
#include <universal/com_files.h>
#include <universal/mem_largelocal.h>
#include "live_ticker.h"
#include <ui/ui_playlists.h>
#include "live_contracts.h"
#include "live_storage_win.h"
#include <zlib/zlib.h>

bool s_haveContracts;
bool s_havePlaylists;

bool __cdecl LiveStorage_DoWeHaveContracts()
{
    return s_haveContracts;
}

bool __cdecl LiveStorage_DoWeHavePlaylists()
{
    return s_havePlaylists;
}

void __cdecl LiveStorage_SetHavePlaylists(bool val)
{
    s_havePlaylists = val;
}

