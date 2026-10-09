#!/usr/bin/env python3
"""Find byte arithmetic that strides over structs with their 32-bit size.

`(T *)((char *)base + 40 * i)`, `&memory[24 * i + 20]` or `((char *)p - (char *)a) / 8`
step through an array of structs by a literal size. If that literal is the i386 size of
a struct that is larger natively (it holds pointers), the 64-bit build lands in the
middle of the wrong element.

The sizes come from probing every struct/union that has an IDA `// sizeof=0x..` comment
in a header, in both ABIs (kbprobe, with the flags of a .cpp in the header's directory).
A statement is reported when it has a `(char *)`-style cast, a `_BYTE` or a `Memory[`
array, and multiplies or divides by the i386 size of such a struct whose name appears in
the same file. Expect some false positives (file formats, script memory nodes).

  tools/audit_stride.py <build_dir> [--sizes out.json]
"""
import collections, json, os, re, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kbprobe

DEF = re.compile(r'^\s*(?:struct|union|class)\s+(?:__declspec\([^)]*\)\s+)?([A-Za-z_]\w*)\s*(?:final\s*)?(?::[^/{]*)?//\s*sizeof=0x([0-9A-Fa-f]+)', re.M)
BYTE = re.compile(r'\((?:const\s+)?(?:char|unsigned __int8|_BYTE|unsigned char|uint8_t|byte)\s*\*\)|Memory\[|\b_BYTE\b')
OPS = re.compile(r'(?<![\w.])(0x[0-9a-fA-F]+|\d+)\s*\*\s*[\w(]|[\w)\]]\s*\*\s*(0x[0-9a-fA-F]+|\d+)(?![\w.])|/\s*(0x[0-9a-fA-F]+|\d+)(?![\w.])')


def probe_sizes(build):
    """{struct: [ida, i386, native, header]} for the IDA-annotated header structs."""
    cmds = kbprobe.compile_commands(build)
    bydir = {}
    for p in cmds:
        bydir.setdefault(os.path.dirname(p), p)
    heads = {}
    for root, _, files in os.walk('src'):
        for f in files:
            if f.endswith('.h'):
                p = os.path.realpath(os.path.join(root, f))
                found = DEF.findall(open(p, encoding='latin-1').read())
                if found:
                    heads[p] = found

    def probe(h):
        tu = bydir.get(os.path.dirname(h)) or next(iter(cmds))
        e, names, out = cmds[tu], [n for n, _ in heads[h]], {}
        for i386 in (True, False):
            fd, tmp = tempfile.mkstemp(suffix='.cpp')
            with os.fdopen(fd, 'w') as f:
                f.write('#include "%s"\ntemplate <int K, unsigned long N> struct KBSZ;\n' % h)
                for k, t in enumerate(names):
                    f.write('KBSZ<%d, sizeof(%s)> kbsz_%d;\n' % (k, t, k))
            r = subprocess.run(kbprobe.compile_args(e, tu, i386) + ['-fsyntax-only', '-Wno-everything', '-ferror-limit=0', tmp],
                               cwd=e['directory'], capture_output=True, text=True, errors='replace')
            os.unlink(tmp)
            out[i386] = {names[int(k)]: int(n) for k, n in re.findall(r'KBSZ<(\d+), (\d+)', r.stderr)}
        return h, out

    sizes = {}
    with ThreadPoolExecutor(os.cpu_count()) as ex:
        for h, out in ex.map(probe, sorted(heads)):
            for n, ida in heads[h]:
                sizes[n] = [int(ida, 16), out[True].get(n), out[False].get(n), os.path.relpath(h)]
    return sizes


def main():
    argv = sys.argv[1:]
    out = None
    if '--sizes' in argv:
        i = argv.index('--sizes'); out = argv[i + 1]; del argv[i:i + 2]
    sizes = probe_sizes(argv[0])
    if out:
        json.dump(sizes, open(out, 'w'), indent=0)
    grown = collections.defaultdict(list)   # i386 size -> structs that differ natively
    for n, (ida, a, b, h) in sizes.items():
        if a and b and a != b and a >= 8:
            grown[a].append(n)
    print('%d structs probed, %d differ between i386 and native' %
          (sum(1 for v in sizes.values() if v[1]), sum(1 for v in sizes.values() if v[1] and v[2] and v[1] != v[2])))
    for root, _, files in os.walk('src'):
        for f in sorted(files):
            if not f.endswith(('.cpp', '.h')):
                continue
            p = os.path.join(root, f)
            s = re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)), open(p, encoding='latin-1').read())
            for m in re.finditer(r'[^;{}]*[;{}]', s):
                st = m.group(0)
                if not BYTE.search(st):
                    continue
                for g in OPS.findall(st):
                    n = int(next(x for x in g if x), 0)
                    names = [x for x in grown.get(n, []) if re.search(r'\b' + re.escape(x) + r'\b', s)]
                    if names:
                        line = s.count('\n', 0, m.start() + len(st) - len(st.lstrip())) + 1
                        print('%s:%d: N=%d %s | %s' % (p, line, n, ','.join(names[:4]), ' '.join(st.split())[:150]))


main()
