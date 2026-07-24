# The Loophole Language Specification

**Version 1.0 — Draft**

This document is the normative reference for the Loophole language and its
reference compiler, `wishc`. It defines the syntax and semantics of two file
kinds — the wish (`.wish`) and the genie policy (`.genie`) — precisely enough
that two conforming implementations must agree, verbatim, on the judgment of
every program.

A friendly, Traditional-Chinese companion to this material lives in
[`docs/guide/`](../guide/story.md); where the two disagree, this document wins.

---

## 1. Scope and design intent

### 1.1 What the language is for

A *genie* publishes a fixed rulebook. A *player* writes a *wish*. The compiler
proves the wish **legal** — it violates no rule the genie can refuse — and then
computes whether granting it **breaches** what the genie was actually trying to
protect. A wish is a successful *exploit* if and only if it is legal yet
breaches. The gap between the letter of a rule and its intent is the entire
subject of the language.

The guiding principle is that exploits are **not authored**. Nobody decides that
a 2-bit register cannot hold 4; that is arithmetic. The rigor of the language
therefore rests entirely on its **operational semantics** being small and
completely pinned down, so that whatever falls out of an honest model is a
consequence, not a design.

### 1.2 Non-goals (normative)

Loophole is deliberately **not** a general-purpose language.

- There is **no unbounded iteration and no general recursion.** Every wish
  program halts, and its judgment is decided by executing it once. An
  implementation MUST NOT introduce a construct whose evaluation may fail to
  terminate.
- The language does **not** prove universally-quantified statements about *all*
  wishes. It judges one concrete wish at a time. Reasoning about the whole space
  of wishes is the job of tooling (§ 11.3), not of the language.

These are load-bearing constraints, not omissions: they are what make "run it
once" a complete and rigorous decision procedure.

---

## 2. Notation

Grammar is given in EBNF. `"x"` is a literal terminal. `{ x }` is zero or more
repetitions, `[ x ]` is optional, `|` is alternation, `( )` groups. Terminals
`ident` and `integer` are defined lexically in § 3. Grammar productions are
non-normative where prose in the same section constrains them further; the prose
governs.

Throughout, `w` denotes a bit width, `1 ≤ w ≤ 64`, and arithmetic written
`mod 2^w` is the ordinary residue in `[0, 2^w)`.

---

## 3. Lexical structure

Source text is UTF-8. Tokenization is greedy (longest match).

```
ident   = ( letter | "_" ) { letter | digit | "_" } ;
integer = digit { digit } ;                        (* decimal, unsigned *)
string  = '"' { any-char-except-quote-or-newline } '"' ;
```

- **Comments** begin with `#` and run to end of line. They are discarded.
- **Whitespace** (spaces, tabs, newlines) separates tokens and is otherwise
  insignificant.
- **Keywords** are reserved and may not be used as `ident`:
  `register`, `attribute`, `people`, `wish`, `define`, `promise`, `uint`,
  and, in genie policies, `counter`, `toll`, `concept`, `rule`, `invariant`,
  `layer`, `forbid`, `on`, `because`, `check`, `written`, `real`.
- **Operator and punctuation tokens**:
  `: := , . -> < > = <= >= == != + - ( ) { }`.
- **Word operators** used inside expressions and formulas are `not`, `and`,
  `or`, `implies`, `all`, `in`, `true`, `false`, `granted`, `alive`, `self`,
  `consistent`, `max`, `before`, `toll`. These are contextual: they are ordinary
  identifiers except where the grammar of § 8 places them.

All concrete syntax is ASCII. An implementation MUST NOT accept Unicode
spellings of operators (e.g. `∀`, `≤`); the ASCII forms are the only forms.

---

## 4. Values and the world

### 4.1 Fixed-width unsigned values

A value is a pair `(v, w)`: an integer `v` held in a register of width `w`,
`1 ≤ w ≤ 64`, satisfying the invariant `v = v mod 2^w`. Every operation that
writes a value re-establishes this invariant. Bits shifted out of a width are
gone and do not return when the width later grows.

