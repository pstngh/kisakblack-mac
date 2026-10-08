#!/usr/bin/env python3
"""Count raw-offset accesses into structs, e.g. `*((unsigned int *)cent + 201)`.

Decompiled code often addresses struct members by 32-bit byte or word offsets.
On 64-bit those are wrong for any struct whose layout changed (see
tools/layout_diff.py); such structs either get their i386 layout back (Ptr32
pointer fields) or their accesses rewritten to members.

  tools/audit_rawofs.py <build_dir> [file.cpp ...] [--sites TYPE]
"""
import collections, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

QUERY = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'audit_rawofs.query')
CLANG_QUERY = '/opt/homebrew/opt/llvm/bin/clang-query'
BIND = re.compile(r"<(/[^:>]+):(\d+):\d+(?:, [^>]*)?> '(?:const )?(?:struct |union )?([A-Za-z_][\w:<>, ]*?) \*'")

argv = sys.argv[1:]
sites_for = None
if '--sites' in argv:
    i = argv.index('--sites'); sites_for = argv[i + 1]; del argv[i:i + 2]
build = argv[0]
files = [os.path.realpath(f) for f in argv[1:]] or \
    [e['file'] for e in json.load(open(os.path.join(build, 'compile_commands.json')))]
sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()

def run(chunk):
    r = subprocess.run([CLANG_QUERY, '-p', build, '--extra-arg=-isysroot', '--extra-arg=' + sdk,
                        '--extra-arg=-Wno-everything', '-f', QUERY] + chunk,
                       capture_output=True, text=True, errors='replace')
    out, lines = [], r.stdout.split('\n')
    for i, l in enumerate(lines):
        if l.startswith('Binding for "base"') and i + 1 < len(lines):
            m = BIND.search(lines[i + 1])
            if m:
                out.append((m.group(3).strip(), m.group(1), int(m.group(2))))
    return out

chunks = [files[i:i + 8] for i in range(0, len(files), 8)]
sites = collections.defaultdict(set)
with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
    for res in pool.map(run, chunks):
        for t, f, line in res:
            sites[t].add((os.path.relpath(f), line))
if sites_for:
    for f, line in sorted(sites.get(sites_for, ())):
        print('%s:%d' % (f, line))
else:
    for t, s in sorted(sites.items(), key=lambda x: -len(x[1])):
        print('%5d  %s' % (len(s), t))
