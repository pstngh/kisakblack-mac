#!/usr/bin/env python3
"""Resolve preprocessor conditionals on the (never-defined) online/Demonware macros.

Every macro in UNDEFINED is treated as not defined: `#ifdef M` bodies are dropped,
`#ifndef M` bodies are kept, and #else/#elif branches are resolved accordingly.
Conditionals on any other macro pass through untouched.
"""
import re, sys

UNDEFINED = {
    'KISAK_DW', 'KISAK_DEMON', 'KISAK_DW_TASK', 'KISAK_DW_STUBS',
    'KISAK_LIVE', 'KISAK_LIVE_STUBS', 'KISAK_LIVE_SERVICE',
    'KISAK_USERINFO', 'KISAK_CAC_STUBS', 'KISAK_STATS',
}

D_IF = re.compile(r'^\s*#\s*(ifdef|ifndef|if)\b\s*(.*)$')
D_ELIF = re.compile(r'^\s*#\s*elif\b\s*(.*)$')
D_ELSE = re.compile(r'^\s*#\s*else\b')
D_ENDIF = re.compile(r'^\s*#\s*endif\b')
SIMPLE = re.compile(r'^\(?\s*(?:defined\s*\(?\s*)?(\w+)\s*\)?\s*\)?\s*(?://.*|/\*.*)?$')

def resolve(kind, expr):
    """Return True/False if the condition only tests an UNDEFINED macro, else None."""
    m = SIMPLE.match(expr.strip())
    if not m or m.group(1) not in UNDEFINED:
        return None
    if kind == 'ifndef':
        return True
    if kind == 'if' and expr.strip().startswith('!'):
        return True
    return False

def process(lines):
    out = []
    # stack entries: dict(managed, keep, taken, parent_emit)
    stack = []
    def emitting():
        return all(e['emit'] for e in stack)
    for line in lines:
        m = D_IF.match(line)
        if m:
            parent = emitting()
            val = resolve(m.group(1), m.group(2))
            if val is None:
                stack.append({'managed': False, 'emit': True})
                if parent: out.append(line)
            else:
                stack.append({'managed': True, 'emit': val, 'taken': val})
            continue
        m = D_ELIF.match(line)
        if m and stack:
            top = stack[-1]
            if top['managed']:
                # An elif after a resolved branch: remaining chain becomes its own #if.
                if top['taken']:
                    top['emit'] = False
                else:
                    val = resolve('if', m.group(1))
                    if val is None:
                        # Hand the rest of the chain back as an unmanaged #if.
                        stack[-1] = {'managed': False, 'emit': True, 'converted': True}
                        if all(e['emit'] for e in stack[:-1]):
                            out.append(re.sub(r'#\s*elif', '#if', line, count=1))
                    else:
                        top['emit'] = val; top['taken'] = val
                continue
            if emitting() or all(e['emit'] for e in stack[:-1]):
                if all(e['emit'] for e in stack[:-1]): out.append(line)
            continue
        if D_ELSE.match(line) and stack:
            top = stack[-1]
            if top['managed']:
                top['emit'] = not top['taken']; top['taken'] = True
                continue
            if all(e['emit'] for e in stack[:-1]): out.append(line)
            continue
        if D_ENDIF.match(line) and stack:
            top = stack.pop()
            if top['managed']:
                continue
            if emitting(): out.append(line)
            continue
        if emitting():
            out.append(line)
    if stack:
        raise SystemExit('unbalanced conditionals')
    return out

changed = 0
for path in sys.argv[1:]:
    with open(path, encoding='utf-8', errors='surrogateescape', newline='') as f:
        text = f.read()
    lines = text.splitlines(keepends=True)
    new = process(lines)
    if new != lines:
        with open(path, 'w', encoding='utf-8', errors='surrogateescape', newline='') as f:
            f.write(''.join(new))
        changed += 1
        print(f'{path}: {len(lines) - len(new)} lines removed')
print(f'{changed} files changed', file=sys.stderr)
