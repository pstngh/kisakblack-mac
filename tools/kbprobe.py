"""Shared helpers for the tools/ audit scripts: compile commands and sizeof probes."""
import json, os, re, shlex, subprocess, tempfile

_SDK = None


def sdk():
    global _SDK
    if _SDK is None:
        _SDK = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()
    return _SDK


def compile_commands(build):
    return {os.path.realpath(e['file']): e for e in json.load(open(os.path.join(build, 'compile_commands.json')))}


def compile_args(entry, path, i386):
    """The file's compile arguments without output/dependency flags, natively or for i386."""
    args, out, skip = shlex.split(entry['command']), [], False
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
                '-isystem', sdk() + '/usr/include/c++/v1', '-isystem', sdk() + '/usr/include', '-D__APPLE__',
                '-Wno-int-to-pointer-cast']
    return out


def sizeof(cmds, path, types, i386):
    """{type: sizeof} for types visible at the end of `path` (i386 or native)."""
    e = cmds[path]
    fd, tmp = tempfile.mkstemp(suffix='.cpp')
    types = list(types)
    with os.fdopen(fd, 'w') as f:
        f.write('#include "%s"\ntemplate <int K, unsigned long N> struct KBSZ;\n' % path)
        for k, t in enumerate(types):
            f.write('KBSZ<%d, sizeof(%s)> kbsz_%d;\n' % (k, t, k))
    r = subprocess.run(compile_args(e, path, i386) + ['-fsyntax-only', '-Wno-everything', '-ferror-limit=0', tmp],
                       cwd=e['directory'], capture_output=True, text=True, errors='replace')
    os.unlink(tmp)
    got = {int(k): int(n) for k, n in re.findall(r'KBSZ<(\d+), (\d+)', r.stderr)}
    return {types[k]: n for k, n in got.items()}
