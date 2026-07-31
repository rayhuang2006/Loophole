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


def comments(text):
    """Every comment in the file, in order, with its leading `#` stripped."""
    out = []
    for line in text.split('\n'):
        i = line.find('#')
        if i >= 0:
            out.append(line[i:].strip())
    return out


bad = 0

# A formatter may move a comment. It may not invent one or lose one.
#
# Neither of the other two properties catches this. Idempotence does not: a
# comment duplicated onto two lines by the first pass sits on two separate lines
# by the second, and each keeps one, so the output is stable. And a duplicated
# comment changes no verdict at all. This was a real bug -- a wish header and its
# first statement written on the same source line both claimed that line's
# comment, and it came out twice.
with tempfile.TemporaryDirectory() as tmp:
    cases = {
        'one line': 'register wishes : uint<2> = 3\n'
                    'wish w { sub wishes, 3  # why\n}\n',
        'header and body share a line':
                    'register wishes : uint<2> = 3\n'
                    'wish w {  sub wishes, 3   # why\n }\n',
        'comment at the end': 'register wishes : uint<2> = 3\n# trailing thought\n',
    }
    for name, src in cases.items():
        src_path = os.path.join(tmp, 'c.wish')
        open(src_path, 'w').write(src)
        out = subprocess.run(['./loophole', '--format', src_path],
                             capture_output=True, text=True)
        if out.returncode:
            print(f'FAIL --format failed on the {name!r} case'); bad = 1; continue
        before, after = comments(src), comments(out.stdout)
        if before != after:
            print(f'FAIL comments changed in the {name!r} case:'
                  f'\n       before {before}\n       after  {after}')
            bad = 1

    # And it must land on the right construct. `wish w { sub x, 1  # why` is a
    # note about `sub`, not about the wish -- the header only shared the line.
    # Counting comments cannot see this: the header stealing it keeps the count
    # at one.
    shared = os.path.join(tmp, 'shared.wish')
    open(shared, 'w').write('register wishes : uint<2> = 3\n'
                            'wish w {  sub wishes, 3   # why\n }\n')
    out = subprocess.run(['./loophole', '--format', shared],
                         capture_output=True, text=True).stdout
    owner = next((l for l in out.split('\n') if '#' in l), '')
    if 'sub wishes' not in owner:
        print(f'FAIL the trailing comment left its statement: {owner!r}')
        bad = 1

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
    print('  ok   --format preserves every verdict and every comment')
sys.exit(bad)
