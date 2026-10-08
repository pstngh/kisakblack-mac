#!/usr/bin/env python3
"""Find memset/memcpy of a struct with a literal size that is a 32-bit sizeof.

`memset(p, 0, 52)` with `p` a `Foo *`: if 52 is a whole number of i386 Foo's and
Foo is larger natively, the call clears or copies too little (GREW). Needs
Homebrew LLVM (clang-query).

  tools/audit_memlit.py <build_dir> [file.cpp ...]
"""
import collections, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kbprobe

QUERY = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'audit_memlit.query')
CLANG_QUERY = '/opt/homebrew/opt/llvm/bin/clang-query'
DST = re.compile(r"<(/[^:>]+):(\d+):\d+(?:, [^>]*)?> '(?:const )?(?:struct |union )?([A-Za-z_][\w:<>, ]*?) \*'")
LIT = re.compile(r"IntegerLiteral .* '[^']*' (\d+)$")

build = sys.argv[1]
cmds = kbprobe.compile_commands(build)
files = [os.path.realpath(f) for f in sys.argv[2:]] or sorted(cmds)


def run(chunk):
    r = subprocess.run([CLANG_QUERY, '-p', build, '--extra-arg=-isysroot', '--extra-arg=' + kbprobe.sdk(),
                        '--extra-arg=-Wno-everything', '-f', QUERY] + chunk,
                       capture_output=True, text=True, errors='replace')
    out, lines, cur = [], r.stdout.split('\n'), None
    for i, l in enumerate(lines):
        if l.startswith('Binding for "dst"') and i + 1 < len(lines):
            m = DST.search(lines[i + 1])
            cur = (m.group(3).strip(), m.group(1), int(m.group(2))) if m else None
        elif l.startswith('Binding for "n"') and cur and i + 1 < len(lines):
            m = LIT.search(lines[i + 1].strip())
            if m:
                out.append(cur + (int(m.group(1)),))
            cur = None
    return out

sites = []
with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
    for res in pool.map(run, [files[i:i + 8] for i in range(0, len(files), 8)]):
        sites += res
by_file = collections.defaultdict(set)
for t, f, line, n in sites:
    by_file[os.path.realpath(f) if os.path.realpath(f) in cmds else None].add(t)


def probe(item):
    f, types = item
    return f, kbprobe.sizeof(cmds, f, types, True), kbprobe.sizeof(cmds, f, types, False)

sizes = {}
with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
    for f, s32, s64 in pool.map(probe, [(f, t) for f, t in by_file.items() if f]):
        for t in s32:
            if t in s64:
                sizes[t] = (s32[t], s64[t])
seen = set()
for t, f, line, n in sorted(sites, key=lambda x: (x[1], x[2])):
    if (f, line) in seen or t not in sizes:
        continue
    seen.add((f, line))
    a, b = sizes[t]
    if n % a == 0 and a != b:
        print('GREW %s:%d: %d = %d x %d-byte %s on i386; %d bytes here' % (os.path.relpath(f), line, n, n // a, a, t, b))
print('%d literal-sized memset/memcpy on struct pointers checked' % len(seen), file=sys.stderr)
