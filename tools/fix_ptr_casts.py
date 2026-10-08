#!/usr/bin/env python3
"""Mechanical 64-bit fixes for pointer <-> 32-bit int casts (see universal/ptr32.h).

Compiles each file with the build's flags, then rewrites the sites clang
reports:

  (T *)intExpr         -> (T *)Ptr32_Decode(intExpr)
  (int)ptrExpr         -> (int)Ptr32_Encode(ptrExpr)
  printf(..., ptr32)   -> printf(..., (T *)ptr32)      (Ptr32<T> through varargs)

Casts to a pointer-to-pointer type are only reported: whether the memory they
point at holds native or 32-bit pointers is a per-site decision.

  tools/fix_ptr_casts.py <build_dir> [--dry] <file.cpp>...
"""
import json, os, re, shlex, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

DIAG = re.compile(r'^(?P<file>/[^:]+):(?P<line>\d+):(?P<col>\d+):(?P<ranges>(?:\{\d+:\d+-\d+:\d+\})*):? error: (?P<msg>.*)$')
RANGE = re.compile(r'\{(\d+):(\d+)-(\d+):(\d+)\}')
INT_TO_PTR = re.compile(r"cast to '(?P<to>[^']+)' from smaller integer type")
PTR_TO_INT = re.compile(r"cast to smaller integer type '(?P<to>[^']+)' from '(?P<from>[^']+)'")
VARARGS = re.compile(r"passing object of class type '(?:const )?Ptr32<(?P<t>.*)>' through variadic")


def compile_errors(build, f, cmds):
    e = cmds[f]
    args, out, skip = shlex.split(e['command']), [], False
    for a in args:
        if skip: skip = False; continue
        if a in ('-o', '-MF', '-MT', '-c'): skip = True; continue
        if a == '-MD' or os.path.realpath(a) == f: continue
        out.append(a)
    r = subprocess.run(out + ['-fsyntax-only', '-fdiagnostics-print-source-range-info', '-fno-caret-diagnostics', f],
                       cwd=e['directory'], capture_output=True, text=True, errors='replace')
    return [m.groupdict() for m in map(DIAG.match, r.stderr.split('\n')) if m]


class Source:
    def __init__(self, path):
        self.path = path
        self.data = open(path, 'rb').read().decode('latin-1')
        self.line_starts = [0]
        for i, ch in enumerate(self.data):
            if ch == '\n':
                self.line_starts.append(i + 1)

    def offset(self, line, col):
        return self.line_starts[line - 1] + col - 1


def match_paren(text, i):
    """Index just past the parenthesis group starting at text[i] == '('."""
    depth = 0
    for j in range(i, len(text)):
        if text[j] == '(':
            depth += 1
        elif text[j] == ')':
            depth -= 1
            if depth == 0:
                return j + 1
    return -1


def cast_operand(text):
    """For a cast expression's text, the (start, end) of its operand."""
    t = text.lstrip()
    lead = len(text) - len(t)
    if t.startswith('('):
        close = match_paren(t, 0)
        if close < 0:
            return None
        start = close
        while start < len(t) and t[start] in ' \t':
            start += 1
        return lead + start, len(text)
    m = re.match(r'(reinterpret_cast|static_cast)\s*<', t)
    if m:
        depth, j = 0, m.end() - 1
        while j < len(t):
            if t[j] == '<': depth += 1
            elif t[j] == '>':
                depth -= 1
                if depth == 0: break
            j += 1
        k = t.find('(', j)
        end = match_paren(t, k) if k >= 0 else -1
        if end != len(t):
            return None
        return lead + k + 1, lead + end - 1
    return None


def wrap(ins, data, start, end, fn):
    """Insert fn(...) around data[start:end], reusing the operand's own parens."""
    op = data[start:end]
    if op.startswith('(') and match_paren(op, 0) == len(op):
        ins.append((start, 1, fn))
    else:
        ins.append((start, 1, fn + '('))
        ins.append((end, 0, ')'))


