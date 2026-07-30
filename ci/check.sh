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
  ./loophole --keywords | python3 -c 'import json,sys; json.load(sys.stdin)' 2>/dev/null \
    || { echo "FAIL --keywords is not valid JSON"; rc=1; }
  # A diagnostic must be reachable without reading the prose. An editor drawing a
  # squiggle needs a line and a column, and §10.1 says the prose may be reworded
  # freely -- so a consumer scraping it breaks the day someone improves a
  # message. `--json` therefore reports the error too, on stdout, so a caller has
  # exactly one thing to parse whether the run succeeded or not.
  et=$(mktemp -d)
  printf 'register wishes : uint<2> = 3\nwish x { sub wishes, 1; }\n' > "$et/e.wish"
  # Captured, not piped. `loophole` exits 2 here because that is the correct code
  # for "could not be judged", and under `pipefail` that becomes the pipeline's
  # status -- so a piped version reports failure even when the JSON is perfect.
  # This is the third time that trap has been walked into in this file; the
  # warning six lines above did not stop it.
  ejson=$(./loophole --json "$et/e.wish" 2>/dev/null)
  printf '%s' "$ejson" | python3 -c '
import json, sys
d = json.load(sys.stdin)
e = d["error"]
assert e["line"] == 2, e["line"]
assert e["column"] == 23, e["column"]
assert "\x27;\x27" in e["message"], e["message"]
assert e["help"] and e["source"]
' 2>/dev/null || { echo "FAIL --json does not report the error structurally"; rc=1; }
  rm -rf "$et"
  # A syntax error is "could not be judged" = 2, never 1. It used to exit 1,
  # which tells a script that a file that does not even parse had found a hole
  # in the genie -- and this suite only ever checked the missing-file path, so
  # nothing caught it. One case per way of failing, not one per exit code.
  tmp=$(mktemp -d)
  printf 'register wishes : uint<2> = 3\nwish x { sub wishes, 1; }\n' > "$tmp/lex.wish"
  printf 'register wishes : uint<99> = 3\nwish x { sub wishes, 1 }\n'  > "$tmp/width.wish"
  printf 'register wishes : uint<2> = 3\nwish x { sub wishes\n'      > "$tmp/parse.wish"
  for bad in lex width parse; do
    ./loophole "$tmp/$bad.wish" >/dev/null 2>&1
    [ $? -eq 2 ] || { echo "FAIL exit code: $bad error should be 2"; rc=1; }
  done
  # A verb that denotes no operation is a compile error (§6.1), not a refusal.
  # It used to be reported as the genie refusing, and the run would finish and
  # exit 0 -- telling a script that a file which never executed was judged clean.
  printf 'register wishes : uint<2> = 3\nwish x { sube wishes, 1 }\n' > "$tmp/verb.wish"
  ./loophole "$tmp/verb.wish" >/dev/null 2>&1
  [ $? -eq 2 ] || { echo "FAIL exit code: an unknown verb should be 2"; rc=1; }
  # ...but R0, a definition cycle, IS a refusal: §7.2 names it as a rule. The
  # wish is ILLEGAL, the world is unchanged, and the run continues.
  printf 'register wishes : uint<2> = 3\nwish x { define a := b\ndefine b := a\na wishes, 1 }\n' > "$tmp/cyc.wish"
  ./loophole "$tmp/cyc.wish" >/dev/null 2>&1
  [ $? -eq 0 ] || { echo "FAIL exit code: an R0 cycle is a refusal, not an error"; rc=1; }
  printf 'counter wishes\ntoll 1\nrule R { layer nope forbid add }\n' > "$tmp/bad.genie"
  ./loophole --genie "$tmp/bad.genie" examples/01_humble.wish >/dev/null 2>&1
  [ $? -eq 2 ] || { echo "FAIL exit code: a bad genie should be 2"; rc=1; }
  # A register value must survive being read by JavaScript. It is a uint64 and a
  # JSON *number* is a double to every browser there is, so the exact result of
  # the wrapping subtraction -- the one number this language exists to produce --
  # would come back as 18446744073709552000. Asserted through `node`, not
  # python3, because python's ints are arbitrary precision and would pass while
  # every actual consumer failed.
  if command -v node >/dev/null; then
    rj="$(./loophole --json examples/07_the_original.wish)"
    printf '%s' "$rj" | node -e '
let s = ""; process.stdin.on("data", d => s += d).on("end", () => {
  const d = JSON.parse(s);
  const last = d.wishes[d.wishes.length - 1].registers.wishes;
  if (typeof last.value !== "string") {
    console.log("FAIL --json register value is not a string"); process.exit(1);
  }
  if (last.value !== "18446744073709551615") {
    console.log("FAIL --json lost the exact value: " + last.value); process.exit(1);
  }
  if (last.width !== 64) {
    console.log("FAIL --json reports width " + last.width + ", not the widened 64");
    process.exit(1);
  }
});' || rc=1
  fi
  # A genie checked on its own: parses -> 0 and silent about wishes (there are
  # none), does not parse -> 2 with the same structured error a wish would get.
  # This is what lets an editor squiggle a `.genie` nobody has run yet.
  ok_out="$(./loophole --check-genie genie/mortal.genie)"; [ $? -eq 0 ] \
    || { echo "FAIL --check-genie: a good genie should exit 0"; rc=1; }
  case "$ok_out" in *"genie ok"*) ;; *) echo "FAIL --check-genie: no ok line"; rc=1;; esac
  ./loophole --check-genie genie/mortal.genie 2>&1 | grep -qi 'wish\|invariant .* holds' \
    && { echo "FAIL --check-genie judged a wish"; rc=1; }
  printf 'counter wishes\ntoll 1\nrule R\n  layer ast\n' > "$tmp/nobrace.genie"
  gerr="$(./loophole --json --check-genie "$tmp/nobrace.genie" 2>/dev/null)"
  [ $? -eq 2 ] || { echo "FAIL --check-genie: a broken genie should exit 2"; rc=1; }
  printf '%s' "$gerr" | python3 -c '
import json, sys
e = json.load(sys.stdin)["error"]
assert e["line"] and e["column"], e
assert e["source"], e
' 2>/dev/null || { echo "FAIL --check-genie: no structured error"; rc=1; }
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
