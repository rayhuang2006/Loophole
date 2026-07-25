# Changelog

The compiler and the two languages it reads are versioned separately. A bug fix
moves the compiler; a new concept moves whichever language grew it.
`loophole --version` prints all three.

Releases before 1.1.0 were published under the compiler's old name, `wishc`.

## loophole 1.2.0 — wish 1.0, genie 1.0

**The report was rewritten.** It was written by someone who already knew the
answers, and the first reader who did not could not tell what it was saying.
Neither language changed; §10.1 states that the prose report is not part of the
tool contract, and `--json` is unchanged.

- **A rule and an invariant no longer look alike.** The old header listed them in
  one comma-separated line — `genie: toll=1, NoMoreCandy[surface], NotTooMuch` —
  so a gate that can refuse a wish and a ruler that only measures afterwards were
  indistinguishable. They are now separate lines, labelled `refuses` and `holds`,
  and a rule says in words which program its layer reads.
- **Each wish is reported in the order §7 performs the steps**: rules, toll,
  execution, checks, verdict. The old report streamed the execution trace and
  then announced `STATUS: LEGAL` beneath it, which read as though the wish had
  been run and then approved — backwards, and it made the toll look like a fee
  charged for something already done.
- **`FOOLED` shows both columns.** It used to print `FOOLED (2 in scope, all
  hold)` followed by a sentence fragment, leaving the reader to work out which
  half held. Both formulas and both results are now given, which is the whole
  explanation: the genie's wording held, the thing it was protecting did not.
- **`VIOLATED` no longer invents a result for the `real` column.** It was
  printing `real ... holds` next to evidence that it failed. The `real` column is
  not evaluated once the written one fails (§8.4), so nothing is claimed for it.
- The world section lists people and their attributes, not only registers.
- The report is entirely English now; one Chinese sentence used to be mixed into
  the verdict line.

## loophole 1.1.1 — wish 1.0, genie 1.0

- **The world banner printed only the genie's counter register.** A world with a
  second register showed it nowhere, while the report below went on to discuss
  its value — the reader was told about a number the header never mentioned. It
  stayed invisible for as long as every bundled example happened to declare
  exactly one register. All registers are now listed, in declaration order.
- **Releases now also carry an unversioned copy of each binary.** The install
  line in the README points at `releases/latest/download/`, which is the only
  URL that stays correct across versions — but it resolves to a *filename*, and
  a filename with the version in it stops existing the moment a new version
  ships. The versioned copies remain, for checksums and for archiving.

## loophole 1.1.0 — wish 1.0, genie 1.0

**The compiler is now `loophole`, not `wishc`.** Loophole was being used as the
name of a single language; it is better used as the name of the compiler, which
reads two languages — *wish* (`.wish`, the player's) and *genie* (`.genie`, the
policy). Those two now carry their own version numbers, because they can grow
independently. Neither language changed in this release.

This is a breaking change to the tool contract of §10.1: the binary, the
`--version` string, and one `--json` key are all different. It is 1.1.0 rather
than 2.0.0 only because nothing depended on the old contract yet — the name was
four days old.

- The binary is `loophole`; `make install` puts it on `PATH`, so it is
  `loophole a.wish` rather than `./wishc a.wish`.
- `--version` prints `loophole 1.1.0  (wish 1.0, genie 1.0)`.
- `--json` replaces `"wishc"`/`"language"` with `"loophole"` and a nested
  `"languages"` object. Nested, because `"genie"` at the top level already names
  the policy file and a duplicate key would make the object ambiguous.

### The `grounded` rule layer is gone

The specification described three rule layers, where `ast` read a resolved verb
and `grounded` additionally read resolved arguments. The implementation never
had three: `ast` resolved both, and always had. The third name described a
distinction that does not exist, so it is removed rather than invented. There are
two layers because there are exactly two programs — the one you submitted and
the one the machine runs.

### Specification corrections

These were found by a full audit, not by a failing test; each is a place where
the document said something the implementation did not do, or left something
load-bearing unsaid.

- **The default genie is now in the specification** (Appendix D). Appendix A is a
  normative acceptance criterion stated in terms of `I1` and `I2`, which existed
  only inside the compiler — the criterion could not be checked from the document
  alone. The same fix retires the phrase "rule (R0/…/Rn)", which had leaked the
  default genie's own rule names into prose about every genie.
- **`everyone` and `alive(p)` are specified** (§8.6). `everyone` is pre-bound to
  the population but is a rebindable definition; `alive(p)` is false for a person
  with no declared attributes, because an empty disjunction is false. That last
  one is left as-is deliberately: it is the exact dual of the vacuously-true
  empty universal, and exempting one while celebrating the other would be
  dishonest.
- **`label` is listed as a genie keyword** (§3). It was in the grammar and in the
  compiler, but not in the reserved list.
- **The verdict table distinguishes levels** (§9.1). `LEGAL`/`ILLEGAL` describe a
  wish, `holds`/`VIOLATED`/`FOOLED` describe an invariant, and `EXPLOIT` is
  derived from both; the old single table invited reading them as one scale.
- **`alive(p)` is described consistently** — §8.4 called it a genie-defined
  concept while §11.4 called it a built-in.
- The embedded genie's own comment said "the five operations". There are six.

## loophole 1.0.1 — language 1.0

Two defects that only appeared off the author's machine. Both were found by CI
on its first run, and neither was reachable on macOS.

- **The searcher's shape ordering was not deterministic across platforms.** The
  comparator ordered by size alone, and `std::sort` is not stable, so shapes of
  equal size could come out in either order — libc++ and libstdc++ disagreed.
  This contradicted §9.3 of the specification, which requires two conforming
  implementations to agree verbatim. The ordering is now a strict total order.
- **`expandProgram` indexed the operation table with `-1` for a promise.** A
  promise carries a formula rather than an operation, and the canonicalisation
  pass had no branch for it, so it fell through to the operation branch and read
  out of bounds. Harmless on macOS, a segfault on Linux. Present since promises
  were added.

CI now also builds under AddressSanitizer and UndefinedBehaviorSanitizer and
searches every world, which is what caught the second one.

**Do not use 1.0.0 on Linux**; `--hunt` crashes there.

## loophole 1.0.0 — language 1.0

First release. The language as specified in
[docs/spec/loophole-1.0.md](docs/spec/loophole-1.0.md).

- Registers with wrapping fixed-width arithmetic; `sub`, `add`, `widen`.
- People with named numeric attributes; `set`, `kill`, `revive`.
- Rebindable definitions, and rules that read either the submitted text or the
  resolved program — the difference is the aliasing axis.
- Promises, the genie's two meta-axioms, and a DPLL consistency engine.
- Genie policies as data: concepts, layered rules, two-column invariants.
- `--hunt`, bounded exhaustive search with minimal-witness normalisation.
- `--json`, `--version`, and meaningful exit codes: the contract dependants use.
