# Changelog

The compiler and the two languages it reads are versioned separately. A bug fix
moves the compiler; a new concept moves whichever language grew it.
`loophole --version` prints all three.

Releases before 1.1.0 were published under the compiler's old name, `wishc`.

## loophole 1.14.0 — wish 1.0, genie 1.0

**Breaking, and it fixes a wrong answer.** `--json` reported each register as a
bare number:

```json
"registers": { "wishes": 18446744073709551615 }
```

JSON numbers are read as IEEE-754 doubles by every JavaScript consumer there is,
and doubles run out of integers at 2^53. So that value came back as
`18446744073709552000` — the contract was quietly handing out a wrong number in
*precisely* the case this language exists to demonstrate. Nothing failed, because
the only consumers so far compared small values.

Registers now report an object, and the value is a string:

```json
"registers": { "wishes": { "value": "18446744073709551615", "width": 64 } }
```

`width` is in there for a second reason: it is not a constant. `widen` changes
it, so a consumer that read the declared width out of the source would print
`uint<2>` for a register that has been 64 bits wide for two wishes — which is the
same class of confidently-stated falsehood.

One break rather than two, since both defects live in the same field. Downstream:
`web/lessons.js` compares `.value` as a string, and `ci/check.sh` asserts the
exact digits **through `node`** — python3 would have passed on arbitrary-precision
ints while every real consumer failed.

## loophole 1.13.0 — wish 1.0, genie 1.0

**`--keywords` now says what each word means.** It listed the 42 reserved words
and nothing else, so an editor wanting to show `sub`'s meaning on hover had to
carry its own prose — a second account of the semantics, free to drift from this
file with nothing to notice. `docs` maps every word to a `syntax` line and a
sentence, taken from the same table rows the lexer and parser read.

An operation's `syntax` is **derived** from the `OperandKind` it declares, not
typed out beside it. `sub` cannot end up documented as taking a width while the
parser demands an immediate, because both answers come from one field.

Additive on purpose: the five word arrays are byte-identical to what they were,
so `tools/gen-grammars.mjs` downstream did not have to change. `ci/keywords-check.sh`
now also asserts that every reserved word has an entry, that no entry describes a
word that is not reserved, and that an operation's syntax actually names its
operand — checked by deleting a group and watching it go red.

## loophole 1.12.0 — wish 1.0, genie 1.0

**A genie can be checked on its own.** `loophole --check-genie mine.genie` reads
a genie and says only whether it parses — nothing about wishes, because there are
none. Someone edits a genie for a while before any wish exists to run it against,
and until now a syntax error in that genie was invisible until a wish happened to
reference it. An editor showing a lone `.genie` had nothing to underline.

It is a syntax check and only that, on purpose. An invariant may name a register
that lives in the wish's world, and whether that register exists cannot be known
from the genie alone (§6) — so the check parses the file and stops. A well-formed
genie exits 0 (`genie ok (N rules, M invariants)`, or an `ok` object under
`--json`); a malformed one exits 2 with the same structured error a malformed
wish gets. The embedded build gains `checkGenie(text)` for the same purpose.

## loophole 1.11.0 — wish 1.0, genie 1.0

**Every wish reports its line, and every refusal reports which rule and where.**
`--json` gave a wish's statements a line each but never the wish itself, so a
wish with an empty body had no position anywhere — and an empty wish is one of
the sharper exploits here: four of them underflow the toll without asking for
anything. A refusal was worse; it was a sentence with `(line 9)` inside it, so
the only way to get the position was to parse prose that §10.1 permits to be
reworded. Wishes now carry `line`, and a refused one carries
`refused_by: { rule, line }` beside the sentence. Additive — the sentence stays.

### Two bugs only the embedded build could have

Both were found while wiring an editor up to `judge()`, and neither is reachable
from a terminal.

`cliMain` never reset `g_wants_json`. The command line calls it once per process,
so the flag being sticky costs nothing there — but the embedded build calls it
twice per judgment, the second time with `--json`, so from the second judgment
onward a *prose* run that hit an error answered in JSON. The build with no
terminal to notice it in was the only build that had it.

And `judgeSource` dropped the JSON whenever the run failed (`if (r.code != 2)`),
which was right when a failed run's JSON was empty and is exactly backwards now
that it carries the diagnostic's position.

`ci/embed-check.mjs` now holds both, plus the property that a judgment made
through `judge()` matches the one the command line makes. `ci/wasm-check.sh`
could not have: it drives `loophole.node.js`, which is a command line — one
process, one call, exit — and every failure above needs a second call to appear.

