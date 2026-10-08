#!/usr/bin/env python3
"""Find allocations sized with 32-bit sizeof literals, e.g. `(Foo *)Z_Malloc(12 * n)`
or `(Foo *)Hunk_Alloc(827392)` for an array of 1024 Foo.

For each `(T *)ALLOC(size...)` whose size is a literal or `literal * expr`, reads
sizeof(T) for i386 and natively. Reported:
  - GREW: the literal is a whole number of i386 T's, and T is larger natively
    (a 64-bit heap overflow);
  - ODD:  the literal is not a whole number of T's even on i386 (the decompiled
    struct and the original binary disagree: check it, on every build).

  tools/audit_allocs.py <build_dir> [file.cpp ...]
"""
import json, os, re, shlex, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

# allocator -> index of the size argument
ALLOCS = {
    'Z_Malloc': 0, 'Z_TryMalloc': 0, 'Z_MallocGarbage': 0, 'Z_TryMallocGarbage': 0,
    'Z_VirtualAlloc': 0, 'Z_TryVirtualAlloc': 0, 'Hunk_Alloc': 0, 'Hunk_AllocAlign': 0,
    'Hunk_AllocLow': 0, 'Hunk_AllocLowAlign': 0, 'Hunk_AllocateTempMemory': 0,
    'Hunk_AllocateTempMemoryHigh': 0, 'Hunk_AllocXAnimPrecache': 0, 'Hunk_AllocPhysPresetPrecache': 0,
    'Hunk_UserAlloc': 1, 'MT_Alloc': 0, 'UI_Alloc': 0, 'TempMalloc': 0, 'TempMallocAlign': 0,
    'TempMallocAlignStrict': 0, 'malloc': 0, 'tlMemAlloc': 0, 'PMem_Alloc': 0, '_PMem_AllocNamed': 0,
    'Hunk_AllocDebugMem': 0, 'Z_VirtualAllocInternal': 0, 'CG_HunkAlloc': 0,
}
CALL = re.compile(r'\(\s*(?:struct\s+|const\s+)?([A-Za-z_]\w*)\s*\*\s*\)\s*(?:Ptr32_Decode\(\s*)?(%s)\s*\(' % '|'.join(ALLOCS))
SKIP_TYPES = {'char', 'void', 'int', 'unsigned', 'float', 'short', 'double', 'bool', 'BYTE', 'DWORD',
              '_BYTE', '_DWORD', '_WORD', 'uint8', 'uint16', 'uint32', 'int8', 'int16', 'int32', 'byte'}


def split_args(text, start):
    """Arguments of the call whose '(' is at text[start]; returns list of strings."""
    depth, args, cur, i = 0, [], '', start
    while i < len(text):
        ch = text[i]
        if ch == '(':
            depth += 1
            if depth > 1: cur += ch
        elif ch == ')':
            depth -= 1
            if depth == 0:
                args.append(cur.strip()); return args
            cur += ch
        elif ch == ',' and depth == 1:
            args.append(cur.strip()); cur = ''
        elif ch == '"':
            j = text.index('"', i + 1)
            while text[j - 1] == '\\': j = text.index('"', j + 1)
            cur += text[i:j + 1]; i = j
        else:
            cur += ch
        i += 1
    return None


def literal_coeff(size):
    size = size.strip()
    m = re.fullmatch(r'(0x[0-9A-Fa-f]+|\d+)u?', size)
    if m: return int(m.group(1), 0)
    m = re.fullmatch(r'(0x[0-9A-Fa-f]+|\d+)u?\s*\*\s*.+', size) or re.fullmatch(r'.+\*\s*(0x[0-9A-Fa-f]+|\d+)u?', size)
    if m: return int(m.group(1), 0)
    return None


def sites_in(path):
    text = open(path, encoding='latin-1').read()
    out = []
    for m in CALL.finditer(text):
        t, fn = m.group(1), m.group(2)
        if t in SKIP_TYPES: continue
        args = split_args(text, m.end() - 1)
        if not args or len(args) <= ALLOCS[fn]: continue
        lit = literal_coeff(args[ALLOCS[fn]])
        if lit is None or lit <= 1: continue
        if '*' not in args[ALLOCS[fn]] and t in SKIP_TYPES: continue
        line = text.count('\n', 0, m.start()) + 1
        out.append((t, lit, line, text[m.start():m.end()] + args[ALLOCS[fn]] + ', ...)'))
    return out


def probe(build, cmds, path, sites, i386, sdk):
    e = cmds[path]
    args, out, skip = shlex.split(e['command']), [], False
    for a in args:
        if skip: skip = False; continue
        if a in ('-o', '-MF', '-MT', '-c'): skip = True; continue
        if a == '-MD' or os.path.realpath(a) == path: continue
        if i386:
            if a in ('-arch', '-isysroot'): skip = True; continue
            if a.startswith('-mmacos') or 'compat/arm64' in a or 'sse2neon' in a: continue
        out.append(a)
    if i386:
        out += ['-target', 'i386-unknown-linux-gnu', '-malign-double', '-mms-bitfields', '-nostdinc++',
                '-isystem', sdk + '/usr/include/c++/v1', '-isystem', sdk + '/usr/include', '-D__APPLE__',
                '-Wno-int-to-pointer-cast']
    fd, tmp = tempfile.mkstemp(suffix='.cpp')
    with os.fdopen(fd, 'w') as f:
        f.write('#include "%s"\n' % path)
        f.write('template <int K, unsigned long N> struct KBSZ;\n')
        for k, (t, lit, line, _) in enumerate(sites):
            f.write('KBSZ<%d, sizeof(%s)> kbsz_%d;\n' % (k, t, k))
    r = subprocess.run(out + ['-fsyntax-only', '-Wno-everything', '-ferror-limit=0', tmp], cwd=e['directory'],
                       capture_output=True, text=True, errors='replace')
    os.unlink(tmp)
    sizes = {int(k): int(n) for k, n in re.findall(r"KBSZ<(\d+), (\d+)U?L?>", r.stderr)}
    return sizes, r.stderr


def main():
    build = sys.argv[1]
    cmds = {os.path.realpath(e['file']): e for e in json.load(open(os.path.join(build, 'compile_commands.json')))}
    files = [os.path.realpath(f) for f in sys.argv[2:]] or sorted(cmds)
    sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()
    work = [(f, sites_in(f)) for f in files if f in cmds]
    work = [(f, s) for f, s in work if s]

    def check(item):
        f, s = item
        n64, _ = probe(build, cmds, f, s, False, sdk)
        n32, _ = probe(build, cmds, f, s, True, sdk)
        out = []
        for k, (t, lit, line, src) in enumerate(s):
            a, b = n32.get(k), n64.get(k)
            if not a or not b:
                continue
            if lit % a == 0:
                if b != a:
                    out.append(('GREW', f, line, '%s: %d = %d x %d-byte %s on i386; %s is %d bytes here'
                                % (src, lit, lit // a, a, t, t, b)))
            else:
                out.append(('ODD ', f, line, '%s: %d is not a multiple of sizeof(%s) = %d on i386 (%d here)'
                            % (src, lit, t, a, b)))
        return out

    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        for res in pool.map(check, work):
            for kind, f, line, msg in res:
                print('%s %s:%d: %s' % (kind, os.path.relpath(f), line, msg))
    print('%d files with literal-sized allocations checked' % len(work), file=sys.stderr)


main()
