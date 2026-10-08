# HANDOFF — native macOS (Apple Silicon) port

Branch `mac-port` of https://github.com/pstngh/kisakblack-mac. Local checkout:
`/Users/pstn/Documents/kisakblack-mac`. Game data: `/Users/pstn/Documents/Games/codbo`
(Steam Black Ops: `main/*.iwd`, `zone/Common`, `zone/English`). Build, run and
debugging commands are in CLAUDE.md; the 64-bit design and rules in docs/64bit.md.

## Goal

A native arm64 macOS build of the multiplayer executable: no Rosetta, no Wine.
Apple Silicon has no 32-bit mode, so this is also the engine's first 64-bit port.

## Status (2026-10-08, end of session 1)

- **Builds and links**: `build_macos/blackops`, Mach-O arm64, no compile errors.
  Every translation unit still passes the i386 syntax check.
- **Dedicated server boots** (`+set dedicated 1`): loads en_/code_pre_gfx_mp,
  en_/code_post_gfx_mp, en_/patch_mp, en_common_mp and the rest of the startup
  zones, runs the filesystem, renderer init, live/stats init, and settles in the
  `Com_Frame` loop (idle, no crash after 45 s). It has **not loaded a map yet**.
- The client (window + GL) has **not been run**. The GL 4.1 core path is written
  but untested (see step 3).
- Many "Redundant asset" lines during zone loading. Believed normal (same asset in
  two zones), UNVERIFIED against a Windows run.

## Next steps (in order)

1. **Load a map on the dedicated server**: add `+map mp_nuked` (or `+devmap`).
   This exercises GfxWorld/clipMap loading, the script compiler and VM (map and
   gametype .gsc), entities, physics and the server frame: the code most changed
   by the 64-bit work. Fix crashes the way session 1 did (CLAUDE.md "Run").
2. Leave it running a few minutes with bots if possible (`scr_testclients`?), and
   connect nothing yet.
3. **Client**: run without `dedicated`. Expect GL work: the shaders the D3D9
   translator emits as GLSL ES 3.00 are given a `#version 410 core` header and
   stripped of precision statements (`gfx_gl/gl_platform.h`), alpha test runs in
   the shaders, A8L8/L8/A8 textures are R8/RG8 plus swizzles. None of it has run.
   Check shader compile logs first (`KB_DUMP_GLSL=<dir>` dumps every shader).
4. Main menu, then a local match against bots.
5. AddressSanitizer build; redzones in the region allocators (`universal/ptr32.cpp`).
6. Performance pass: alignment checks that call `Ptr32_Encode` on stack or malloc
   pointers take the handle-table mutex (renderer and physics fixed theirs; others
   remain: grep `Ptr32_Encode(.*) & 0x`).
7. Later: wire compatibility with a Windows/Linux server, an .app bundle, Retina
   (SDL_WINDOW_ALLOW_HIGHDPI), controller support.

## Open items found but not fixed

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
- `tools/audit_rawofs.py` still lists raw-offset accesses into flameGeneric_s (15),
  WeaponComponent (10), GfxStaticModelDrawStream (6), rigid_body (5), token_s (5),
  script_s (4) and a tail: check each struct with layout_diff; those with the same
  layout on both ABIs are fine.

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