Shipped separately from 1.10.0 because 1.10.0's released WebAssembly build has
those two bugs in it, and a fix that reuses a version number never reaches
anybody: the release only fires when `COMPILER_VERSION` changes.

## loophole 1.10.0 — wish 1.0, genie 1.0

Two things a downstream tool needs and could not get.

**`--json` reports the error too.** Until now a failed run produced only prose:
an editor wanting to draw a squiggle had a line and a column available nowhere
except a message §10.1 says may be reworded at will. So the only way to get them
was to scrape the report — the exact mistake that has broken CI once and the
course's marking once in this project's history. The JSON now carries
`error.{message,line,column,help,note,source}` on stdout, so a caller has one
thing to parse whether the run succeeded or not.

**Releases carry the WebAssembly build.** `loophole.js` and `loophole.wasm`, for
anything that has to judge without a shell — an editor extension, a page. It is
published rather than left to each consumer because building it needs emsdk, and
because `make wasm-check` has already proved that this exact artifact judges
identically to the native compiler.

Neither language changed, and the prose report is byte-identical.

### Walked into the same trap a third time

The new contract check piped `loophole --json` into a validator. `loophole`
exits 2 there, correctly, and under `set -o pipefail` that becomes the
pipeline's status — so the check reported failure on perfect JSON. There is a
comment six lines above the new code warning about exactly this, written the
first time it happened. Capturing before parsing is the fix, again.

## loophole 1.9.0 — wish 1.0, genie 1.0

**Releasing is automatic now, because I kept forgetting to do it by hand.** Six
versions in a row were bumped in the source and never tagged: the compiler said
1.9.0 while `releases/latest` still pointed at 1.3.1, so anything downstream
asking for `--keywords` would have got a binary that did not have it.

Bumping `COMPILER_VERSION` is the act of deciding to release. Everything after
it is mechanical now: CI passes, a job notices the version has no tag, tags it,
and asks the release workflow to run. An ordinary commit publishes nothing,
because an ordinary commit does not change the version.

One wrinkle worth recording: **a tag pushed with `GITHUB_TOKEN` does not start
another workflow.** GitHub blocks that so a run cannot trigger itself forever.
So the release is requested explicitly by `workflow_dispatch`, which is one of
the few events that token may raise — and which is why that trigger and its
`tag` input existed already.

**Releases now carry `keywords.json`.** A downstream repo colouring the language
needs one `curl`: no binary to download, nothing to execute, any runner. It is
the DATA and not a grammar on purpose — an editor's format is that editor's
business, and putting TextMate's scope names in this repo would make every new
editor a change here.


**`--keywords`**, so that nothing has to keep its own copy of what the language
reserves. The list existed in three places — the lexer, §3 of the specification,
and the web page's highlighter — and an editor plugin would have made four. That
kind of copy does not fail loudly when it falls behind: the language grows, and
the only symptom is a word that quietly stops changing colour.

The output is generated from the tables the lexer and the parser read, so it
cannot disagree with the compiler. Adding an operation to `OPS` makes it appear
with nobody editing anything.

- **The wish keywords are a table now**, not an if-else chain, because the lexer
  and `--keywords` have to read the same thing for that guarantee to hold.
- **The genie's keywords get a weaker guarantee, and it is worth saying so.**
  They are contextual — the parser recognises them by name where it expects
  them — so the table beside them is a declaration, not the thing they read.
  `ci/keywords-check.sh` compares it against the parser's own call sites; a
  `word("...")` that is not declared turns CI red. Making that structural would
  mean an enum and twenty-two rewritten call sites, which is not worth it.
- **The page no longer has a keyword list at all.** It asks the compiler at
  load, through the worker. CI checks it has not gone back to a literal one.
- The check also holds §3 to the compiler: a reserved word the document does not
  list is a defect in the document.

### A caching bug this uncovered

The worker is never cached and the module is, so for a few minutes after a
deploy a browser can pair a **new worker with the previous module**. The new
worker called `M.keywords()`, which that build did not have, and threw before
posting `ready` — the page sat on "loading" forever with nothing in the console.
This is the mirror of the bug fixed in 1.6.0, where a cached worker was paired
with a newer page. The worker now calls the optional function defensively:
colour is worth degrading, a dead page is not.

## loophole 1.8.3 — wish 1.0, genie 1.0

Three problems a first-time reader found, none of which the author could see.

- **The text named positions the layout did not have.** Seventeen places said
  「左邊」and「右邊」, but the two editors only sit side by side above 1560px;
  on an ordinary laptop they stack, and every one of those sentences was wrong.
  They name the panes now — `wish` and `genie` — which is true at any width.
