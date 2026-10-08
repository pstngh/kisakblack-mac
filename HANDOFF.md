# HANDOFF — native macOS (Apple Silicon) port

Branch `mac-port` of https://github.com/pstngh/kisakblack-mac. Game data:
`/Users/pstn/Documents/Games/codbo` (Steam Black Ops: `main/*.iwd`, `zone/Common`,
`zone/English`).

## Goal

A native arm64 macOS build of the multiplayer executable: no Rosetta, no Wine.
Apple Silicon has no 32-bit mode, so this is also the engine's first 64-bit port.

## Status (2026-10-08)

- Build config: `cmake/macos.cmake` (Homebrew SDL2/GLEW/OpenAL/Speex/libvpx,
  sse2neon for SSE on arm64). Configure + build commands are in README.md.
- 64-bit approach decided and in place; see **docs/64bit.md** (read it first).
  Asset structs keep their 32-bit layout via `Ptr32<T>`; pointers kept in ints go
  through `Ptr32_Encode/Decode` over a 32-bit linear window (image + in-image heap).
- All 732 asset structs and the networked structs match the i386 layout
  (`tools/layout_diff.py`).
- Compile: in progress. ~300 errors were left after the mechanical rewrite and
  are being fixed module by module (script VM, UI, game, physics, renderer).
- Not started: linking, first run, GL core-profile path.

## Next steps (in order)

1. Finish the compile errors; run `tools/check_file.py --i386` over every changed
   file (32-bit builds must still compile).
2. Run `tools/audit_64bit.py build_macos` and fix what it finds; apply the queued
   silent hazards (msg_mp.cpp `_BitScanReverse((unsigned long *)&int)`,
   com_files.cpp `unzGetCurrentFileInfoPosition((unsigned long *)&uint)`,
   q_shared.h `sint32` is `long`).
3. Link. The Linux platform layer (`src/platform/linux`, `src/platform/sdl`) is
   reused; add `SIGBUS` to the crash handler.
4. First run as a dedicated server (`+set dedicated 1 +map mp_nuked`): exercises
   the fastfile loader and the script VM without graphics.
5. GL: macOS has GL 4.1 core only. Use the WebGL2 path's choices where they are
   about core GL (GLSL `in/out` dialect with a `#version 410 core` preamble,
   alpha test via discard uniforms instead of `glAlphaFunc`, no
   `GL_LUMINANCE_ALPHA` -> RG8 + swizzle), keep browser-only code under
   `__EMSCRIPTEN__`. Request a 4.1 core forward-compatible context in
   `glcontext_sdl.cpp`.
6. Client run to the main menu, then a local match.
7. Runtime hardening: AddressSanitizer build; redzones in the region allocators.

## Decisions

- **Ptr32 + linear window** instead of retyping the engine to native 64-bit
  pointers: the decompiled code stores pointers in ints in thousands of places,
  and bytecode/assets/network rely on 4-byte pointers. Retyping would need a
  per-site rewrite of the script compiler, VM and loaders; the window keeps their
  32-bit arithmetic valid.
- **Heap inside the executable image** (1.5 GB zero-fill array): globals and heap
  then share one linear window. ~1.9 GB is the most macOS will load below the
  dyld shared cache; 2 GB fails.
- Networked structs keep their exact layout (checked with layout_diff), so that
  64-bit clients can stay wire-compatible with 32-bit Windows/Linux builds. Not
  tested yet: other messages may still send raw structs.