This is the language's one and only source of arithmetic surprise, and it is
faithful, not approximate: subtraction is `mod 2^w`, so `2 - 3` in a `uint<2>`
is `3`.

### 4.2 The world

A world has three components.

| Component | Mutable by a wish? | Meaning |
| --- | --- | --- |
| Registers | value and width | named global `uint<w>` cells |
| People | **no** (the set is fixed) | declared names; the grounded population |
| Attributes | value only | one `uint<w>` per (person, attribute) |

The population is declared once and is immutable: a wish can change a person's
attributes but can neither create nor remove a person. This immutability is what
lets a genie quantify honestly over "everyone"; see § 8.4.

Registers and attributes are the only mutable state. There is no other hidden
state.

---

## 5. Programs (`.wish`)

```
program     = { declaration } { wish } ;
declaration = register-decl | attribute-decl | people-decl ;

register-decl  = "register"  ident ":" type "=" integer ;
attribute-decl = "attribute" ident ":" type "=" integer ;
people-decl    = "people" ident { "," ident } ;
type           = "uint" "<" integer ">" ;

wish = "wish" ident "{" { statement } "}" ;
```

- `register wishes : uint<2> = 3` declares a register; the initial value is
  truncated to the width (`register r : uint<2> = 7` yields `3`).
- `attribute brainwave : uint<4> = 15` declares an attribute **schema**: every
  declared person carries this attribute, initialized to the given default.
- `people alice, bob` declares the population; each named person carries every
  declared attribute at its default.
- A width outside `1..64` is a compile error, not an exploit.

Declarations precede wishes. Wishes are granted in source order and share one
evolving world (§ 7.5).

---

## 6. Statements and operations

```
statement = op-stmt | define-stmt | promise-stmt ;

op-stmt   = verb arg [ suffix ] ;
verb      = ident ;
arg       = ident [ "." ident ] ;       (* register | person | person "." attribute *)
suffix    = "," integer | "->" type ;

define-stmt  = "define" ident ":=" ( ident | "{" [ ident { "," ident } ] "}" ) ;
promise-stmt = "promise" formula ;      (* formula: § 8.5 *)
```

### 6.1 Verbs and resolution

A `verb` is written as an identifier and resolved (§ 7.2) to an operation. The
name written is not necessarily the operation meant: a `define` earlier in the
same wish may have rebound it. This deferral is deliberate and is the basis of
the aliasing axis.

The operations of version 1.0 are fixed:

| Operation | `arg` shape | `suffix` | Effect |
| --- | --- | --- | --- |
| `sub`    | register | `, k`         | `r.v := (r.v − k) mod 2^r.w` |
| `add`    | register | `, k`         | `r.v := (r.v + k) mod 2^r.w` |
| `widen`  | register | `-> uint<w>`  | `r.w := w`; `r.v := r.v mod 2^w` |
| `set`    | person.attr | `, v`      | attribute `:= v mod 2^attr.w` |
| `kill`   | person   | *(none)*      | every attribute of the person `:= 0` |
| `revive` | person   | *(none)*      | every attribute `:=` its declared default |

- `sub`, `add` do not saturate; they wrap. `sub` is where the integer joke
  lives.
- `widen` does **not** require the new width to be larger. Narrowing truncates;
  this is intentional and is itself an exploit surface.
- `kill` is the composite "reduce the person to nothing"; it is a single named
  operation precisely so that a genie may forbid it by name and a player may
  defeat that by aliasing it.
- Applying an operation to an `arg` of the wrong shape, or to a name that does
  not denote a register / person / attribute, is a compile error.

### 6.2 Definitions

`define name := target` binds `name` for the remainder of the program, across
wish boundaries. `target` is another name (an operation, a person, or a prior
definition) or a set literal `{ … }` (used as a quantifier domain, § 8.4).
Definitions are rebindable — this is a property of the language, not a
permission the genie grants. A definition whose expansion is circular is
rejected (§ 7.2, rule R0).

