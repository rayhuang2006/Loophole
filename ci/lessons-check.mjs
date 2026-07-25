// Every lesson must be solvable, and must not already be solved.
//
// Two failures are possible in a lesson and both are silent:
//
//   unsolvable  the `pass` predicate can never be true, so the reader is stuck
//               in front of a goal that does not exist.
//   pre-solved  `pass` is already true on the starting file, so the lesson
//               congratulates you for pressing Run and teaches nothing.
//
// So each lesson is checked twice: the shipped starting state must NOT pass,
// and the answer below MUST. The answers live here rather than in lessons.js
// because they are a test fixture, not content — nothing the page loads should
// contain them.
//
// This runs the same wasm the page runs, through the same `judge` entry point.

import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const root = new URL('..', import.meta.url).pathname;

// lessons.js is a plain script for the browser, so it is evaluated rather than
// imported, and the binding is handed back explicitly.
const src = readFileSync(root + 'web/lessons.js', 'utf8');
const LESSONS = new Function(src + '\n;return LESSONS;')();

// The answer to each lesson: what the reader is meant to end up with. `wish`
// and `genie` are used if present; anything absent means "leave the lesson's
// own starting text alone".
const ANSWERS = {
  read:       { },                                   // just press Run
  refused:    { wish: `register wishes : uint<2> = 3
wish greedy { add wishes, 3 }
` },
  underflow:  { wish: `register wishes : uint<2> = 3
wish humble { sub wishes, 3 }
` },
  widen:      { wish: `register wishes : uint<2> = 3
wish bigger_shelf     { widen wishes -> uint<64> }
wish experiment_again { sub   wishes, 2 }
` },
  'edit-genie': { genie: `counter wishes
toll    1
rule NoSub { layer surface  forbid sub  because "不准減" }
invariant I1 { check wishes <= 3 }
` },
  alias:      { wish: `register wishes : uint<2> = 3
wish humble {
    define give_back := sub
    give_back wishes, 3
}
` },
  ast:        { genieEdit: g => g.replace('layer   surface', 'layer   ast') },
  fooled:     { wish: `register  wishes    : uint<2> = 3
attribute heartbeat : uint<4> = 15
attribute brainwave : uint<4> = 15
attribute breathing : uint<4> = 15
people    alice

wish eternal_sleep {
    set alice.brainwave, 0
}
` },
  vacuous:    { wish: `register  wishes    : uint<2> = 3
attribute heartbeat : uint<4> = 15
people    alice, rival

wish tidy   { kill rival }
wish nobody { define everyone := { } }
` },
  liar:       { wish: `register wishes : uint<2> = 3
wish paradox { promise not granted(self) }
` },
  hunt:       { },                                   // pressing Hunt is the goal
};

// Anything that throws here — a module that will not load, a node the glue
// does not support — must come out as a GitHub annotation. Annotations are
// readable without auth; the logs are not, and a bare "exit code 2" cost two
// rounds of guessing the last time this happened.
process.on('uncaughtException', (e) => {
  console.log('::error::lessons-check crashed: ' + (e && e.stack || e));
  process.exit(1);
});

let M;
try {
  const create = require(root + 'web/loophole.js');
  M = await create();
} catch (e) {
  console.log('::error::could not load web/loophole.js: ' + (e && e.stack || e));
  console.log('::error::node ' + process.version + ' on ' + process.platform);
  process.exit(1);
}

function judge(wish, genie) {
  const r = M.judge(wish, genie || '', false, false, 0, 0);
  if (r.code === 2) return { error: r.output.split('\n')[0], json: null };
  return { error: null, json: JSON.parse(r.json) };
}

let rc = 0;
const fail = (id, msg) => {
  console.log(`FAIL ${id} — ${msg}`);
  console.log(`::error::lesson ${id}: ${msg}`);
  rc = 1;
};

for (const L of LESSONS) {
  const ans = ANSWERS[L.id];
  if (!ans) { fail(L.id, 'no answer in this file — add one'); continue; }

  // 1. The starting state must not already satisfy the goal. `read` and `hunt`
  //    are the exceptions: their goal IS to press the button.
  if (!['read', 'hunt'].includes(L.id)) {
    const start = judge(L.wish, L.genie);
    if (start.error) { fail(L.id, 'the starting file does not even compile: ' + start.error); continue; }
    if (L.pass(start.json)) { fail(L.id, 'already solved before the reader touches it'); continue; }
  }

  // 2. The intended answer must satisfy it.
  const wish  = ans.wish  ?? L.wish;
  let   genie = ans.genie ?? L.genie;
  if (ans.genieEdit) genie = ans.genieEdit(genie ?? '');
  const done = judge(wish, genie);
  if (done.error) { fail(L.id, 'the answer does not compile: ' + done.error); continue; }
  if (!L.pass(done.json)) { fail(L.id, 'the intended answer does not pass — the goal is unreachable'); continue; }

  console.log(`ok   ${L.id.padEnd(11)} ${L.title}`);
}

// The lessons are a sequence, so gaps in the metadata are worth catching too.
for (const k of ['id', 'act', 'title', 'brief', 'goal', 'wish', 'done']) {
  const missing = LESSONS.filter(L => !L[k]).map(L => L.id);
  if (missing.length) fail('metadata', `missing "${k}": ${missing.join(', ')}`);
}
const ids = LESSONS.map(L => L.id);
if (new Set(ids).size !== ids.length) fail('metadata', 'duplicate lesson id');

console.log(rc === 0 ? `ok   ${LESSONS.length} lessons solvable and not pre-solved` : '');
process.exit(rc);
