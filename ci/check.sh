#!/usr/bin/env bash
# Regression suite.
#
# Two goldens, because they catch different things:
#
#   examples.txt      the full prose report for every example. Catches any
#                     change to a verdict, a trace line, or the wording.
#   fingerprints.txt  the hunter's output on several worlds. This is the strong
#                     one: the shape list and per-shape counts are a fingerprint
#                     of the SEMANTICS, so an accidental change to how the
#                     machine behaves shows up here even when every example
#                     still passes.
#
# `./ci/check.sh --update` rewrites the goldens. Do that only when the change is
# intentional, and read the diff before you commit it.
set -uo pipefail
cd "$(dirname "$0")/.."

UPDATE=0
[ "${1:-}" = "--update" ] && UPDATE=1

gen_examples() { make -s run; }

gen_fingerprints() {
  for spec in \
    "::examples/01_humble.wish:--max-stmts 3 --max-wishes 4" \
    "::examples/07_the_original.wish:--max-stmts 2 --max-wishes 4" \
    "genie/mortal.genie::examples/08_eternal_sleep.wish:--max-stmts 2 --max-wishes 2" \
    "genie/careful.genie::examples/08_eternal_sleep.wish:--max-stmts 2 --max-wishes 2" \
    "genie/vigil.genie::examples/04_nobody.wish:--max-stmts 2 --max-wishes 2"
  do
    g="${spec%%::*}"; rest="${spec#*::}"
    w="${rest%%:*}"; bounds="${rest#*:}"
    echo "### ${g:-(built-in)} | $w | $bounds"
    # shellcheck disable=SC2086
    ./loophole ${g:+--genie $g} --hunt "$w" $bounds || true
    echo
  done
}

check_one() {
  local name="$1" golden="ci/expected/$1.txt"
  local actual; actual="$(gen_"$1")"
  if [ "$UPDATE" = "1" ]; then
    mkdir -p ci/expected; printf '%s\n' "$actual" > "$golden"
    echo "updated $golden"; return 0
  fi
  if [ ! -f "$golden" ]; then echo "MISSING $golden (run ./ci/check.sh --update)"; return 1; fi
  if diff -u "$golden" <(printf '%s\n' "$actual") > /tmp/loophole-$name.diff 2>&1; then
    echo "ok   $name"
    return 0
  fi
  echo "FAIL $name — output changed:"; sed -n '1,60p' /tmp/loophole-$name.diff
  return 1
}

rc=0
check_one examples     || rc=1
check_one fingerprints || rc=1

# The exit-code contract other tools depend on.
if [ "$UPDATE" = "0" ]; then
  ./loophole examples/00_naive.wish >/dev/null 2>&1; [ $? -eq 0 ] || { echo "FAIL exit code: expected 0 (no exploit)"; rc=1; }
  ./loophole examples/01_humble.wish >/dev/null 2>&1; [ $? -eq 1 ] || { echo "FAIL exit code: expected 1 (exploit)"; rc=1; }
  ./loophole /nonexistent.wish        >/dev/null 2>&1; [ $? -eq 2 ] || { echo "FAIL exit code: expected 2 (missing file)"; rc=1; }
  # A syntax error is "could not be judged" = 2, never 1. It used to exit 1,
  # which tells a script that a file that does not even parse had found a hole
  # in the genie -- and this suite only ever checked the missing-file path, so
  # nothing caught it. One case per way of failing, not one per exit code.
  tmp=$(mktemp -d)
  printf 'register w : uint<2> = 3\nwish x { sub w, 1; }\n' > "$tmp/lex.wish"
  printf 'register w : uint<99> = 3\nwish x { sub w, 1 }\n'  > "$tmp/width.wish"
  printf 'register w : uint<2> = 3\nwish x { sub w\n'         > "$tmp/parse.wish"
  for bad in lex width parse; do
    ./loophole "$tmp/$bad.wish" >/dev/null 2>&1
    [ $? -eq 2 ] || { echo "FAIL exit code: $bad error should be 2"; rc=1; }
  done
  printf 'counter w\ntoll 1\nrule R { layer nope forbid add }\n' > "$tmp/bad.genie"
  ./loophole --genie "$tmp/bad.genie" examples/01_humble.wish >/dev/null 2>&1
  [ $? -eq 2 ] || { echo "FAIL exit code: a bad genie should be 2"; rc=1; }
  # Escape codes must never reach a pipe: the goldens and every grep downstream
  # are plain text.
  if ./loophole "$tmp/lex.wish" 2>&1 | grep -q $'\033'; then
    echo "FAIL colour leaked into a non-tty"; rc=1
  fi
  rm -rf "$tmp"
  # loophole exits 1 here (an exploit was found), which is correct — so capture the
  # output first rather than piping, or pipefail would read that as a failure.
  json_out="$(./loophole --json examples/01_humble.wish)"
  printf '%s' "$json_out" | python3 -c 'import json,sys; json.load(sys.stdin)' 2>/dev/null \
    || { echo "FAIL --json is not valid JSON"; rc=1; }
  [ $rc -eq 0 ] && echo "ok   contract (exit codes, --json)"
fi

exit $rc
