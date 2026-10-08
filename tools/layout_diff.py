#!/usr/bin/env python3
"""Compare struct layouts between the original 32-bit x86 ABI and the native build.

Compiles the given translation units twice with clang's record-layout dump: once
for i386 (with -malign-double and MS bitfield packing, which matches the layout
the shipped game data and network protocol were built with) and once as the
native build compiles them. Prints every struct whose size or field offsets
differ, optionally restricted to a list of names.

usage: tools/layout_diff.py <build_dir> <source.cpp>... [--only NAME,NAME] [--json out.json]
"""
import json, os, re, shlex, subprocess, sys

def compile_cmd(build_dir, src):
    for e in json.load(open(os.path.join(build_dir, 'compile_commands.json'))):
        if os.path.realpath(e['file']) == os.path.realpath(src):
            args = shlex.split(e['command'])
            out, skip = [], False
            for a in args:
                if skip: skip = False; continue
                if a in ('-o', '-MF', '-MT', '-c'): skip = True; continue
                if a == '-MD': continue
                out.append(a)
            return out, e['directory']
    raise SystemExit('no compile command for %s' % src)

def dump(args, cwd, src, i386, probe=None):
    args = [a for a in args if a != os.path.realpath(src) and a != src]
    if probe:
        # Clang only lays out records the TU needs; force the requested ones.
        import tempfile
        fd, tmp = tempfile.mkstemp(suffix='.cpp')
        with os.fdopen(fd, 'w') as f:
            f.write('#include "%s"\n' % src)
            for i, n in enumerate(sorted(probe)):
                f.write('static unsigned long kisak_layout_probe_%d = sizeof(%s);\n' % (i, n))
        src = tmp
    extra = ['-Wno-everything', '-fsyntax-only', '-Xclang', '-fdump-record-layouts']
    if i386:
        sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()
        drop = re.compile(r'^-I.*(compat/arm64|sse2neon)')
        cleaned, skip = [], False
        for a in args:
            if skip: skip = False; continue
            if a in ('-arch', '-isysroot'): skip = True; continue
            if a.startswith('-mmacos') or drop.match(a): continue
            cleaned.append(a)
        args = cleaned + ['-target', 'i386-unknown-linux-gnu', '-malign-double', '-mms-bitfields',
                          '-nostdinc++', '-isystem', sdk + '/usr/include/c++/v1', '-isystem', sdk + '/usr/include',
                          '-D__APPLE__', '-DKISAK_LAYOUT_I386']
    r = subprocess.run(args + extra + [src], cwd=cwd, capture_output=True, text=True, errors='replace')
    return r.stdout

HEAD = re.compile(r'^\s*0 \| (struct|union|class) (.*)$')
FIELD = re.compile(r'^\s*(\d+)(?::\d+-\d+)? \|(\s+)(.*?)\s*$')
SIZE = re.compile(r'\[sizeof=(\d+), dsize=\d+, align=(\d+)')

def parse(text):
    recs = {}
    for block in text.split('*** Dumping AST Record Layout')[1:]:
        lines = block.strip('\n').split('\n')
        m = HEAD.match(lines[0])
        if not m: continue
        name = re.sub(r' at [^)]*\)', ')', m.group(2)).strip()
        fields, size = [], None
        for l in lines[1:]:
            s = SIZE.search(l)
            if s: size = (int(s.group(1)), int(s.group(2))); break
            f = FIELD.match(l)
            if f and len(f.group(2)) == 3:      # direct members only
                fields.append((int(f.group(1)), f.group(3)))
        recs.setdefault(name, (size, fields))
    return recs

def main():
    argv = sys.argv[1:]
    only, jout = None, None
    if '--only' in argv:
        i = argv.index('--only'); only = set(argv[i + 1].split(',')); del argv[i:i + 2]
    if '--json' in argv:
        i = argv.index('--json'); jout = argv[i + 1]; del argv[i:i + 2]
    build, srcs = argv[0], [os.path.realpath(s) for s in argv[1:]]
    a32, a64 = {}, {}
    for src in srcs:
        args, cwd = compile_cmd(build, src)
        a32.update(parse(dump(args, cwd, src, True, only)))
        a64.update(parse(dump(args, cwd, src, False, only)))
    diffs = {}
    for name in sorted(set(a32) & set(a64)):
        if only and name not in only: continue
        (s32, f32), (s64, f64) = a32[name], a64[name]
        if s32 is None or s64 is None: continue
        if s32[0] == s64[0] and [o for o, _ in f32] == [o for o, _ in f64]: continue
        first = next(((x, y) for x, y in zip(f32, f64) if x[0] != y[0]), None)
        diffs[name] = {'size32': s32[0], 'size64': s64[0], 'firstMoved': first}
    for n, d in diffs.items():
        fm = d['firstMoved']
        print('%-50s %6d -> %6d   %s' % (n[:50], d['size32'], d['size64'],
              ('first moved: %s @%d -> %s @%d' % (fm[0][1], fm[0][0], fm[1][1], fm[1][0])) if fm else ''))
    if only:
        missing = only - (set(a32) & set(a64))
        if missing: print('not seen:', ' '.join(sorted(missing)))
    if jout: json.dump(diffs, open(jout, 'w'), indent=1)
    print('%d records compared, %d differ' % (len(set(a32) & set(a64)), len(diffs)), file=sys.stderr)

main()
