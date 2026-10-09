#include "live_clans.h"
#include <win32/win_gamerprofile.h>

char *__cdecl Clan_GetName(int controllerIndex)
{
    GamerSettingState *settings = GamerProfile_GetProfileSettings(controllerIndex);

    // The macOS/Linux builds have no gamer profile (stubs_online.cpp returns null).
    return settings ? settings->clanPrefix : (char *)"";
}