- **The verdict was below the fold.** The editors had a fixed 244px floor, so a
  six-line chapter sat in a mostly empty box, and the built-in genie is fifty
  lines. A reader had to scroll from the top of the page to the bottom to find
  out what had happened. Editors now size to their text, and while the panes are
  stacked the report comes before the genie: after pressing Run you are looking
  for the verdict, and the genie is reference material the report already
  summarises. Side by side, source order returns.
- **The course ended without a door.** The last chapter said what it all meant
  and stopped. It now offers four ways on — stay and rewrite the genie, install
  the binary, read the specification, read the source — and states plainly that
  nothing the reader typed ever left their browser.

## loophole 1.8.2 — wish 1.0, genie 1.0

- **A reading chapter showed two buttons that said the same thing.** Once read,
  the "next chapter" at the foot of the text and the one in the seal below it
  were both on screen, a few centimetres apart. The first is only shown before
  the chapter has been read.
- **The table of contents no longer opens at all.** Dropping the full table down
  on hover put the same two rows back that collapsing it was meant to remove.
  The readout on the left now names whichever tick the pointer is over —
  `08 · 一、數字 · 減過頭` — so the contents are reachable one chapter at a time
  and never occupy the page. The rail is a 35px hairline in every state.

## loophole 1.8.1 — wish 1.0, genie 1.0

- **Passing a chapter offered no way onward.** The only route forward was a
  small arrow at the top of the panel, which is nowhere near where a reader is
  looking at the moment they have just succeeded. The seal at the foot of the
  chapter now names the next one and goes there.
- **The table of contents was two full rows** of numbered chapters, and it
  shouted louder than the chapter being read. It is a single hairline of
  twenty-eight ticks now — solved in green, current taller and darker — and the
  full table drops down on hover, or on focus, so a keyboard can still reach it.

## loophole 1.8.0 — wish 1.0, genie 1.0

**A course, rather than a worksheet.** The previous set was twenty-one
exercises: every chapter demanded an action, and the first one opened with
`register wishes : uint<2> = 3` without having said what this language is for or
what a bit is. That is written for somebody who already knows.

Twenty-eight chapters now, and **nine of them ask for nothing at all**. They
exist so the next chapter can assume something:

- The joke itself, told properly, before any syntax.
- What a register is — as a two-digit odometer that rolls over at 99, before
  `uint<2>` is ever mentioned.
- That it rolls over *backwards* too, worked through by hand, one chapter before
  the reader is asked to underflow anything.
- Rules versus invariants — a gate versus a ruler — before either is used.
- Aliasing, as a doorman who was told to keep out a name rather than a person.
- The letter and the intent, as a school rule about phones and a student with a
  tablet, before `written`/`real` appears.
- What the whole thing meant, at the end.

Every doing-chapter still teaches its syntax first, annotated piece by piece.
The arc where the reader walls off their own exploit, aliases past their own
wall, and then repairs it is unchanged; it just has a chapter of explanation in
front of it now.

**The page was redesigned around what this project actually is**: a joke taken
with the seriousness of a standards document. Serif for the prose and monospace
for the machinery, and the contrast between the two is the thesis. Hairline
rules instead of cards and shadows, the way a book divides a page. One red,
used for exactly one thing — where the genie was beaten — after the way a scribe
marked what mattered in vermilion and everything else in iron gall.

`ci/lessons-check.mjs` covers the reading chapters too: they are skipped for
solvability but still have to carry their text.

**Progress is stored under a new key.** Chapter ids survive a rewrite, and
reusing the old one marked six chapters done that the reader had never seen.

## loophole 1.7.0 — wish 1.0, genie 1.0

**The lessons were rewritten, because the first set taught nothing.** They were
a list of objectives — "write a wish that gets refused" on lesson two, before
anything had said what a statement looks like. That is a worksheet for somebody
who can already write the language, and on lesson two nobody can.

Twenty-one lessons now, in six acts, and **the first five ask no questions at
all**: here is a line, here is what each part of it is called, type it and look
at what changed. Thirteen of the twenty-one are that shape. The arc that used to
be three lessons is now four, and the last act builds `promise` up before asking
for the paradox.

**`--json` gained two fields**, because the marking needed them and there was no
honest way to get them otherwise:

- `wrote` — the statements as typed, with `kind` and the surface `verb`. The
  written form is the useful one: an alias reports the alias, which is the
  distinction the whole language is about.
- `registers` — the values after each wish. A judgment without the numbers it
  was made from cannot be checked by anything downstream, and a lesson about what
  `sub` does needs to see what `sub` did. The alternative was scraping it back
  out of prose that §10.1 says may be reworded at will.

