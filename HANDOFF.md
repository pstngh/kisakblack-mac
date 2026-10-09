# HANDOFF — native macOS (Apple Silicon) port

Branch `mac-port` of https://github.com/pstngh/kisakblack-mac. Local checkout:
`/Users/pstn/Documents/kisakblack-mac`. Game data: `/Users/pstn/Documents/Games/codbo`
(Steam Black Ops: `main/*.iwd`, `zone/Common`, `zone/English`). Build, run and
debugging commands are in CLAUDE.md; the 64-bit design and rules in docs/64bit.md.

## Goal

A native arm64 macOS build of the multiplayer executable: no Rosetta, no Wine.
Apple Silicon has no 32-bit mode, so this is also the engine's first 64-bit port.

## Status (2026-10-08, end of session 3)

- **Builds and links**: `build_macos/blackops`, Mach-O arm64. Every changed file
  passes the i386 syntax check. Sessions 2 and 3 are **uncommitted** in the working
  tree (last commit: "the dedicated server boots and runs its main loop").
- **Dedicated server** with 4 bots: deaths, killstreaks, ragdolls, no DDL error (the
  stats buffer now matches the data, see Decisions).
- **Client main menu draws correctly** at full texture detail (picmip 0, S3TC
  detected, autoconfigure picks the "4 GHz / 640 MB" row like Windows).
- **Local match** (`+map mp_nuked` without `dedicated`, 4 bots, see CLAUDE.md):
  loads, spawns the player, runs 150 s without crashing at ~100 fps (M4, cg 9 ms).
  HUD, minimap, scoreboard, team menu and bot name tags draw. **The 3D world does
  not**: the screen shows the clear colour (`r_clear` "blink" alternates
  r_clearColor blue / r_clearColor2 orange) run through the post effects.
- Diagnostics added: `KB_SCREENSHOT=<dir>` (+`KB_SCREENSHOT_EVERY=n`) writes the back
  buffer as TGA every n presents and prints per-interval draw counters
  (draws/builtinFall/skipPending/links); `KB_TRACEFRAME=n1,n2,..` logs the
  SetRenderTarget/SetViewport/Clear/StretchRect/draw calls of those frames and, with
  KB_SCREENSHOT set, dumps the back buffer at the first 3 resolves
  (`resolve_N.tga`). Convert TGAs with any tool that ignores alpha.

## Next steps (in order)

1. **3D world invisible.** What is known:
   - At the first resolve after the main scene (`resolve_0.tga`) the sky is drawn,
     but every opaque world/model pixel is still the clear colour: the depth
     pre-pass (COLORWRITEENABLE 0) lands, the lit pass (cw=0xf, ZFUNC LESSEQUAL,
     sane matrices in c0-c3, full viewport, depth range 0.015625..1) leaves no
     colour. `KB_NOPREPASS=1` and `r_fullbright 1` don't change it.
   - The final frame is the clear colour after tone mapping, so the post chain also
     loses the sky.
   - Open question from the last run: with `KB_TRACEFRAME` the draw trace placed in
     DrawIndexedPrimitive's immediate path (`if (!g_kbBatchEnable)`) printed nothing
     in frames that clearly drew (an earlier placement before `useDrawProgram()` did
     print). Find out which path native draws take (a probe that printed
     `g_kbBatchEnable` at function entry failed to compile: "undeclared" at that
     point although `int g_kbBatchEnable` is defined near the top of
     gl_d3d9_draw.cpp; check for a shadowing macro/namespace). Then compare GL state
     at a lit draw (the trace prints depth func/range, colour mask, blend, program).
   - Suspects: alpha/discard (the translator emits the in-shader alpha test only for
     ES; core profile has no fixed-function alpha test), blend state, pixel-shader
     output/MRT naming in the desktop GLSL path, the `invariant gl_Position` pairing
     between pre-pass and lit shaders.