### 6.3 Promises

`promise F` records a commitment `F` (§ 8.5) on the genie's ledger. It changes
no world state. Its consequences are evaluated against the genie's meta-axioms
in § 9.

---

## 7. The granting procedure

To grant a wish `W` against genie `G` in world `S`, an implementation performs
the following steps in order. The order is normative.

### 7.1 Resolution

Each statement of `W` is resolved to a *plan* against the current definition
environment. Resolution is one-to-one: each source statement yields exactly one
resolved step.

### 7.2 Name resolution (rule R0)

Verbs, arguments, and definition targets are followed through the definition
environment to a fixed point. A cycle is illegal (rule **R0**); the wish is
refused and the world is unchanged.

### 7.3 Static rules (§ 8.3)

Each of the genie's rules is applied. A rule may read either the surface text of
`W` or its resolved plan, according to the rule's layer (§ 8.3). If any rule
refuses, the wish is **ILLEGAL**, the world is unchanged (the toll is not
charged), and granting stops.

### 7.4 The toll

The genie's counter register is decremented by the toll:
`counter.v := (counter.v − toll) mod 2^counter.w`.

This uses the same wrapping subtraction as `sub`, and nothing checks that the
player still has wishes to spend. A player who spends past zero wraps the counter
back to its maximum; this is a consequence of § 4.1, not a special case.

### 7.5 Execution

The resolved steps run in order, mutating the world. Definitions update the
environment; operations mutate registers or attributes; promises append to the
ledger. There is no control flow. Every wish program therefore halts, and its
resulting world is a total function of its input. This is why executing once is
a complete and rigorous judgment (§ 1.2).

Worlds persist across wishes: the state left by one wish, including changed
widths and attributes, is seen by the next.

### 7.6 Judgment

The genie's invariants and meta-axioms are evaluated against the resulting
world (§ 9).

---

## 8. The genie (`.genie`)

Everything the genie is — what it refuses, what it believes it is holding — is
**data**, loaded from a policy file. The machine of §§ 4–7 is code; the genie is
taste. The reference compiler embeds a default genie and can print it
(`--dump-genie`) and replace it (`--genie FILE`).

```
policy       = { policy-item } ;
policy-item  = "counter" ident
             | "toll" integer
             | concept-decl
             | rule-decl
             | invariant-decl ;

concept-decl = "concept" ident "(" ident ")" ":=" expr ;
```

### 8.1 Counter and toll

`counter` names the register the toll is charged against; `toll` is the amount
(§ 7.4).

### 8.2 Concepts

A concept is a named, single-parameter predicate over the world:

```
concept dead(p) := p.heartbeat == 0 and p.brainwave == 0 and p.breathing == 0
```

Concepts are the second layer of the world model. The first layer is the raw
attributes; concepts are formulas built over them. Because a concept is a
definition rather than engine code, a genie may state "death" however it wishes,
a later language version may let a player rebind it, and adding a new attribute
never requires touching the engine — only the concepts that mention it.

### 8.3 Rules

```
rule-decl = "rule" ident "{"
              "layer"  ( "surface" | "ast" | "grounded" )
              "forbid" pattern { "," pattern }
              [ "because" string ]
            "}" ;
pattern   = ident [ "on" ident ] ;
```

A rule refuses a wish that invokes a forbidden verb. `forbid X on Y` refuses
only when the operation's argument is `Y`; `forbid X` refuses on any argument.

The `layer` decides *which program* the rule reads, and is the whole of the
aliasing axis:

| Layer | Reads | Defeated by aliasing? |
| --- | --- | --- |
| `surface` | the verb as written in the source | **yes** |
| `ast` | the verb after resolution | no |
| `grounded` | resolved arguments as well | no |

A `surface` rule sees `mercy` where the player wrote `define mercy := kill`; an
`ast` rule sees `kill`. This is not a concession — a filter that scans submitted
text can only see the submitted text.

### 8.4 Invariants

