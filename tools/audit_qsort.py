#!/usr/bin/env python3
"""Find qsort/bsearch calls whose literal element size is a 32-bit sizeof.

`qsort(names, n, 4, cmp)` over a `char *[]` sorts in 4-byte steps on 64-bit.
Reports calls where the literal equals sizeof(element) on i386 but not here.
Needs Homebrew LLVM (clang-query).

  tools/audit_qsort.py <build_dir> [file.cpp ...]
"""
import os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kbprobe

QUERY = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'audit_qsort.query')
CLANG_QUERY = '/opt/homebrew/opt/llvm/bin/clang-query'
build = sys.argv[1]
cmds = kbprobe.compile_commands(build)
files = [os.path.realpath(f) for f in sys.argv[2:]] or sorted(cmds)


def run(chunk):
    r = subprocess.run([CLANG_QUERY, '-p', build, '--extra-arg=-isysroot', '--extra-arg=' + kbprobe.sdk(),
                        '--extra-arg=-Wno-everything', '-f', QUERY] + chunk, capture_output=True, text=True, errors='replace')
    out, lines, cur = [], r.stdout.split('\n'), None
    for i, l in enumerate(lines):
        if l.startswith('Binding for "base"') and i + 1 < len(lines):
            m = re.search(r"<(/[^:>]+):(\d+):\d+[^>]*> '([^']*)'", lines[i + 1])
            cur = (m.group(1), int(m.group(2)), m.group(3)) if m else None
        elif l.startswith('Binding for "n"') and cur and i + 1 < len(lines):
            m = re.search(r"IntegerLiteral .* (\d+)$", lines[i + 1].strip())
            if m:
                out.append(cur + (int(m.group(1)),))
    return out


def elem(t):
    m = re.fullmatch(r'(.*?)\s*\[\d+\]', t.strip())
    if m: return m.group(1).strip()
    return t.strip()[:-1].strip() if t.strip().endswith('*') else None

sites = set()
with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
    for res in pool.map(run, [files[i:i + 8] for i in range(0, len(files), 8)]):
        sites |= set(res)
byfile = {}
for f, l, t, n in sites:
    e = elem(t)
    if e and e not in ('void', 'const void'):
        byfile.setdefault(os.path.realpath(f), set()).add(e)
sizes = {}
with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
    for f, a, b in pool.map(lambda it: (it[0], kbprobe.sizeof(cmds, it[0], it[1], True), kbprobe.sizeof(cmds, it[0], it[1], False)),
                            [(f, t) for f, t in byfile.items() if f in cmds]):
        for t in a:
            if t in b: sizes[(f, t)] = (a[t], b[t])
for f, l, t, n in sorted(sites):
    k = (os.path.realpath(f), elem(t))
    if k in sizes and n == sizes[k][0] and sizes[k][0] != sizes[k][1]:
        print('%s:%d: element %s is %d on i386, %d here; literal %d' % (os.path.relpath(f), l, k[1], sizes[k][0], sizes[k][1], n))