2. HUD icon at the bottom right (the weapon/ammo area) draws the font atlas.
3. devgui asserts in a match: `DevGui_FindMenu` gets handle 29554 (0x7372, "rs":
   string bytes read as a handle) from `DevGui_AddDvar` (39x), then
   `parentMenu->childType == DEV_CHILD_MENU` (9x).
4. Redzones in the region allocators (`universal/ptr32.cpp`) so ASan sees heap
   overflows inside the in-image heap.
5. Performance pass: alignment checks that call `Ptr32_Encode` on stack or malloc
   pointers take the handle-table mutex (grep `Ptr32_Encode(.*) & 0x`).
6. Later: input in a match, wire compatibility with a Windows/Linux server, an .app
   bundle, Retina (SDL_WINDOW_ALLOW_HIGHDPI), controller support.

## Open items found but not fixed

Seen in session 2, unverified against a Windows run:
- 75x "Field lerp.u.anonymous.data[6] changed for archived eType Invisible Entity
  when we thought it never would" (sv_msg_write.cpp change hints) with bots.
- "Could not load xanim pb_huey_*", "xmodel t5_veh_civ_tiara", "weapon X not found
  in weapon table" (killstreak/utility weapons), bg_shock dvars set to 0.

Seen in session 3:
- `ui_mp/after_action_report.menu` lines 1989/2087: "Expected 4 params to function
  StringTableLookup, found 3" every frame, and "Could not load stringtable
  PlayerStatsList"; only these two expressions, so probably the newer menu data.
- "Unknown command" xsigninlive, updatevehiclebindings, resetCustomGametype;
  "missing TECHNIQUE_BUILD_FLOAT_Z" spam for some vehicle decal materials; several
  missing code materials/images ($floatz_donotremove, bloom_mip3_blur, redact*).
- Glass shard memory (`shardMemorySize` from the asset) also holds the 40-byte
  64-bit `Allocator::Memory` headers (20 on i386): fewer shards fit before it runs out.
- `Key_KeynumToString` (French/German digit keys) reads a table that followed
  `keynames_localized` in the original binary and is missing from the decompile.

Pre-existing (wrong on every build):
- `actor_fields.cpp` `aifields` offsets don't match `actor_s` (e.g. "fovcosine"
  3860); the hard-coded `ofs != 604/5800/3472/548` checks too.
- `KeyValueToField` case 9 (FX) falls through into the Material cases.
- Undefined-behaviour stack tricks: `ParseConfigStringToStructMerged` `(&dest)[i]`;
  `BG_LoadWeaponVariantDefFile` `&pszBuffer`/`&sourceName`, `szBuffer[0x4000*i]`;
  `xanim_load_obj.cpp` 622/664 `*(&i + type)`; `xmodel_load_phys_collmap` 981-988.
- `cscr_parsetree.cpp` `debugger_buffer` returns the type slot, not a node.
- `phys_colgeom.cpp:1156` allocates `gjk_double_sphere_t` with alignment 0 (NULL).
- `rb_pixelcost` key leaves its upper 24 bits uninitialised.
- `Material_CompileShader` passes NULL constants to D3DXCompileShader.
- `ui_server.cpp:603` writes 1 over `serverStatusInfo.lines[30][7]` (kept as on
  32-bit; probably meant `serverStatus.refreshActive`, offsets don't confirm it).
- `g_pop_iter` (physics) is never initialised.

64-bit, deliberately left:
- Physics buffer (0x380000) and 16 KB transient blocks are unchanged while 64-bit
  objects are larger: e.g. a SAP axis list block holds ~1022 nodes instead of 2046.
- Code that relies on linear encoding (fine while the memory is hunk/zone/global):
  script `OP_EvalLocal*Cached` (`Encode(top) + 12`), vector reads
  `Decode(intValue + 4)`, destructibledef/fx_profile/fx_load_obj, r_xsurface_load_obj:1446.
- `rb_resource` CREATEVERTEXDECL/LOADINDEXBUFFER treat `resource` as native `T **`
  (no producer today). r_cinematic's skip/wait logic is decompiler garbage (dead
  while Bink is stubbed; radbase.h defines `__RAD32__`, a real Bink would need 64).
