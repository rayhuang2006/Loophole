// The compiler runs here, not on the page's thread.
//
// `--hunt` walks every wish program within a bound: nineteen seconds natively
// on the default bounds, longer under wasm. Calling that synchronously from the
// page would freeze the tab outright — no spinner, no cancel, no scrolling —
// and it is behind a button anyone would press first.
importScripts('./loophole.js');

const ready = createLoophole().then((M) => {
  // Hand the page the two constants it needs, so it does not have to
  // instantiate a second copy of the module to read them.
  self.postMessage({ type: 'ready', genie: M.defaultGenie(), versions: M.versions() });
  return M;
});

self.onmessage = async (e) => {
  const { id, wish, genie, hunt, maxStmts, maxWishes } = e.data;
  try {
    const M = await ready;
    const r = M.judge(wish, genie, hunt, true, maxStmts | 0, maxWishes | 0);
    self.postMessage({ id, code: r.code, output: r.output });
  } catch (err) {
    self.postMessage({ id, error: String(err) });
  }
};
