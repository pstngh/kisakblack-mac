// sdl_mainthread.h — work that has to run on the process's main thread.
//
// macOS only lets the main thread initialise SDL video and create windows and GL
// contexts (Cocoa), but the renderer creates its device on the render thread.
// Sys_RunOnMainThread() posts such work and blocks until the main thread has run
// it; the main thread runs posted work from its event pump (Sys_PumpSDLEvents), its
// idle waits (NET_Sleep, e.g. R_BeginRegistration waiting for the render thread) and
// its waits on events and semaphores (win_kernel.cpp, e.g. vid_restart waiting for
// the device reset that resizes the window). Elsewhere, and when called on the main
// thread, it runs the work directly.
#ifndef KISAK_SDL_MAINTHREAD_H
#define KISAK_SDL_MAINTHREAD_H

void Sys_RunOnMainThread(void (*fn)(void *), void *arg);

// Main thread: run work posted by other threads. Any other thread: no-op.
void Sys_ServiceMainThreadWork();

#endif // KISAK_SDL_MAINTHREAD_H
