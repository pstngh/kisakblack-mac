// linux_main.cpp — the Linux entry point + the remaining platform stubs that pull
// the whole engine together into a binary. main() mirrors win32/win_main.cpp's WinMain
// init sequence (critical sections -> main thread/TLS -> Com_Init -> Com_Frame loop).
// The gamepad, worker-thread and streaming symbols are stubbed so
// the engine links and boots; these are progressively replaced as bring-up continues.
#include <qcommon/common.h>
#include <qcommon/threads.h>
#include <qcommon/tl_support.h>
#include <win32/win_main.h>
#include <win32/win_common.h>
#include <win32/win_gamepad.h>
#include <win32/win_workercmds.h>
#include <win32/win_stream.h>
#include <demo/demo_common.h>
#include <physics/phys_broad_phase.h>

#include <cstring>
#include <cstdio>
#include <csignal>
#include <execinfo.h>
#include <cstdlib>

// Crash backtrace — the engine's asserts trap via __debugbreak (SIGILL); print where.
static void CrashHandler(int sig) {
    void *bt[40]; int n = backtrace(bt, 40);
    fprintf(stderr, "\n*** caught signal %d — backtrace (%d frames) ***\n", sig, n);
    backtrace_symbols_fd(bt, n, 2);
    fflush(stderr); _exit(128 + sig);
}

// ---- Gamepad: no controller (SDL game-controller support is future work) ----
const dvar_t *gpad_enabled = nullptr;
bool   GPad_IsActive(int) { return false; }
double GPad_GetButton(int, GamePadButton) { return 0.0; }
double GPad_GetStick(int, GamePadStick) { return 0.0; }
bool   GPad_IsButtonPressed(int, GamePadButton) { return false; }
bool   GPad_IsStickPressed(int, GamePadStick, GamePadStickDir) { return false; }
bool   GPad_IsStickReleased(int, GamePadStick, GamePadStickDir) { return false; }


// ---- Streaming (worker threads now in linux_workercmds.cpp) ------------------
// R_InitWorkerThreads, IW_task_manager_*, and the nuge_physics job module are the
// real job-queue bring-up, ported to src/platform/linux/linux_workercmds.cpp.
char Stream_Init() { return 1; }
bool PC_StartWithNoSounds() { return false; }

// ---- vtable/typeinfo anchor for a decompiled polymorphic struct -------------
// Defining the class's key virtual out-of-line makes the compiler emit its vtable
// and typeinfo here (otherwise nothing in the build emits them).
void broad_phase_terrain_query_callback::query(const broad_phase_environment_query_input *, broad_phase_environement_query_results *) {}

// ---- Entry point -----------------------------------------------------------
int main(int argc, char **argv) {
    char cmdline[2048] = {0};
    for (int i = 1; i < argc; ++i) {
        size_t n = strlen(cmdline);
        snprintf(cmdline + n, sizeof(cmdline) - n, "%s%s", argv[i], i + 1 < argc ? " " : "");
    }
    fprintf(stderr, "[KisakBlack] boot: cmdline=\"%s\"\n", cmdline);
    signal(SIGILL, CrashHandler); signal(SIGSEGV, CrashHandler); signal(SIGABRT, CrashHandler);

    Sys_InitializeCriticalSections();
    Sys_InitMainThread();
    Sys_SetupTLCallbacks(0x900000);
    Com_Init(cmdline);

    for (;;) Com_Frame();
    return 0;
}