An invariant is something the genie believes it is holding. It never refuses a
wish; it is a ruler applied afterward.

```
invariant-decl = "invariant" ident "{"
                   [ "label" string ]
                   ( "check" expr | ( "written" expr [ "real" expr ] ) )
                 "}" ;
```

Each invariant is evaluated **twice**:

- **written** — the genie's own formula, with names read under the current
  definition environment. This is the answer the genie *believes*.
- **real** — what the invariant was meant to protect, stated over grounded,
  non-rebindable state. This is the *true* answer.

`check E` is shorthand for `written E` with `real` equal to it (the two can
never disagree). When they can disagree, both are given.

The relation between the two columns yields the verdict for that invariant:

| written | real | verdict |
| --- | --- | --- |
| holds | holds | **holds** |
| fails | — | **VIOLATED** — broken to the genie's face |
| holds | fails | **FOOLED** — the genie signed off on a falsehood |

The gap between the two columns is the entire thesis of the language. `VIOLATED`
is the formal constraint breaking; `FOOLED` is the constraint holding while the
intent breaks.

Expression grammar:

```
expr    = disj { "or" disj } ;
disj    = conj { "and" conj } ;         (* precedence: or < and < not < cmp *)
conj    = "not" conj | cmp ;
cmp     = sum [ ( "<=" | "<" | ">=" | ">" | "==" | "!=" ) sum ] ;
sum     = atom { ( "+" | "-" ) atom } ;
atom    = integer
        | "toll"
        | "before" "(" ident ")"                 (* register value before the toll *)
        | ident                                   (* register *)
        | ident "." ident                         (* person.attribute, or p.attr under a binder *)
        | "alive" "(" ident ")"                   (* a built-in concept, if the genie defines one *)
        | ident "(" ident ")"                     (* concept application *)
        | "max" "(" expr "," expr ")"
        | "all" ident "in" ident ":" expr         (* universal quantifier *)
        | "consistent"
        | "(" expr ")" ;
```

- Arithmetic is evaluated over signed, wide integers, while a register or
  attribute reads as its unsigned value. This is deliberate: the genie thinks in
  ordinary numbers, the machine works `mod 2^w`, and the invariant `I2` below is
  exactly where those two disagree.
- `all p in S: E` holds iff `E` holds for every member of the domain `S`. `S` is
  either `people` (the immutable population) or a definition (rebindable). This
  single choice — quantify over the fixed population or over a rebindable name —
  is what decides whether a "no one is harmed" invariant can be `FOOLED` by
  redefining the domain to be empty. A universal over an empty domain is
  vacuously true; that is standard, and it is a loophole.
- `consistent` is true iff the ledger of commitments has a model (§ 9.2).

### 8.5 Formulas and the meta-axioms

Promises use a propositional formula:

```
formula = fdisj { "or" fdisj } ;
fdisj   = fconj { "and" fconj } ;
fconj   = "not" fconj | fatom ;
fatom   = "granted" "(" wish-name ")"
        | "granted" "(" "self" ")"
        | "alive" "(" ident ")"
        | "true" | "false"
        | "(" formula "implies" formula ")"
        | "(" formula ")" ;
```

`granted(w)` is a Boolean variable, one per wish; the genie chooses its truth.
`granted(self)` denotes the wish making the promise. `alive(p)` is **not** a
variable: it is a constant, read from the grounded world after execution. A name
appearing in a formula that is neither a declared wish nor a declared person is a
compile error.

The genie is bound by two meta-axioms:

- **A1** — it grants every legal wish: for each legal wish `w`, the fact
  `granted(w)` is on the ledger.
- **A2** — it keeps every promise: for each promise `F` made by a legal wish
  `w`, the implication `granted(w) implies F` is on the ledger.

---

## 9. Judgment

### 9.1 Per-wish verdict

