// sdl_display.h — display queries shared by the Win32 monitor shim (sdl_window.cpp)
// and the GL backend's adapters (one per SDL display).
#ifndef KISAK_SDL_DISPLAY_H
#define KISAK_SDL_DISPLAY_H

// Start SDL video if it isn't yet (on the main thread, as macOS requires). The
// renderer asks for monitors and display modes before it creates the window.
void Sys_EnsureSDLVideo();

// Pixels per point of a display: 2 on a Retina display, else 1.
int Sys_DisplayPixelDensity(int display);

// The display the game window is on, which the GL backend notes as it changes
// (MonitorFromWindow answers with it).
void Sys_NoteGameWindowDisplay(int display);

#endif // KISAK_SDL_DISPLAY_H
