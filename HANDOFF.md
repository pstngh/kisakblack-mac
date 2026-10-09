# HANDOFF — native macOS (Apple Silicon) port

Branch `mac-port` of https://github.com/pstngh/kisakblack-mac. Local checkout:
`/Users/pstn/Documents/kisakblack-mac`. Game data: `/Users/pstn/Documents/Games/codbo`
(Steam Black Ops: `main/*.iwd`, `zone/Common`, `zone/English`). Build, run and
debugging commands are in CLAUDE.md; the 64-bit design and rules in docs/64bit.md.

## Goal

A native arm64 macOS build of the multiplayer executable: no Rosetta, no Wine.
Apple Silicon has no 32-bit mode, so this is also the engine's first 64-bit port.

## Status (2026-10-09, session 8)

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
  rotate maps; the ASan client ran 8 match ends/rotations with no report.
- **Networking** (session 6): UDP on Linux/macOS (`src/platform/linux/sys_net.cpp`).
  A client process connects to a separate dedicated server (CLAUDE.md has the
  commands), plays, follows match ends and map rotations (3 maps), and
  `disconnect` drops it on the server at once; a second process also joins a listen
  server. Two 64-bit processes read each other's packets, so the Huffman fix holds.
  ASan client vs. server and ASan server vs. client: 3 maps each, no report. A
  21-minute soak (ASan client, scripted player, dedicated server rotating dm, dom,
  koth, ctf, sd, dem, sab, hq, tdm with 1-minute limits) played 12 maps with no
  report after the two fixes below.
- **Offline progression** (session 7): the local player signs in at startup and
  their stats live in `players/mpstats.dat` (under `fs_h`, the home path dvar;
  `.bak` is the previous save). A `map`/`devmap` listen server plays as a ranked
  online game for the host: XP, rank-ups, CP, unlocks by rank, purchases,
  custom classes (spawned with an equipped bought weapon), prestige, and they
  persist across restarts, map rotations, `quit` and hard kills (since the last
  save). Tested with `KB_CMDS` (`set scr_givexp N`, `purchaseItem <idx>`,
  `equipClassItem customclass1 <idx>`, `uploadstats`, `prestigerequest`,
  `getxp` prints NORMAL/global XP, rank, CP) and the ASan client (3 maps).
  Screens checked: in-game "choose class" lists Custom 1-5 with "new" badges.
  Not exercised: the main-menu Create-a-Class/barracks/AAR menus by hand.