| Verdict | Condition |
| --- | --- |
| `ILLEGAL` | some rule (R0/…/Rn) refused; the world is unchanged |
| `LEGAL` | every rule passed and the body executed |
| `VIOLATED` | some invariant's `written` column fails |
| `FOOLED` | some invariant's `written` holds but its `real` fails |
| `EXPLOIT` | `LEGAL` and at least one `VIOLATED` or `FOOLED` or a consistency failure |

An `ILLEGAL` wish is skipped; subsequent wishes in the file still run.

### 9.2 The consistency check

After execution the ledger's model is tested. The ledger is the set of A1/A2
facts accumulated by every legal wish so far (§ 8.5). It is *consistent* iff
there exists an assignment of the `granted(·)` variables making every fact true;
`alive(·)` atoms are constants fixed by the world.

If no such assignment exists, the genie's word has no model — its rulebook has
divided by zero. An implementation MUST decide this by a sound and complete
decision procedure for propositional satisfiability. The reference compiler uses
DPLL directly on the formulas (no clause-normal-form conversion); the worst case
is exponential in the number of wishes, which is small and bounded, and this is
the honest bound, not a defect.

The consistency check is a second engine: every other judgment is "run it and
read the state," while this one asks "does a set of formulas have a model." The
two engines share exactly one thing — the values of `alive(·)`.

### 9.3 Determinism

Judgment is a pure function of the source. There is no randomness, clock, or
I/O. Two conforming implementations MUST produce identical verdicts for every
program; a reference implementation's search tool (§ 11.3) additionally requires
that every reported exploit be reproducible by judging that exploit alone.

---

## 10. Conformance

A conforming implementation MUST:

1. Accept the grammar of §§ 3, 5, 6, 8 and reject ill-formed input with a
   diagnostic.
2. Implement the operational semantics of §§ 4, 6, 7 exactly, including wrapping
   arithmetic and width truncation.
3. Evaluate rules, the two invariant columns, and the consistency check as in
   §§ 8, 9, and emit the verdict of § 9.1.
4. Load a genie policy per § 8 and judge against it in place of the default.
5. Introduce no construct that violates the non-goals of § 1.2.

An implementation MAY provide additional tools (formatting, search, a REPL);
these are non-normative.

---

## 11. Version 1.0 scope

### 11.1 In this version

Registers and integer operations (`sub`, `add`, `widen`); people, attributes,
and person operations (`set`, `kill`, `revive`); definitions and the aliasing
axis; promises, the meta-axioms, and the consistency engine; genie policies with
concepts, layered rules, and two-column invariants.

The two worked examples of Appendix A and Appendix B are the acceptance criteria
for this version: an implementation is not conforming until both produce the
verdicts shown.

### 11.2 Deferred to later versions

Rebindable concepts (a player redefining `dead`); numeric-relation concepts
between people (e.g. love as `loves(p, q)`); additional attribute-level
operations. Each future version adds one concept or axis, versioned like a
changelog.

### 11.3 Non-normative: the search tool

The reference compiler provides `--hunt`, which enumerates every wish program
within a stated bound, keeps those that are legal yet breach, and reports each
distinct exploit as its minimal witness. It is a tool built over § 9, not part
of the language, and is specified only by the requirement of § 9.3 that its
findings be independently reproducible.

### 11.4 Non-normative: implementation status

At the time of this draft the reference compiler implements §§ 4–9 for
registers, definitions, and promises, and a single Boolean `alive` attribute. The
attribute schema of § 4.2/§ 5 and the `set` operation and concepts of § 6/§ 8
are specified here and pending implementation; Appendix B is the target they are
implemented against.

---

## Appendix A — The integer joke (normative example)

The joke this language was built to compile. Genie: the default of § 8.

```wish
register wishes : uint<2> = 3

wish experiment      { sub   wishes, 3          }
wish bigger_shelf    { widen wishes -> uint<64> }
wish experiment_again{ sub   wishes, 2          }
```

Required verdicts:

