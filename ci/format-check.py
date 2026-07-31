#!/usr/bin/env python3
"""`--format` must not change what a program means.

The other formatter property -- idempotence -- is a text comparison and lives in
`check.sh`. This one needs the compiler's own judgment, twice, so it is here.

Positions are dropped before comparing: reformatting moves lines, and moving
lines is the entire point. Everything else must be identical, including which
invariants broke and in which column.
"""
import json, os, pathlib, subprocess, sys, tempfile

root = pathlib.Path(__file__).resolve().parent.parent
os.chdir(root)


def verdict(path, genie=None):
    argv = ['./loophole', '--json'] + (['--genie', genie] if genie else []) + [path]
    d = json.loads(subprocess.run(argv, capture_output=True, text=True).stdout)
    for w in d.get('wishes', []):
        for moved in ('wrote', 'line', 'refused_by'):
            w.pop(moved, None)
    for moved in ('symbols', 'file', 'genie'):
        d.pop(moved, None)
    return d


def genie_of(src):
    for line in src.split('\n'):
        if line.startswith('# genie:'):
            return line.split(':', 1)[1].strip()
    return None


bad = 0
with tempfile.TemporaryDirectory() as tmp:
    for f in sorted(root.glob('examples/*.wish')):
        g = genie_of(f.read_text())
        out = subprocess.run(['./loophole', '--format', str(f)],
                             capture_output=True, text=True)
        if out.returncode:
            print(f'FAIL --format failed on {f}'); bad = 1; continue
        formatted = os.path.join(tmp, 'f.wish')
        open(formatted, 'w').write(out.stdout)
        if verdict(str(f), g) != verdict(formatted, g):
            print(f'FAIL formatting changed the verdict of {f}'); bad = 1

    # A genie is judged by what it does to a wish, so reformat it and re-judge
    # the same wish against both.
    probe = 'examples/08_eternal_sleep.wish'
    for f in sorted(root.glob('genie/*.genie')):
        out = subprocess.run(['./loophole', '--format', str(f)],
                             capture_output=True, text=True)
        if out.returncode:
            print(f'FAIL --format failed on {f}'); bad = 1; continue
        formatted = os.path.join(tmp, 'f.genie')
        open(formatted, 'w').write(out.stdout)
        if verdict(probe, str(f)) != verdict(probe, formatted):
            print(f'FAIL formatting changed the verdict of {f}'); bad = 1

if not bad:
    print('  ok   --format preserves every verdict')
sys.exit(bad)
