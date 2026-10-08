#!/usr/bin/env python3
"""Drop entries for deleted source files from cmake_files.cmake (the Windows file list).

Removes quoted file entries that no longer exist on disk, then removes any
set(...)/source_group(...) blocks and aggregate references left empty.
"""
import os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(ROOT, 'cmake_files.cmake')
DIRS = {'SRC_DIR': 'src', 'TL_DIR': 'tl'}

def exists(entry):
    m = re.match(r'"\$\{(\w+)\}/(.*)"', entry.strip())
    return not m or os.path.exists(os.path.join(ROOT, DIRS.get(m.group(1), m.group(1)), m.group(2)))

text = open(PATH).read()
lines = [l for l in text.split('\n') if not (l.startswith('\t"') and not exists(l))]
text = '\n'.join(lines)

# Iteratively drop empty set() blocks, their source_group, and references to them.
while True:
    empty = re.findall(r'^set\((\w+)\n\)\n', text, flags=re.M)
    if not empty:
        break
    for var in empty:
        text = re.sub(r'^set\(%s\n\)\n' % var, '', text, flags=re.M)
        text = re.sub(r'^source_group\([^\n]*\$\{%s\}\)\n' % var, '', text, flags=re.M)
        text = re.sub(r'^\t\$\{%s\}\n' % var, '', text, flags=re.M)

while True:
    new = re.sub(r'\n# ----- [^\n]* -----\n\n+(?=# -----|# =====)', '\n\n', text)
    if new == text:
        break
    text = new
text = re.sub(r'\n{3,}', '\n\n', text)
open(PATH, 'w').write(text)
