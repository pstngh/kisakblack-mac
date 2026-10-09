#!/usr/bin/env python3
"""Find Ptr32 objects passed to printf-style functions.

clang reports a class object passed through varargs (-Wclass-varargs, an error in
the macOS build) except to functions with a format attribute (snprintf, sprintf,
...), where only -Wformat notices. A Ptr32 there is read as a native pointer.

usage: tools/audit_format.py   (run from the repo root; uses build_macos)
"""
import json, shlex, subprocess, sys, os
from concurrent.futures import ThreadPoolExecutor
cmds = json.load(open('build_macos/compile_commands.json'))
def run(e):
    f = e['file']
    if not f.endswith('.cpp'): return []
    args, out, skip = shlex.split(e['command']), [], False
    for a in args:
        if skip: skip = False; continue
        if a in ('-o', '-MF', '-MT', '-c'): skip = True; continue
        if a in ('-MD', '-Wno-everything') or a == f: continue
        out.append(a)
    r = subprocess.run(out + ['-fsyntax-only', '-Wno-everything', '-Wformat', f], cwd=e['directory'], capture_output=True, text=True, errors='replace')
    return [l for l in r.stderr.split('\n') if 'warning:' in l and 'Ptr32<' in l]
hits = []
with ThreadPoolExecutor(max_workers=10) as ex:
    for res in ex.map(run, cmds):
        hits += res
seen = set()
for h in hits:
    k = h.split(' warning')[0]
    if k in seen: continue
    seen.add(k); print(h.replace(os.getcwd() + '/', ''))
print(len(seen), 'sites', file=sys.stderr)