- `tools/audit_rawofs.py` still lists raw-offset accesses into WeaponComponent (10),
  rigid_body (5), token_s (5), script_s (4) and a tail (flameGeneric_s and
  GfxStaticModelDrawStream are fixed): check each struct with layout_diff; those with
  the same layout on both ABIs are fine.

## Decisions (session 1)

- **Ptr32 + 32-bit linear window** instead of retyping the engine to native
  64-bit pointers: thousands of sites keep pointers in ints, and bytecode, assets
  and network fields rely on 4-byte pointers. docs/64bit.md has the details.
- **Heap inside the executable image** (1.5 GB zero-fill array): globals and heap
  share one linear window. ~1.9 GB is the most macOS loads below the dyld shared
  cache; 2 GB fails.
- **Encode passes values below 4 GB through; Decode of an unissued handle returns
  the value**: decompiled ints typed as pointers, and the loader's raw on-disk
  offsets, survive.
- **Asset, networked and offset-addressed structs keep the i386 layout**
  (`Ptr32` fields): the 732 asset structs, playerState_s/entityState_s/etc.,
  centity_s, WeaponFullDef, vehicle_info_t, VariableStackBuffer, GfxModel*Surface.
- **Decompiled structs larger than the original allocations**: allocations use
  max(original literal, sizeof) so 32-bit builds never shrink (client_t, IKState,
  vehicle_cache_t, cg_s).
- Networked structs match the i386 layout (layout_diff), so 64-bit clients can
  stay wire-compatible with 32-bit builds. Not tested.
- Five parallel agents fixed the compile errors per module (script, UI, game,
  physics, renderer); their reports' open items are above.

## Decisions (session 2)

- `Scr_Settings` honours `developer_script` (it compiles the `/# #/` blocks);
  `developer` and `abort_on_error` stay forced off as upstream had them.
- `VM_Execute_0` is built unoptimized on clang/GCC (`SCR_VM_SETJMP_SAFE`) rather
  than making its locals volatile: the error path resumes the loop with them.
- `-fno-strict-aliasing -fwrapv` on macOS and Linux.
- Struct sizes in allocation budgets follow the allocations (`sizeof`, or
  max(original literal, sizeof) where the original literal was larger).

## Decisions (session 3)

- **Stats buffer = the installed data's size**: `STATS_BUFFER_SIZE` 40548
  (live_storage.h; the 7.0.61 exe had 40168). The stat upload and stats file are
  stubbed in this codebase, so only the server->client modified-stats message (4-hex
  offsets, fine) depends on it; `MODIFIED_STATS_BYTE_SIZE` derives from it.
  `persistentStats` had a stale IDA size and its flags are named fields now.
- Linux/macOS `main` runs WinMain's pre-`Com_Init` steps (Com_InitParse, Dvar_Init,
  InitTiming, Sys_FindInfo): without Dvar_Init `set`/`seta`/`toggle` were unknown
  commands (saved configs never loaded) and the zero timer scale broke the resource
  wait (fastfile loads 4-8x slower).
- POSIX `Sys_GetInfo` reports real memory/CPU (sysctl, /proc/cpuinfo) with the same
  "2.4 GHz swag" times core factor as Windows' stubbed benchmark; with no dialog,
  changed hardware or configure_mp.csv re-runs autoconfigure (Windows asks).
- GL backend on desktop: `g_kbPresentEnter` counts presents (the per-frame program
  link budget never reset, so only 4 programs ever linked); NOOVERWRITE/DISCARD
  buffer ranges upload through an unsynchronized `glMapBufferRange` (Apple's GL made
  each in-flight `glBufferSubData` a Metal blit: 6 fps -> 100).
- SDL events are pumped only on the main thread (the render thread reached it via
  `Sys_LoadingKeepAlive` during map loads; AppKit throws).
- `tools/audit_memlit.py` looks through casts on the destination and matches
  `literal * count` sizes (found the scene, flame, huffman and graph clears).