def arg_end(text, i):
    depth, quote = 0, None
    j = i
    while j < len(text):
        ch = text[j]
        if quote:
            if ch == '\\':
                j += 1
            elif ch == quote:
                quote = None
        elif ch in '"\'':
            quote = ch
        elif ch in '([{':
            depth += 1
        elif ch in ')]}':
            if depth == 0:
                return j
            depth -= 1
        elif ch == ',' and depth == 0:
            return j
        j += 1
    return -1


def arg_start(text, i):
    depth = 0
    j = i - 1
    while j >= 0:
        ch = text[j]
        if ch in ')]}':
            depth += 1
        elif ch in '([{':
            if depth == 0:
                break
            depth -= 1
        elif ch == ',' and depth == 0:
            break
        j -= 1
    j += 1
    while j < i and text[j] in ' \t\r\n':
        j += 1
    return j


def simple_expr(t):
    return re.fullmatch(r'[\w.\[\]>-]+(\([^()]*\))?', t.replace('->', '>')) is not None


def main():
    argv = sys.argv[1:]
    dry = '--dry' in argv
    argv = [a for a in argv if a != '--dry']
    build = argv[0]
    files = [os.path.realpath(f) for f in argv[1:]]
    cmds = {os.path.realpath(e['file']): e for e in json.load(open(os.path.join(build, 'compile_commands.json')))}

    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        results = list(pool.map(lambda f: compile_errors(build, f, cmds), [f for f in files if f in cmds]))

    sites = {}
    for errs in results:
        for d in errs:
            key = (d['file'], d['line'], d['col'], d['msg'])
            sites[key] = d

    insertions = {}   # path -> list of (offset, order, text)
    report = []
    sources = {}
    for (path, line, col, msg), d in sorted(sites.items()):
        rs = RANGE.findall(d['ranges'])
        src = sources.setdefault(path, Source(path))
        if rs:
            l0, c0, l1, c1 = map(int, rs[0])
            s, e = src.offset(l0, c0), src.offset(l1, c1)
        elif VARARGS.search(msg):
            # No range for a varargs argument (clang points inside it): it runs
            # between the enclosing top-level ',' / '(' and ',' / ')'.
            p0 = src.offset(int(line), int(col))
            s = arg_start(src.data, p0)
            e = arg_end(src.data, p0)
            if e < 0:
                report.append('%s:%s: unparsed argument' % (path, line)); continue
        else:
            continue
        text = src.data[s:e]
        ins = insertions.setdefault(path, [])
        m = INT_TO_PTR.search(msg)
        if m:
            if m.group('to').rstrip().endswith('**') or '(*' in m.group('to'):
                report.append('%s:%s: pointer-to-pointer cast, review: %s' % (path, line, text.strip()))
                continue
            op = cast_operand(text)
            if not op:
                report.append('%s:%s: unparsed cast: %s' % (path, line, text.strip())); continue
            wrap(ins, src.data, s + op[0], s + op[1], 'Ptr32_Decode')
            continue
        m = PTR_TO_INT.search(msg)
        if m:
            op = cast_operand(text)
            if not op:
                report.append('%s:%s: unparsed cast: %s' % (path, line, text.strip())); continue
            wrap(ins, src.data, s + op[0], s + op[1], 'Ptr32_Encode')
            continue
        m = VARARGS.search(msg)
        if m:
            t = m.group('t').strip()
            arg = text.strip()
            lead = len(text) - len(text.lstrip())
            if simple_expr(arg):
                ins.append((s + lead, 1, '(%s *)' % t))
            else:
                ins.append((s + lead, 1, '(%s *)(' % t))
                ins.append((s + lead + len(arg), 0, ')'))
            continue

    changed = 0
    for path, ins in insertions.items():
        if not ins:
            continue
        data = sources[path].data
        # Apply from the end; at one offset, closers go before openers.
        for off, order, txt in sorted(set(ins), key=lambda x: (-x[0], x[1])):
            data = data[:off] + txt + data[off:]
        if dry:
            print('would change %s (%d insertions)' % (path, len(ins)))
        else:
            open(path, 'wb').write(data.encode('latin-1'))
        changed += 1
    for r in report:
        print(r)
    print('%d sites, %d files changed, %d left for review' % (len(sites), changed, len(report)), file=sys.stderr)


main()