**The page was redesigned.** Syntax highlighting is a transparent textarea over
a highlighted `<pre>` — native editing, selection and IME intact, no editor
dependency, and the page stays three files anybody can read straight through.
The progress rail groups the lessons by act, because twenty-one numbered circles
read as a wall and six named groups read as a shape. Warm paper rather than IDE
grey: this is a fairy tale that happens to compile.

The keyword list for highlighting is duplicated from §3 of the specification and
will go stale silently if the language grows. The fix is a `--keywords` flag on
the compiler; until that exists the duplication is marked in both places.

## loophole 1.6.0 — wish 1.0, genie 1.0

**Eleven lessons.** The playground had nothing to do in it: a language nobody
knows, an empty editor, and a Run button. The guide in `docs/guide/` teaches the
same material, but reading is not how anyone learned this — the author included,
who got fluent by editing one line of a file and running it.

The order is the one concepts depend on, not the one the guide is written in.
Lessons 5–7 are a single arc: you write a rule that blocks your own exploit, you
alias your way past it, and then you fix your own genie by moving one word.

**The compiler is the marker.** No lesson checks what you typed; each one asks
the genie whether you got away with it. Grading by matching an expected answer
would quietly deny the thing the project exists to claim — that the exploits are
consequences of the semantics rather than things the author planted.

- Marking reads `--json`, never the report. §10.1 says the prose is not the
  contract, and CI has already been broken once by a check that read it; a
  lesson doing the same would start failing the day someone improved a sentence.
- The browser entry now returns the JSON verdict alongside the prose. It judges
  twice to do it, which is a millisecond and cannot disagree with itself: §9.3
  makes the verdict a pure function of the source.
- **`ci/lessons-check.mjs` runs every lesson twice**: the shipped starting file
  must NOT satisfy the goal, and the intended answer MUST. Both failures are
  silent otherwise — a goal nobody can reach, or a lesson that congratulates you
  for pressing Run.
- The worker is never cached. A stale one paired with a newer page reads as a
  logic error rather than a caching one: the page asks for a field the old
  worker does not send, so every lesson silently reports "not solved". That
  happened during development, and it cost twenty minutes.

Progress is kept per lesson in `localStorage`; the lesson text never is, so
rewriting a lesson reaches people who already passed it.

## loophole 1.5.0 — wish 1.0, genie 1.0

**There is a playground.** A static page, no server: the compiler is the same
one, built to WebAssembly, running in the reader's own browser. Write a wish on
the left, edit the genie on the right, press Run. `make wasm-check` is a
precondition for publishing it, so the page cannot ship a compiler that judges
differently from the one the specification describes.

Three things had to be measured rather than guessed:

- **`--hunt` froze the tab.** It is one long call into wasm with no yield point,
  and the default bounds take 19 seconds natively. It now runs in a Web Worker,
  with an elapsed-time readout and a cancel button — cancelling means killing
  the worker, because a search cannot be asked to stop politely.
- **The page uses smaller search bounds than the command line.** On the standard
  world, `--max-stmts 2 --max-wishes 4` finds the same seven shapes as the
  default in **0.4s instead of 19** — including the empty one. Fifty times
  faster for nothing given up.
- **`-fexceptions` cost a 2x slowdown.** Emscripten emulates exceptions in
  JavaScript by instrumenting every call, to support one path that is taken only
  when a program fails to parse. `-fwasm-exceptions` brings the searcher back to
  within 15% of the native binary and drops 85 KB off the module.

Diagnostics keep their colour: the page asks the compiler for it and renders the
escape sequences as spans. Whether a terminal exists is now something the host
states rather than something `isatty` guesses at, since under wasm its answer is
an implementation detail of the runtime.

## loophole 1.4.0 — wish 1.0, genie 1.0

**The compiler builds for the browser**, as a step toward a playground: nobody
learns a language by reading about it, and running one currently costs an
install, an editor and a terminal. Neither language changed, and the native
build is byte-identical to 1.3.1 on every example.

- `make wasm` produces a WebAssembly build (447 KB) via Emscripten.
- **`make wasm-check` is the claim it has to earn**: both builds run the same
  arguments and their output is diffed against *each other*, never against a
  golden. A playground that disagreed with the compiler on one verdict, one exit
  code or one line of a diagnostic would be a second implementation of the
  language — and §9.3 requires two conforming implementations to agree verbatim.
  All eight examples, five hunt fingerprints and four diagnostics match. CI runs
  it.
- **A diagnostic throws instead of calling `exit()`.** This is what made the
  browser build possible: `exit()` under wasm tears down the runtime, so a page
  would have judged exactly one program and then been dead. It is the better
  shape natively too — the stack now unwinds instead of being abandoned.
