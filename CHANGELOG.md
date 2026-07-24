# Changelog

The compiler and the language are versioned separately. A bug fix moves the
compiler; a new concept moves the language. `wishc --version` prints both.

## wishc 1.0.1 — language 1.0

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

## wishc 1.0.0 — language 1.0

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
