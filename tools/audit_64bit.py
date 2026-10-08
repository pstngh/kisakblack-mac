#!/usr/bin/env python3
"""Find 64-bit hazards the compiler accepts (see docs/64bit.md), with clang-query.

Matchers live in tools/audit_64bit.query:
  1. *(int *)&ptrVar     reads 4 bytes of an 8-byte native pointer
  2. (T **)&ptr32Field   views a 4-byte Ptr32 slot as a native pointer
  3. (T *)enumValue      a pointer kept in an enum-typed field (not covered by
                         -Wint-to-pointer-cast)

Needs Homebrew LLVM (clang-query). Prints each matched source location once.

  tools/audit_64bit.py <build_dir> [file.cpp ...]   (default: every file in the build)
"""
import json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

QUERY = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'audit_64bit.query')
CLANG_QUERY = '/opt/homebrew/opt/llvm/bin/clang-query'
LOC = re.compile(r'^(/[^:]+):(\d+):(\d+): note: "root" binds here')

build = sys.argv[1]
files = [os.path.realpath(f) for f in sys.argv[2:]] or \
    [e['file'] for e in json.load(open(os.path.join(build, 'compile_commands.json')))]
sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()

def run(chunk):
    r = subprocess.run([CLANG_QUERY, '-p', build, '--extra-arg=-isysroot', '--extra-arg=' + sdk,
                        '--extra-arg=-Wno-everything', '-f', QUERY] + chunk,
                       capture_output=True, text=True, errors='replace')
    out, lines = [], r.stdout.split('\n') + r.stderr.split('\n')
    for i, l in enumerate(lines):
        m = LOC.match(l)
        if m:
            src = lines[i + 1].split('|', 1)[1].strip() if i + 1 < len(lines) and '|' in lines[i + 1] else ''
            out.append((m.group(1), int(m.group(2)), src))
    return out

chunks = [files[i:i + 8] for i in range(0, len(files), 8)]
seen = set()
with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
    for res in pool.map(run, chunks):
        for f, line, src in res:
            if (f, line) not in seen:
                seen.add((f, line))
                print('%s:%d: %s' % (os.path.relpath(f), line, src))
print('%d sites' % len(seen), file=sys.stderr)
