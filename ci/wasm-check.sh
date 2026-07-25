#!/usr/bin/env bash
# Does the browser build judge exactly as the native one does?
#
# This is the only question that matters about the wasm build. A playground that
# disagrees with the compiler -- on one verdict, one exit code, one line of a
# diagnostic -- would be a second implementation of the language, and §9.3 says
# two conforming implementations must agree verbatim on every program.
#
# So nothing here is a golden file. Every case runs BOTH binaries with identical
# arguments and diffs them against each other. A change to the report changes
# both at once and this suite stays silent, which is correct: it is not testing
# the wording, it is testing that two builds of one compiler agree.
set -uo pipefail
cd "$(dirname "$0")/.."

NATIVE=./loophole
WASM="node loophole.node.js"
rc=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Run both, compare stdout+stderr and the exit code.
both() {
  local label="$1"; shift
  $NATIVE "$@" > "$tmp/n.txt" 2>&1; local rn=$?
  $WASM   "$@" > "$tmp/w.txt" 2>&1; local rw=$?
  if [ "$rn" != "$rw" ]; then
    echo "FAIL $label — exit codes differ: native $rn, wasm $rw"
    echo "::error::$label exit codes differ: native $rn, wasm $rw"
    rc=1; return
  fi
  if ! diff -u "$tmp/n.txt" "$tmp/w.txt" > "$tmp/d.txt"; then
    echo "FAIL $label — output differs:"; sed -n '1,30p' "$tmp/d.txt"
    # As an annotation too: those are readable without auth, the logs are not.
    echo "::error::$label differs: $(sed -n '3,8p' "$tmp/d.txt" | tr '\n' ' ')"
    rc=1; return
  fi
  return 0
}

n=0
for f in examples/*.wish; do
  g=$(sed -n 's/^# genie: *//p' "$f" | head -1)
  if [ -n "$g" ]; then both "judge $(basename "$f")" --genie "$g" "$f"
  else                 both "judge $(basename "$f")" "$f"; fi
  n=$((n+1))
done
[ $rc -eq 0 ] && echo "ok   $n examples judged identically"

# The searcher is the real test: it walks millions of programs, so an
# arithmetic or ordering difference between the two builds that no single
# example happens to reach still shows up in the shape list.
hrc=0
for spec in \
  "::examples/01_humble.wish:--max-stmts 3 --max-wishes 4" \
  "::examples/07_the_original.wish:--max-stmts 2 --max-wishes 4" \
  "genie/mortal.genie::examples/08_eternal_sleep.wish:--max-stmts 2 --max-wishes 2" \
  "genie/careful.genie::examples/08_eternal_sleep.wish:--max-stmts 2 --max-wishes 2" \
  "genie/vigil.genie::examples/04_nobody.wish:--max-stmts 2 --max-wishes 2"
do
  g="${spec%%::*}"; rest="${spec#*::}"; w="${rest%%:*}"; bounds="${rest#*:}"
  # shellcheck disable=SC2086
  if [ -n "$g" ]; then both "hunt $(basename "$w") [$(basename "$g")]" --genie "$g" --hunt "$w" $bounds
  else                 both "hunt $(basename "$w")" --hunt "$w" $bounds; fi
  [ $rc -ne 0 ] && hrc=1
done
[ $hrc -eq 0 ] && [ $rc -eq 0 ] && echo "ok   fingerprints identical"

# The error paths, which are the ones that changed to make the browser build
# possible at all: a diagnostic throws now rather than calling exit(), because
# exit() in wasm tears down the runtime and the page would work exactly once.
printf 'register wishes : uint<2> = 3\nwish x { sub wishes, 1; }\n'  > "$tmp/lex.wish"
printf 'register wishes : uint<99> = 3\nwish x { sub wishes, 1 }\n'  > "$tmp/width.wish"
printf 'register wishes : uint<2> = 3\nwish x { sube wishes, 1 }\n'  > "$tmp/verb.wish"
printf 'register wishes : uint<2> = 3\nwish x { sub wishes\n'        > "$tmp/parse.wish"
erc=0
for bad in lex width verb parse; do
  both "error $bad" "$tmp/$bad.wish" || erc=1
done
[ $erc -eq 0 ] && [ $rc -eq 0 ] && echo "ok   diagnostics identical"

exit $rc