- `main` is a four-line wrapper around `cliMain`, so the exception has one place
  to be caught and a host that is not a command line has a function to call
  rather than a process to start.

## loophole 1.3.1 — wish 1.0, genie 1.0

**A verb that denotes no operation was reported as the genie refusing, and the
run exited 0.** `sube candy, 2` produced `REFUSED. unknown operation 'sube'` and
then `1 refused, 0 granted, 0 exploits` — a clean judgment, exit 0, from a file
that never executed. This is the same defect as the exit-1 one fixed in 1.3.0,
in a worse place: 0 is the code that means "judged, and the genie held".

It also put words in the genie's mouth. A genie's rules are about operations;
it has no opinion whatever about a name that denotes none. §6.1 calls this a
compile error, and it now is one, with a caret and a suggestion.

The distinction had to be made inside resolution without making it fatal there,
because the hunter depends on both failures being recoverable: its alphabet
writes an alias as a verb independently of the `define` that binds it, so it
generates use-before-define candidates deliberately and must skip them. The
resolver now reports *which* kind of failure occurred and the caller decides.

- **An R0 cycle is still a refusal.** §7.2 names R0 as a rule, so a circular
  definition leaves the wish `ILLEGAL`, the world unchanged, and the run
  continuing. Only the malformed cases became errors. CI asserts both.
- **The closing note tells the truth about what ran.** Resolution cannot be done
  up front — a verb may be bound by a `define` in an earlier wish — so the error
  can surface mid-run, and earlier wishes really were judged. The note says
  which case it is instead of always claiming nothing was judged.
- Nothing is printed for a wish until it is known to be readable, so a malformed
  one no longer leaves a dangling header above its own error.
- §6.1 of the specification now states this explicitly, including that the error
  is not necessarily detectable before execution begins.

## loophole 1.3.0 — wish 1.0, genie 1.0

**A syntax error exited 1, which is the code for "an exploit was found."**
Anything that could not be parsed reported itself as a successful hunt: the
README's own `loophole --hunt w.wish && echo "airtight"` would read a typo in
the genie file as a hole in the genie. All three parsers now exit 2. The
regression suite only ever checked the missing-file path, so nothing caught it;
it now covers a lex error, a bad width, a truncated parse and a bad genie —
one case per way of failing, rather than one per exit code.

**Diagnostics were rewritten.** Nobody knows this language, so every error is
somebody's first encounter with a rule they did not know existed — and
`lex error (line 12): unexpected character ';'` spends that moment saying
nothing. Errors now quote the line, point at the column, and state the rule:

```
error: unexpected character ';'
 --> playground.wish:4:17
  |
4 |     sub candy, 2;
  |                 ^
  |
help: statements are not terminated in Loophole -- one ends where its line
      does. Delete the ';'.
```

- Tokens carry a column, so the caret lands on the offending token rather than
  on the line. Diagnostics raised after a value has been validated point back at
  the token they are about, not at whatever the parser has since moved to.
- Specific help for the mistakes the design makes likely: `;` (no statement
  terminators), a non-ASCII byte (§3 is ASCII-only, and why), `/` or `*`
  (comments are `#`), `[` (sets use braces), a width outside 1..64, and an
  unknown `layer` (there are two, because there are two programs).
- **"did you mean" on an unknown operation.** Candidates are derived from the
  operations table and the names in scope, so adding an operation improves the
  suggestion with no second place to update. A prefix match outranks edit
  distance, since the operations have short names and writing the whole word —
  `subtract` for `sub` — is five edits away and would never clear a threshold.
- **Colour, only when a human is looking.** Disabled when stderr is not a
  terminal and when `NO_COLOR` is set, so nothing leaks into a pipe, a golden,
  or a `grep`. CI asserts this.

The report itself is unchanged, and so is `--json`.

## loophole 1.2.1 — wish 1.0, genie 1.0

- **The summary line said the opposite of what it meant.** `0 of 1 wishes got
  past the genie` was printed for a wish that had just been granted and had
  broken nothing — and "got past the genie" is the natural way to say "was
  granted", so the count read as a contradiction. Being granted and being an
  exploit are two different things, and every exploit is also granted, so one
  ratio cannot carry both. The line now reports all three counts:
  `1 wish: 0 refused, 1 granted, 0 exploits.`

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
- **CI checks conformance against `--json`, not against the prose.** The
  Appendix A/B step used to grep the report for `breached I1+I2`, so rewording
  the report for readability turned a conformance check into a false regression.
  §10.1 is explicit that the prose is not the contract; the check now asserts on
  the machine-readable verdict, which is.

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
