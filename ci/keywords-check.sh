#!/usr/bin/env bash
# Does everything that claims to know the language actually agree with it?
#
# A keyword list is the kind of thing that rots without anybody noticing: the
# language grows, the highlighter does not, and the only symptom is that a word
# stops changing colour. Nothing fails. That is why this exists.
#
# `--keywords` is generated from the tables the lexer and parser read, so it
# cannot disagree with the compiler. What it CAN disagree with is:
#
#   1. the parser's own `word("...")` call sites, for the contextual keywords
#      the genie language recognises by name rather than by token. Those calls
#      are what make the words keywords; the table is a declaration beside them,
#      and this check is what keeps the two honest.
#   2. §3 of the specification, which lists the reserved words in prose.
#   3. any consumer that keeps its own copy. The web page does not: it asks the
#      compiler at load and builds its pattern from the answer, so there is
#      nothing there to fall behind. An editor plugin cannot do that — its
#      grammar is a static file the editor reads — so when one exists it gets
#      checked here the way the specification is.
#
# Only (1) is a property of the compiler. The rest are consumers, and the point
# of the exercise is that a consumer can no longer quietly fall behind.
set -uo pipefail
cd "$(dirname "$0")/.."

rc=0
fail() { echo "FAIL $1"; echo "::error::keywords: $1"; rc=1; }

KW=$(./loophole --keywords)
group() { printf '%s' "$KW" | python3 -c "
import json,sys
d = json.load(sys.stdin)
for k in '$1'.split('+'):
    for w in d[k]: print(w)
" | sort -u; }

# ---- 1. the parser's contextual keywords --------------------------------
# Every word a parser matches by name must be declared, and every declared word
# must actually be matched somewhere. Both directions matter: an undeclared one
# is invisible to editors, a declared-but-unused one is a lie.
#
# Two helpers do the matching, because there are two parsers: `word()` in the
# genie policy parser, `isWord()` in the formula parser on the wish side.
#
# `self` is the one exception and it is not a keyword at all: it is a reserved
# NAME, only meaningful as the argument of `granted(...)`, and it is recognised
# by comparing the name after parsing. It is listed for editors to colour, so it
# is excluded here rather than being quietly dropped from the table.
used=$( { grep -oE '[^a-zA-Z]word\("[a-z_]*"\)'  loophole.cpp
           grep -oE 'isWord\("[a-z_]*"\)'          loophole.cpp
         } | grep -o '"[a-z_]*"' | tr -d '"' | sort -u)
declared=$(group 'genie+expressions' | grep -vx 'self')
if [ "$used" != "$declared" ]; then
  fail "the genie parser and the keyword tables disagree"
  echo "  only in the parser:  $(comm -23 <(echo "$used") <(echo "$declared") | tr '\n' ' ')"
  echo "  only in the tables:  $(comm -13 <(echo "$used") <(echo "$declared") | tr '\n' ' ')"
fi

# The layer names are compared as strings rather than via word(), so they are
# checked against their own call sites.
lused=$(grep -oE 'l == "(surface|ast|[a-z]+)"' loophole.cpp | sed 's/l == "\(.*\)"/\1/' | sort -u)
ldecl=$(group 'layers')
[ "$lused" = "$ldecl" ] || fail "the layer names in the parser and in --keywords differ"

# ---- 2. the specification ------------------------------------------------
# §3 lists the reserved words in prose. It is normative, so a word the compiler
# reserves and the document does not is a defect in the document.
spec=$(sed -n '/^- \*\*Keywords\*\* are reserved/,/^- \*\*Layer names\*\*/p' \
         docs/spec/loophole-1.0.md | grep -o '`[a-z]*`' | tr -d '`' | sort -u)
for w in $(group 'wish+genie'); do
  printf '%s\n' "$spec" | grep -qx "$w" || fail "spec §3 does not list the keyword '$w'"
done

# ---- 3. the web highlighter ---------------------------------------------
# There is nothing to compare: the page asks the compiler at load and builds its
# pattern from the answer. What IS worth checking is that it still does that,
# because reintroducing a literal list would be an easy and invisible mistake.
if grep -qE 'const KW = /\^\(.*register' web/index.html; then
  fail "web/index.html has gone back to a hard-coded keyword list"
fi
grep -q 'setKeywords(d.keywords)' web/index.html \
  || fail "the page no longer takes its keywords from the compiler"

[ $rc -eq 0 ] && echo "ok   keywords agree: parser, spec §3, web highlighter"
exit $rc
