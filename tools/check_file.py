#!/usr/bin/env python3
"""Syntax-check source files with the build's own flags, without running ninja.

Safe to run in parallel (no build-directory state is touched).

  tools/check_file.py <build_dir> [--i386] <file.cpp>...

--i386 compiles for the 32-bit x86 ABI the Windows/Linux builds use (Ptr32<T>
is a plain T* there), to confirm a change keeps those builds compiling. It uses
the macOS SDK's headers, so it is a syntax check of the engine code, not a full
Linux build.
"""
import json, os, shlex, subprocess, sys

def main():
    argv = sys.argv[1:]
    i386 = '--i386' in argv
    argv = [a for a in argv if a != '--i386']
    build, files = argv[0], [os.path.realpath(f) for f in argv[1:]]
    cmds = {os.path.realpath(e['file']): e for e in json.load(open(os.path.join(build, 'compile_commands.json')))}
    sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip() if i386 else None
    status = 0
    for f in files:
        e = cmds.get(f)
        if not e:
            print('%s: no compile command' % f); status = 1; continue
        args, out, skip = shlex.split(e['command']), [], False
        for a in args:
            if skip: skip = False; continue
            if a in ('-o', '-MF', '-MT', '-c'): skip = True; continue
            if a == '-MD' or os.path.realpath(a) == f: continue
            if i386:
                if a in ('-arch', '-isysroot'): skip = True; continue
                if a.startswith('-mmacos') or 'compat/arm64' in a or 'sse2neon' in a: continue
            out.append(a)
        if i386:
            out += ['-target', 'i386-unknown-linux-gnu', '-malign-double', '-mms-bitfields',
                    '-nostdinc++', '-isystem', sdk + '/usr/include/c++/v1', '-isystem', sdk + '/usr/include',
                    '-D__APPLE__', '-Wno-int-to-pointer-cast']
        r = subprocess.run(out + ['-fsyntax-only', f], cwd=e['directory'], capture_output=True, text=True, errors='replace')
        sys.stdout.write(r.stderr)
        if r.returncode: status = 1
    sys.exit(status)

main()