- **Everything maxed out** (session 8, the default): every weapon, attachment,
  perk and pro perk, camo/reticle/lens/tag, killstreak, emblem part, clan tag
  feature, face paint and feature (Custom 6-10) is unlocked and owned, and the
  player is rank 50, prestige 15. The stats file only matters for what the player
  sets up. `sv_cheats` stays on for `map` games. Tested on a listen server
  (mp_nuked, 4 bots, scratch `fs_h`): `getxp` shows rank 50/prestige 15 on a
  fresh and on an old profile; `equipClassItem` of L96A1 (43), G11 (32, a
  "classified" one), Tomahawk (65), Warlord (168), Ninja (172), Hacker Pro (177) and
  `statWriteDDL cacLoadouts killstreak3 209` (Attack Dogs) take, `purchaseItem`
  answers "already purchased", the player spawns with the L96A1 or G11, and the
  setup (read back with `statReadDDL`) survives restarts and a match end with map
  rotation, perks upgraded to their pro versions. In-game "choose class" lists
  Custom 1-10 without "new" badges; the main-menu Create-a-Class (`cac_main`,
  `cac_weapon`) and Killstreaks screens show no locks or prices (with the dvars
  off, a fresh profile's Killstreaks screen says "Unlocked at level 10"). `god`,
  `noclip`, `give m60_mp` work on a `map` game; after `disableCheats` they answer
  "Cheats are not enabled on this server" and `sv_cheats 1` brings them back. A
  dedicated `map` server keeps `sv_cheats 1`. ASan client: a fresh profile through
  two match ends (mp_nuked, mp_array, mp_cracked) with the scripted player, and the
  main-menu screens above, no report.
- **Portable game folder** (session 8): `tools/make_portable.sh <game dir>` puts
  `blackops` and its six Homebrew libraries (`lib/`) in the game folder; the
  user's `/Users/pstn/Documents/Games/codbo` has it. Started from another
  directory with no arguments, it loads only its own libraries (`DYLD_PRINT_LIBRARIES`:
  none from /opt/homebrew), finds its data, and writes config, stats, logs and
  screenshots only inside the folder; a scan of the home and temp folders after a
  match and a main-menu session found nothing else from the game except the
  system's Metal shader cache.
- Diagnostics: `KB_SCREENSHOT=<dir>` (+`KB_SCREENSHOT_EVERY=n`) writes the back
  buffer as a top-down TGA every n presents with per-interval draw counters;
  `KB_TRACEFRAME=n1,n2,..` logs the frames' SetRenderTarget/SetViewport/Clear/
  StretchRect calls and every draw (GL state, GL error after the draw, bound
  textures) and, with KB_SCREENSHOT, dumps the back buffer at the first 16 resolves
  (`resolve_N.tga`). `KB_PTR32STATS=1` prints the busiest slow-path
  `Ptr32_Encode` call sites (return addresses; `atos` after the slide).

## Next steps (in order)

1. **Everything maxed out and `sv_cheats` on: done in session 8** (see Decisions),
   on top of session 7's offline progression. Left over: drive the main-menu menus
   by hand (keyboard/mouse) beyond the screens `KB_MENU_CMDS` reached (`cac_main`,
   `cac_weapon`, killstreaks): the attachment/camo/reticle pickers, emblem editor,
   clan tag, barracks, combat record and after-action report; combat training
   (`xblive_basictraining`, its own buffer and rank, not maxed) is saved but
   untested. To show a prestige other than 15, `LiveStorage_SetTopRank` is the
   place (a dvar would do).
2. Keep soaking with `KB_CMDS` (longer matches, match end and map rotation, other
   gametypes) and the ASan client; each fix of this kind so far came from a run.
   Start the ASan client at the main menu too (session 6's report was in a main
   menu script that runs only without `+map`).
3. Rendering fidelity: compare against a Windows screenshot (shadows, reflections,
   gamma). `vFace`, `vPos` and the half-pixel offset follow D3D9 now.
4. Later: wire compatibility with a Windows/Linux server, an .app bundle, Retina
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
- `live_pcache` asserts `xuid != PCACHE_INVALID_XUID` at map load: gone since
  session 7 signs the local player in with an XUID.
- 50 decompiled structs whose i386 layout no longer matches IDA's `sizeof` comment
  (e.g. cgs_t 12712 vs 12708, trace_t 64 vs 56, pmove_t, actor_s, client_t,
  sharedUiInfo_t): any raw offset into them is wrong on every build. The
  cg_ents_mp.cpp corpse lookup was one (fixed).

Seen in session 8:
- One crash in ~10 match ends (listen server, scripted player, 4 bots): at the end
  of the match `G_FreeEntity: Player 0 is being freed.` without the usual
  `SV_RemoveDemoClient: democlient removed 0.` before it, then the asserts
  `g_utils_mp.cpp (2258) ed->r.inuse` and `g_spawn_mp.cpp (1067) ent->r.inuse`
  twice, then SIGSEGV on the server thread in `Demo_WritePlayerStates` <-
  `Demo_BuildDemoSnapshot` <- `SV_SendClientMessages`. Client 0 is the server's
  demo-recording client; it looks like it was freed twice and then recorded. The
  graphics options menu had just opened (`ui/options_graphics.cfg`), probably
  from a click in the window; opening it from the console at the match end didn't
  reproduce it. Not reproduced since.
- The user's `codbo/main` held junk from an Oct 8 run (sessions 3-4): a directory
  named by the bytes 0x01-0x2E (one 1 KB file in it) and empty files named 0x02 and
  0x03 (moved to the Trash in session 8). If such names reappear, a file path was
  built from garbage.
- The system's Metal shader cache (`$(getconf DARWIN_USER_CACHE_DIR)com.apple.metal`)
  is written by Apple's GL driver for any GL program, outside the game folder.

Seen in session 7:
- Contracts have no data offline (`LiveStorage_DoWeHaveContracts` is 0) though
  `level.contractsEnabled` follows `IsGlobalStatsServer`; nothing broke in tests.
- A fresh profile's first `ResetStats` runs `stats_init.cfg` with basic training
  on; `LiveStorage_ReadStats` sets `onlinegame` around it (else ValidateGameModes
  asserts hundreds of times).
- `SV_UpdatePersonalBestsForClient` stays stubbed (server-side "new item" flags
  and personal bests for remote clients); the client computes its own on disconnect.

Seen in session 6:
- `CL_Connect_f` connects only to LAN addresses (`Sys_IsLANAddress`: 10/8, 127/8,
  169.254/16, 172.16/12, 192.168/16, or the first three octets of a local interface);
  any other address silently stays `CA_DISCONNECTED` (upstream; every build).
- A remote client is "Unknown Soldier N" and every non-Steam client has the same
  SteamID (stubs_online.cpp); the server keys nothing on it today.
- "CG_SetWeaponHidePartBits: No such bone tag (tag_iron_sightlow) for weapon (m16_mp)":
  probably the data (the viewmodel lacks a tag the weapon file hides).

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

## Decisions (session 8)

- **Everything maxed out = the original's own switches, on by default.**
  `allItemsUnlocked`, `allItemsPurchased` (bg_unlockable_items.cpp) and
  `allEmblemsUnlocked`, `allEmblemsPurchased` (bg_emblems.cpp) default to 1 (on
  every build). `BG_UnlockablesAllItemsUnlocked/Free` check them first, in every
  game mode: the original honoured them only in public online games and basic
  training, and the main menu at startup is neither (a client's `onlinegame` is 0
  until it hosts a map). Everything that asks whether an item, attachment, weapon
  option, pro perk, clan tag feature or emblem part is locked or bought goes
  through these, as do the server's `purchasedItems` bits (`isItemPurchased` in
  the scripts: challenges, pro perk tracking) and "classified" weapons (declassify
  after enough purchases). They keep the cheat flag (`Dvar_SetCheatState` resets
  them to their default, 1). Turning the mode off: `+set allItemsUnlocked 0 +set
  allItemsPurchased 0 +set allEmblemsUnlocked 0 +set allEmblemsPurchased 0`
  (session 7's progression; the rank stays at what was last saved).
- **Rank** (`LiveStorage_SetTopRank`, at sign-in, while allItemsUnlocked): RANKXP
  = `CL_GetMaxXP()` (1262500), RANK = its rank (49, shown as 50), PLEVEL =
  `CL_GetMaxPrestige()` (15) in the global buffer, copied into the CAC copy; a
  listen server takes them with the host's stats, and the scripts cap XP at the
  top rank. Max prestige also hides prestige mode, whose `prestige_reset.cfg`
  resets every custom class. CP is left alone (nothing costs anything; matches
  still add to it).
- **Pro perks**: `BG_ReplaceItemsWithPurchasedProItem` at sign-in, as buying a pro
  perk did, so saved classes upgrade; the menus list the pro versions in place of
  the base ones. A base perk equipped from the console stays base until the next
  start.
- **No CAC validation** while `allItemsUnlocked` or `allItemsPurchased` is set,
  as `CL_CACValidateRequest_f` did (git history before the Demonware removal):
  `SV_ValidateClientCAC` would reject classes holding items the stats never
  recorded as bought. Mid-match class edits are then valid at once. As in the
  original, a prestige request made with only allItemsPurchased set would be lost.
- **`sv_cheats`**: `SV_Map_f` only turns it on (devmap, or developer 2 with
  thereisacow 1960), so every server, listen or dedicated, keeps the default 1.
  sv_cheats is write-protected and read-only (0x10|0x40 from two registrations),
  which lets the console set it only to its default: `sv_cheats 1` works, `0`
  doesn't; `disableCheats` (a UI command, nothing in the MP menus runs it) turns
  cheats off until a devmap or `sv_cheats 1`. Systeminfo goes out only in
  gamestates (configstring 1 is rebuilt for each), so `SV_PreFrame` clears the
  modified flag after a mid-match change instead of asserting every frame; the
  release build ignored it.
- `KB_MENU_CMDS` (linux_main.cpp): KB_CMDS's syntax, timed from startup and run
  once, connected or not, for the main menu (`openmenu cac_main`, ...).
- **Portable**: on macOS/Linux `Sys_DefaultInstallPath` (zones) and the `fs_b`
  default (base path; `fs_h`, where everything is written, defaults to it) are
  the executable's directory when `main/` and `zone/` are there, else the current
  directory as before (Windows unchanged: the original used the current
  directory, where Windows starts a program). The base path dvar is `fs_b`
  (`fs_basepath` never existed here) and doesn't move the zones, which load
  before the filesystem starts. `make_portable.sh` copies the build and every
  non-system library it links (plus SDL3, which sdl2-compat loads from next to
  itself), rewrites the references to `@executable_path/lib/`, and re-signs
  (ad hoc). Rerun it after each build; the dev workflow (build_macos/blackops
  started in the game folder, Homebrew libraries) is unchanged.
- Writes that ignored the game folder: the `screenshot` command's
  `FS_BuildOSPath(fs_gamedir, 0, ..)` had lost its fs_homepath argument (IW3 has
  it) and wrote to `<current directory>/main/screenshots` (every build);
  `LiveSteam_Init` deleted `steam_appid.txt` in the current directory (Windows
  only now; Steam is stubbed elsewhere). Debug-only writers still use the current
  directory: `quickprint.log` (cg_draw_debug), `dx.log` (`r_logFile`), and the
  `KB_*` dumps (their own paths). Bink's video path (`r_cinematic.cpp`) is the
  current directory too, but Bink is stubbed.

## Decisions (session 7)

- **Offline stats storage** (live_storage_win.cpp): one file `players/mpstats.dat`
  (header + the CAC, global and basic-training buffers, each with its checksum;
  written to `.tmp`, previous save kept as `.bak`, an unreadable file renamed
  `.bad`). `PC_InitSigninState` signs the local player in on a client (XUID = the
  Steam/stub SteamID, now 64-bit; gamertag = `name`), which reads the file or
  creates fresh stats (`mp/stats_init.cfg` gives the default custom classes).
- **Buffers**: DDL permission 1 = client (CAC loadouts, weapon attachment bits),
  2 = server (PlayerStatsList: RANKXP/CODPOINTS/PLEVEL, challenges, item
  `purchased`/`new` flags - but the client keeps those in its CAC buffer), 3 =
  both. The client reads currency/prestige from the CAC copy of PlayerStatsList;
  the global buffer is authoritative and copied over it at every commit.
- **Listen server**: the host's stats are copied into `client_t` once per
  connection (`SV_SendClientGameState`, `statPacketsReceived`); from then on the
  server's copy is newer (kept across map changes) and reaches the client as the
  `N` command. The host's custom classes are read live from the client buffer
  (`SV_GetClientCACStats`). Bots and remote clients get zeroed stats; a client
  ignores `N` from any server but its own (`com_sv_running`), so a dedicated or
  someone else's server can't overwrite it. `SV_IsLocalStatsServer` makes
  `IsGlobalStatsServer()` true (scripts: `level.rankedMatch`) and `SV_Map_f` sets
  `onlinegame 1` plus the XP/CP rates `default_xboxlive.cfg` would (scr_xpscale 1,
  scr_codpoints*scale 0.1) when unset. Without onlinegame the scripts read custom
  classes from the gamer profile (a stub returning 0).
- **Commit** (`LiveStorage_UploadStats`: the menus' `uploadstats`, the join+1 s `Q`,
  map loading; `CL_UploadStatsForController` on disconnect/quit): ports the ranked
  server's `SV_ValidateClientCAC`, charging new purchases/prestige to the global
  stats against the last validated classes. While a local match holds the stats
  it only previews (purchases charged when the host leaves; class edits that
  charge nothing are accepted at once). `N` changes to PlayerStatsList are
  mirrored into the CAC copy as deltas, so CP earned mid-match is spendable.
- Restored from the strip/decompile: `GScr_GetDStat`, `LiveStats_SpendCurrency`,
  the AAR comparison (`LiveStats_CompareStatsVsStableBuffer` on disconnect and
  in challenge sorting), `CL_PrestigeRequest`'s commit. Removed `iassert(0)` from
  `SV_GetClientDIntStat/DInt64Stat`; the 64-bit getters return 64 bits.
- Decompile bugs found on the way (every build): ~50 `DDL_MoveTo` calls whose last
  path names were dropped (count > args, varargs read garbage: item
  purchased/new flags, weapon attachment/option bits, emblems, item stat
  iteration, combat record; one combat-record name, `"used"`, is a guess);
  `LODWORD(v) = Live_GetXuid()/DDL_GetInt64()` kept only the low half (18 sites);
  `Dvar_SetFromLocalizedStr_f` (`setFromLocString`) had its buffer split into
  `char combined; char pszInputBuffer[4099]` (ASan stack overflow).
- 64-bit: `expressionEntry` is 24 bytes, allocated as 16 (`Expression_Alloc`):
  runtime `if ( ... )` commands (cfgs, menu `execNow if`) crashed in `MakeRPN`.
  Sizes use `sizeof` now and the command buffers scale with it.

## Decisions (session 6)

- **POSIX UDP** (`sys_net.cpp`) ports win_net.cpp's IP socket: `net_ip`/`net_port`
  (3074, tries the next 9 ports), non-blocking, SO_BROADCAST, `getaddrinfo` for
  names, `getifaddrs` for the local addresses that `Sys_IsLANAddress` compares,
  `net_noudp` and `net_restart`. Not ported: the SOCKS proxy, the client socket pool
  (`cl_socketpool_enabled` defaults to 0) and the remote script-debug sockets.
- **Offline connect without Steam** (`stubs_online.cpp`): the handshake carries a
  Steam ticket (getchallenge) that the server checks with `Steam_CheckClientTicket`.
  Without Steam the client sends the placeholder ticket "offline" and the stub
  user's SteamID (the same for every client), and a non-Steam server accepts any
  nonzero SteamID: there is nothing to verify against (LAN/offline play). A Windows
  Steam server rejects these clients. `CL_CDKeyValidate` calls
  `Steam_UpdateClientAuthTicket` on every platform (the stub returns true).
- `CG_Vehicle_DoControllers` is upstream's simplified rewrite of the decompiled
  function (kept under `#if 0`); it lost the `boneIndex < 0xFE` guards on the gunner
  turret, wheel and extra-wheel tags, so a vehicle without those tags (255) indexed
  `partBits[7]` of a 5-word stack array and could call `DObjSetLocalTagInternal` on
  bone 255 (writes past the bone matrices). Guards restored (every build). The rewrite
  also drops wheel suspension, steering and child-bone rotation (vehicle visuals).
- `Com_Init`: when a startup command fails (`+connect` to a bad address), the error
  cleanup unloads ui_mp right after `Com_InitUIAndCommonXAssets` loaded it, and the UI
  started without `ui_mp/menus.txt` ("Could not find menu 'main'" every frame, no
  menu). ui_mp is now reloaded right after that cleanup, as `Com_AssetLoadUI` does.
- `UI_RunMenuScript`'s "stop refresh" branches wrote `dc[1].localVars.table[65].name`:
  on i386 that is `uiInfo_s::nextFindPlayerRefresh` (offset 10080), on 64-bit 3312
  bytes past `uiInfoArray`, i.e. 8 zero bytes into another global every time the
  main menu opened. Found by the ASan client started at the main menu.

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
