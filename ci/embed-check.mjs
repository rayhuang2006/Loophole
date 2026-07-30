// The embedded API's contract, checked against the real WebAssembly build.
//
// `ci/wasm-check.sh` cannot cover this. It drives `loophole.node.js`, which is a
// command line: one process, one `cliMain`, then exit. `judge()` is the other
// shape entirely -- one long-lived module whose `cliMain` runs twice per
// judgment and many times per session -- and everything below is a way that
// shape can break while every command-line test stays green.
//
// Both properties here are ones a downstream editor depends on, and both were
// broken when this file was written.

import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const createLoophole = require('../web/loophole.js');

const M = await createLoophole();
let failed = 0;
const check = (name, ok, saw) => {
    if (ok) return;
    console.log(`FAIL ${name}` + (saw === undefined ? '' : `\n     saw: ${saw}`));
    failed = 1;
};

const BROKEN = 'register wishes : uint<2> = 3\nwish x { sub wishes, 1; }\n';
const GOOD   = 'register wishes : uint<2> = 3\nwish humble { sub wishes, 3 }\n';
const judge = (src) => M.judge(src, '', false, false, 0, 0);

// 1. A file that cannot be parsed reports where. This is the whole reason an
//    editor can draw a squiggle without reading the prose -- §10.1 lets the
//    prose be reworded, so a position obtainable only from it is a position
//    nothing may depend on.
{
    const r = judge(BROKEN);
    check('unparseable source exits 2', r.code === 2, r.code);
    check('unparseable source still produces JSON', r.json.length > 0, JSON.stringify(r.json));
    if (r.json) {
        const e = JSON.parse(r.json).error;
        check('the JSON carries an error object', !!e, r.json);
        check('with a line', e?.line === 2, e?.line);
        check('with a column', e?.column === 23, e?.column);
        check('with the offending source line', /sub wishes, 1;/.test(e?.source ?? ''), e?.source);
    }
}

// 2. Judging again gives prose again. `--json` is a flag on one call, and the
//    embedded build shares argv state between calls that the command line never
//    has to think about: judgment 1 asking for JSON must not turn judgment 2's
//    human-readable report into JSON.
{
    judge(GOOD);                    // internally runs --json as its second pass
    const r = judge(BROKEN);        // must answer a person, not a parser
    check('a later report is still prose', !r.output.trimStart().startsWith('{'),
          r.output.slice(0, 60));
    check('a later report still says what is wrong', /unexpected character/.test(r.output),
          r.output.slice(0, 60));
}

// 3. The two shapes agree. A judgment made through `judge()` must match the one
//    the command line makes, or the editor and the terminal would disagree
//    about the same file in front of the same person.
{
    const r = judge(GOOD);
    check('a clean exploit still exits 1', r.code === 1, r.code);
    const j = JSON.parse(r.json);
    check('and reports the exploit', j.exploits === 1, r.json);
}

// 4. A genie checked on its own, the entry an editor uses for a `.genie` file
//    no wish has referenced. Reachable only here -- there is no wish involved,
//    so `judge()` cannot express it.
{
    const good = M.checkGenie(M.defaultGenie());
    check('a good genie checks out', good.code === 0, good.code);
    const okj = JSON.parse(good.json);
    check('a good genie reports no error', okj.error === undefined, good.json);
    check('and says it is ok', okj.genie && okj.genie.ok === true, good.json);

    // A rule with its brace left off -- the silent case this whole feature is
    // about.
    const bad = M.checkGenie('counter wishes\ntoll 1\nrule R\n  layer ast\n');
    check('a broken genie fails to check', bad.code === 2, bad.code);
    const e = JSON.parse(bad.json).error;
    check('and reports where', e && e.line > 0 && e.column > 0, bad.json);
    check('and quotes the line', /layer ast/.test(e?.source ?? ''), e?.source);
}

if (!failed) console.log('  ok   embedded api');
process.exit(failed);
