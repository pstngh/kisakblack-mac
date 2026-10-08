# KisakBlack, mac-port branch

Decompiled Call of Duty: Black Ops multiplayer, being ported to native macOS on
Apple Silicon (arm64). Read HANDOFF.md for status and next steps, and
docs/64bit.md before touching anything that handles pointers.

## Build (macOS)

```sh
cmake -S . -B build_macos -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build_macos            # -> build_macos/blackops
```

The Windows (MSVC x86) and Linux (GCC i386) builds must keep working. They can't
be built here, so check every changed file with
`python3 tools/check_file.py build_macos --i386 <files>` as well as without
`--i386`. Known artefacts of the i386 check (not code errors): phys_gjk.cpp
"expects x87 floats", cm_mesh.cpp long double, TargetConditionals.h, SDL headers,
and the gfx_gl/ files (they include the macOS GLEW header).

## Run

Game data: /Users/pstn/Documents/Games/codbo. From that directory:

```sh
/Users/pstn/Documents/kisakblack-mac/build_macos/blackops +set fs_basepath /Users/pstn/Documents/Games/codbo +set dedicated 1 +set developer 1
```

macOS has no `timeout`; use `perl -e 'alarm shift; exec @ARGV' 60 <cmd>`, or
`perl tools/run_sampled.pl <seconds> <log> <cmd...>` to run, capture every
thread's stack (macOS `sample`) into `<log>.sample`, and kill it. Crash
backtraces print from the signal handler; `atos -o build_macos/blackops <addr>`
symbolizes after subtracting the slide (compare `nm` of CrashHandler with its
printed address). `lldb --batch -o run -k "bt" -k "frame variable" -- <cmd>`
works; if a launch under lldb hangs in dyld `open`, kill leftover blackops
processes first.

## Conventions

- Match the decompiled style around the edit; no reformatting.
- Pointer <-> 32-bit int: `Ptr32_Encode` / `Ptr32_Decode`. Never `.v`/`.get()`
  on a Ptr32 (it is `T*` on 32-bit builds): use `Ptr32_Raw`/`Ptr32_SetRaw` or a cast.
- Asset and networked structs keep the i386 layout: `tools/layout_diff.py`.
- Commit messages end with the Co-Authored-By line the session provides.