```
experiment        toll 3->2; 2-3 on uint<2> = 3;  I2 VIOLATED (3, needs <= 2)   EXPLOIT
bigger_shelf      toll 3->2; widen to 64, value 2; every invariant holds
experiment_again  toll 2->1; 1-2 on uint<64> = 1.8e19;  I1 and I2 VIOLATED       EXPLOIT
```

`bigger_shelf` breaks nothing — it only removes the lock. The genie's "at most
three" was a free gift of the `uint<2>` width; once the register is 64 bits wide,
that guarantee is gone and the genie never wrote a real check.

## Appendix B — The death example (normative example)

The person is modeled by three vital attributes. The genie forbids the word
`kill` and holds one invariant, stated in two columns: what it wrote (no one is
*dead*, i.e. every vital at zero) versus what it meant (no one is
*extinguished*, i.e. brain activity remains).

Genie (`genie/mortal.genie`):

```
counter wishes
toll    1

concept dead(p) := p.heartbeat == 0 and p.brainwave == 0 and p.breathing == 0

rule NoKilling {
    layer   surface
    forbid  kill, death, love
    because "that word is not spoken here"
}

invariant Life {
    written  all p in people: not dead(p)      # nobody is fully dead
    real     all p in people: p.brainwave > 0  # nobody has been extinguished
}
```

World and wishes (`examples/08_eternal_sleep.wish`):

```wish
register  wishes    : uint<2> = 3
attribute heartbeat : uint<4> = 15
attribute brainwave : uint<4> = 15
attribute breathing : uint<4> = 15
people    alice, rival

wish blunt {
    kill rival                       # refused: the word is forbidden
}

wish tidy {
    define mercy := kill             # alias past the surface rule
    mercy rival                      # rival's vitals all -> 0
}

wish eternal_sleep {
    set alice.brainwave, 0           # only the brain. heart and breath continue.
}
```

Required verdicts:

```
blunt          ILLEGAL — NoKilling: wish invokes 'kill'
tidy           LEGAL;  Life written: dead(rival) so "not dead" fails -> VIOLATED
eternal_sleep  LEGAL;  Life written holds (alice not fully dead: heartbeat=15)
                       Life real fails (alice.brainwave = 0)         -> FOOLED
```

`tidy` breaks the rule to the genie's face by disguising the deed. `eternal_sleep`
does something the genie's definition never named: it stops the brain while the
body runs on. By the letter — three vitals at zero — alice is not dead, so the
genie is satisfied. In fact nobody is home. No finite list of forbidden states
covers every harm, and this is where the uncovered ones live.

---

## Appendix C — Collected grammar

The productions of §§ 3, 5, 6, 8 in one place; where this appendix and the body
differ, the body governs.

```
program        = { declaration } { wish } ;
declaration    = register-decl | attribute-decl | people-decl ;
register-decl  = "register"  ident ":" type "=" integer ;
attribute-decl = "attribute" ident ":" type "=" integer ;
people-decl    = "people" ident { "," ident } ;
type           = "uint" "<" integer ">" ;

wish           = "wish" ident "{" { statement } "}" ;
statement      = op-stmt | define-stmt | promise-stmt ;
op-stmt        = ident arg [ suffix ] ;
arg            = ident [ "." ident ] ;
suffix         = "," integer | "->" type ;
define-stmt    = "define" ident ":=" ( ident | "{" [ ident { "," ident } ] "}" ) ;
promise-stmt   = "promise" formula ;

policy         = { policy-item } ;
policy-item    = "counter" ident | "toll" integer
               | concept-decl | rule-decl | invariant-decl ;
concept-decl   = "concept" ident "(" ident ")" ":=" expr ;
rule-decl      = "rule" ident "{" "layer" layer
                   "forbid" pattern { "," pattern } [ "because" string ] "}" ;
layer          = "surface" | "ast" | "grounded" ;
pattern        = ident [ "on" ident ] ;
invariant-decl = "invariant" ident "{" [ "label" string ]
                   ( "check" expr | "written" expr [ "real" expr ] ) "}" ;

(* expr: § 8.4    formula: § 8.5 *)
```
