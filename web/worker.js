// The compiler runs here, not on the page's thread.
//
// `--hunt` walks every wish program within a bound: nineteen seconds natively
// on the default bounds, longer under wasm. Calling that synchronously from the
// page would freeze the tab outright — no spinner, no cancel, no scrolling —
// and it is behind a button anyone would press first.
importScripts('./loophole.js');

const ready = createLoophole().then((M) => {
  // Hand the page what it needs, so it does not instantiate a second copy of a
  // 370 KB module just to read three constants.
  //
  // `keywords` is called defensively on purpose. This worker is never cached
  // and the module is, so for a few minutes after a deploy a browser can pair a
  // new worker with the previous module. Calling a function that build does not
  // have would throw here — before `ready` is ever posted — and the page would
  // sit on "loading" forever with no error anywhere. Colour is worth degrading;
  // it is not worth a dead page.
  let keywords = null;
  try { if (M.keywords) keywords = M.keywords(); } catch (_) {}
  self.postMessage({ type: 'ready', genie: M.defaultGenie(),
                     versions: M.versions(), keywords });
  return M;
});

self.onmessage = async (e) => {
  const { id, wish, genie, hunt, maxStmts, maxWishes } = e.data;
  try {
    const M = await ready;
    const r = M.judge(wish, genie, hunt, true, maxStmts | 0, maxWishes | 0);
    self.postMessage({ id, code: r.code, output: r.output, json: r.json });
  } catch (err) {
    self.postMessage({ id, error: String(err) });
  }
};
