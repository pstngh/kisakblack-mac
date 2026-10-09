# HANDOFF — native macOS (Apple Silicon) port

Branch `mac-port` of https://github.com/pstngh/kisakblack-mac. Local checkout:
`/Users/pstn/Documents/kisakblack-mac`. Game data: `/Users/pstn/Documents/Games/codbo`
(Steam Black Ops: `main/*.iwd`, `zone/Common`, `zone/English`). Build, run and
debugging commands are in CLAUDE.md; the 64-bit design and rules in docs/64bit.md.

## Goal

A native arm64 macOS build of the multiplayer executable: no Rosetta, no Wine.
Apple Silicon has no 32-bit mode, so this is also the engine's first 64-bit port.

## Status (2026-10-09, session 5)

- **Builds and links**: `build_macos/blackops`, Mach-O arm64, and the ASan build
  `build_asan`. Every changed file passes the i386 syntax check (gfx_gl/ files: only
  the known header artefacts).
- **Dedicated server** with 4 bots: deaths, killstreaks, ragdolls, no asserts; also
  clean under ASan with heap redzones (120 s).
- **Client**: main menu, team menu over the blurred world, local matches that render
  on every MP map; ~95-130 fps on an M4. Keyboard, mouse and console work.
  `KB_CMDS` (CLAUDE.md) plays without a keyboard: join, class, move, fire, grenade,
  reload, weapon switch, death and respawn. A soak of all 14 MP maps x 180 s with
  the scripted player ran without a crash; 1-minute matches end (scoreboard) and
  rotate maps; the ASan client ran 8 match ends/rotations with no report after the
  fixes below. Session 5 is **uncommitted** in the working tree.
- Diagnostics: `KB_SCREENSHOT=<dir>` (+`KB_SCREENSHOT_EVERY=n`) writes the back
  buffer as a top-down TGA every n presents with per-interval draw counters;
  `KB_TRACEFRAME=n1,n2,..` logs the frames' SetRenderTarget/SetViewport/Clear/
  StretchRect calls and every draw (GL state, GL error after the draw, bound
  textures) and, with KB_SCREENSHOT, dumps the back buffer at the first 16 resolves
  (`resolve_N.tga`). `KB_PTR32STATS=1` prints the busiest slow-path
  `Ptr32_Encode` call sites (return addresses; `atos` after the slide).

## Next steps (in order)

1. **Client and dedicated server as separate processes.** A dedicated server
   (`+set dedicated 1 +set net_port 28960 ... +map mp_nuked`) and a client started
   with `+set net_port 28961 +connect 127.0.0.1:28960` (25 s later): the client
   prints "Disconnecting: Bad server address" and never connects (then "Could not
   find menu 'main'" spam); the server only ever sees its bots. Look at
   NET_StringToAdr / the connect command and the sockaddr handling on POSIX. Once
   it connects, this is the test for the Huffman fix below (two 64-bit processes
   must build the same tree).
2. Keep soaking with `KB_CMDS` (longer matches, match end and map rotation, other
   gametypes) and the ASan client; each fix of this kind so far came from a run.
3. Offline stats: nothing sets `statsFetched` since the online services were
   removed, so every stat operation fails (rank, unlocks, after-action report).
   Decide on a local stats file (LiveStorage) or fetched-empty defaults.
4. Rendering fidelity: compare against a Windows screenshot (shadows, reflections,
   gamma). `vFace`, `vPos` and the half-pixel offset follow D3D9 now.
5. Later: wire compatibility with a Windows/Linux server, an .app bundle, Retina
   (SDL_WINDOW_ALLOW_HIGHDPI), controller support.

## Open items found but not fixed

Seen in session 2, unverified against a Windows run:
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
- `tools/audit_rawofs.py`'s remaining sites are reviewed: centity_s and the GfxModel
  surfaces keep the i386 layout; WeaponComponent, the rigid_body fields reached (m_last_position,
  m_mat, m_a_vel) and the other listed structs lay out the same on both ABIs; the rest are byte-wise
  string copies through a struct pointer.
- `mem_fixed.cpp` (HU_SCHEME_FIXED) keeps its header in pointer slots with i386
  sizes; nothing creates a hunk user with that scheme.

## Decisions (session 5)

- arm64 memory ordering: acquire/release fences (`std::atomic_thread_fence`, no
  code on x86) where a thread reads data another thread published through a plain
  variable: the dvar read lock (after the writeCount spin), job-queue completion
  (`jqPoll`'s "done" result, the end of `jqFlush`), the stream IO thread's
  request status (texture buffers), deferred server->client packets and the glass
  action ring, on top of session 4's sound queues. Hand-offs through events,
  critical sections or the Interlocked wrappers (full barriers) need nothing.
- ASan: under `-fsanitize=address` the region allocator poisons a guard granule
  after each allocation and freed ranges; the small-block heap adds a 16-byte
  redzone and poisons slack and freed blocks. The first run found the physics
  debug buffers allocated with their i386 size.
- `KB_CMDS` scripted console commands (linux_main.cpp) for unattended play.
- "Tried to perform a stats operation before the stats have been fetched" prints
  once (it fired every frame); the underlying issue is next step 2.
- The session-1 cast rewrite also produced `Ptr32_Decode(<size>)` comparisons
  (glass shard Defrag assert) and hid out-of-bounds reads into the next global
  (live_contracts display order); FindCycleBFS's stack-local queue is a struct now.
- qsort comparators over arrays of native pointers that read the element as a
  32-bit value and decoded it sorted by garbage (the low half of a 64-bit pointer;
  it only works when the image base's low 32 bits are 0, as under lldb): HUD
  elements (`compare_hudelems`), unlockable items, and the **network Huffman tree**
  (`nodeCmp`), which came out ASLR-dependent, so two 64-bit processes could not
  read each other's packets (the listen server shares one tree, so it never showed).
- `GfxCmdBufState` stats pointers (`prim.viewStats/primStats/backupPrimStats`)
  point into the state's own `frameStats`; R_DrawCall's stack copies left the global
  pointing into a dead frame (later draws added counts into other functions'
  locals). `R_RebaseCmdBufStats` re-points them after each copy (every build).
- `R_SetDLightsConstants` (upstream reconstruction): the spot light def was stored as
  8 bytes into `diffuseColor[0]` (clobbering an omni light's colour, or the pointer
  read back with an omni's colour in its high half), and the spot matrix transform
  read a 4th component past `lightAttentuation` (now w 0, then `[3][3] = 1`).
- `cdl_proftimer::reset` sorted `mx[5]` with `i < 5` (read and swapped one past).

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
