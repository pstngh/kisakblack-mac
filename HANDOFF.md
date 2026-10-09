# HANDOFF — native macOS (Apple Silicon) port

Branch `mac-port` of https://github.com/pstngh/kisakblack-mac. Local checkout:
`/Users/pstn/Documents/kisakblack-mac`. Game data: `/Users/pstn/Documents/Games/codbo`
(Steam Black Ops: `main/*.iwd`, `zone/Common`, `zone/English`). Build, run and
debugging commands are in CLAUDE.md; the 64-bit design and rules in docs/64bit.md.

## Goal

A native arm64 macOS build of the multiplayer executable: no Rosetta, no Wine.
Apple Silicon has no 32-bit mode, so this is also the engine's first 64-bit port.

## Status (2026-10-08, end of session 4)

- **Builds and links**: `build_macos/blackops`, Mach-O arm64. Every changed file
  passes the i386 syntax check (gfx_gl/ files: only the known header artefacts).
  Session 4 is uncommitted in the working tree.
- **Dedicated server** with 4 bots: deaths, killstreaks, ragdolls, no asserts.
- **Client**: main menu, team menu (over the blurred world), and a **local match
  that renders**: world, lighting, models, viewmodel, sky, post effects, HUD,
  minimap, ammo/equipment icons. ~95-130 fps on an M4. Keyboard, mouse and the
  console work in a match (a player spawned, moved, fired and typed commands).
- Diagnostics: `KB_SCREENSHOT=<dir>` (+`KB_SCREENSHOT_EVERY=n`) writes the back
  buffer as a top-down TGA every n presents with per-interval draw counters;
  `KB_TRACEFRAME=n1,n2,..` logs the frames' SetRenderTarget/SetViewport/Clear/
  StretchRect calls and every draw (GL state, GL error after the draw, bound
  textures) and, with KB_SCREENSHOT, dumps the back buffer at the first 16 resolves
  (`resolve_N.tga`). `KB_PTR32STATS=1` prints the busiest slow-path
  `Ptr32_Encode` call sites (return addresses; `atos` after the slide).

## Next steps (in order)

1. Longer play sessions to shake out crashes: this session's crash (XAnim client
   notifies) only showed up while playing. Look for i386 struct sizes and offsets
   still hard-coded: `tools/audit_stride.py` (literal strides; its remaining hits
   are reviewed false positives) and `tools/audit_rawofs.py` (raw field offsets).
2. x86 memory-ordering assumptions: lock-free hand-offs through plain variables
   (flags spun on, ring buffers) are ordered on x86 but not on arm64. The sound
   command/notify queues are fixed; audit the rest (render command hand-off,
   `volatile` flags, worker/job queues outside the `__sync`-based Interlocked
   wrappers, which are full barriers).
3. Redzones in the region allocators (`universal/ptr32.cpp`) so ASan sees heap
   overflows inside the in-image heap.
4. Rendering fidelity: compare against a Windows screenshot (shadows, reflections,
   gamma). `vFace`, `vPos` and the half-pixel offset follow D3D9 now.
5. Later: wire compatibility with a Windows/Linux server, an .app bundle, Retina
   (SDL_WINDOW_ALLOW_HIGHDPI), controller support.

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

Seen in session 4:
- `live_pcache` asserts `xuid != PCACHE_INVALID_XUID` at map load: with Demonware
  removed the local player has no XUID (not 64-bit related).
- `sv_main_mp.cpp (2903) !(dvar_modifiedFlags & DVAR_SYSTEMINFO)` after `sv_cheats 1`
  in the console of a listen server.
- 50 decompiled structs whose i386 layout no longer matches IDA's `sizeof` comment
  (e.g. cgs_t 12712 vs 12708, trace_t 64 vs 56, pmove_t, actor_s, client_t,
  sharedUiInfo_t): any raw offset into them is wrong on every build. The
  cg_ents_mp.cpp corpse lookup was one (fixed).

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

## Decisions (session 4)

- **The GL backend renders in D3D's row order.** The translated vertex shaders (and
  the built-in XYZRHW program) negate clip-space Y, so row 0 of every target is
  D3D's top row: render targets sample the right way up with D3D texture
  coordinates, StretchRect/viewport/scissor rects need no flip, and gl_FragCoord.y
  counts from the top like vPos. The back buffer is an offscreen FBO
  (`GLDevice::backbufferFbo`, colour + depth-stencil renderbuffers); Present blits
  it to the window upside down. Before this every post pass that read a render
  target flipped the image, and the cull mapping/viewport flips were compensating
  for the window only.
- Cull: GL's front face is always CCW (= D3D's CW), D3DCULL_CW culls GL_FRONT, so
  `gl_FrontFacing` matches D3D's vFace.
- `SetRenderTarget(0, ..)` resets the viewport (depth 0..1) and scissor to the new
  target, as D3D9 does; the renderer relies on it for its downsample chains (the
  480x270/240x135 passes drew with a 1920x1080 viewport).
- D3D9 pixel centres are at integer coordinates: vertex shaders add half a pixel
  (`kbPosFixup`, per viewport size, as Wine does) and vPos is `floor(gl_FragCoord)`.
- Sampler `sN` is bound to texture unit N at a program's first use on native GL
  (it was only done in the web build's verified-bind path; every sampler read unit 0
  and draws mixing 2D and cube samplers failed with GL_INVALID_OPERATION: the whole
  lit world, and the HUD icon that drew the font atlas).
- Sound command/notify queues: acquire/release fences (`std::atomic_thread_fence`,
  no-ops on x86) around the lock-free consumer side.
- Stride audit: the struct sizes of all 1560 IDA-annotated header structs were
  probed in both ABIs (557 differ); byte arithmetic that multiplies or divides by
  one of their i386 sizes found devgui (40), XAnimClientNotify (24, the crash),
  UILocalVar (12), static_model_leaf_t (8), the fx_marks point-group limit (68)
  and a corpse clientInfo_t lookup (`tools/audit_stride.py`); a grep for byte
  pointer differences divided by a literal found a pointer array counted in 4-byte
  slots in phys_collision.cpp. The remaining audit_stride hits are file formats,
  bytecode, script memory nodes and asset structs with the same layout.
- `Ptr32_Encode` slow path (the old "performance pass" item): measured ~16K calls
  per map load and none during play; not a hot spot. SV_LinkEntity's NaN check
  decoded float bits as a pointer (fixed).

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
