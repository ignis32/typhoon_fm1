// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// Breath is smoothed in one place (G_BSMOOTH, global): a preset cannot change it, and the engines add no lag of their
// own (no hidden lags). A breath step 0 -> 127 under a held note: the level's time to 90 %
// follows SMOOTH (a one-pole: ~2.3 time constants), on a plain sine and on EVA at its fastest RESPONSE.
export default function (t) {
  const { ex, C, L1, SG, SP, G, check, run } = t;
  const env = (n) => {                              // n ms of the left side's RMS, 1 ms windows
    const out = [], buf = [];
    while (out.length < n) {
      ex.sim_render(128); buf.push(...new Float32Array(ex.memory.buffer, ex.sim_out_l(), 128));
      while (buf.length >= 44) { const w = buf.splice(0, 44); out.push(Math.sqrt(w.reduce((s, v) => s + v * v, 0) / 44)); }
    }
    return out.slice(0, n);
  };
  const rise = (set) => {                           // ms to 90 % of the level reached after 300 ms
    ex.sim_preset(255);
    ex.sim_par_set(SG(C.S_REV_MIX), 0); ex.sim_par_set(SG(C.S_DLY_MIX), 0);
    ex.sim_par_set(SG(C.S_LPF), 127);               // (the mix's breath LP open: it is not what is measured)
    set();
    ex.sim_midi(0xB0, 2, 0); ex.sim_midi(0x90, 69, 80); run(400);
    ex.sim_midi(0xB0, 2, 127);
    const e = env(400), fin = e.slice(300).reduce((s, v) => s + v, 0) / 100;
    t.off(69, 1500);
    return e.findIndex((v) => v >= 0.9 * fin) + 1;
  };
  check(ex.sim_par_get(G(C.G_BSMOOTH)) === 0, `SMOOTH: global, 0 (none) by default (${ex.sim_par_get(G(C.G_BSMOOTH))})`);
  ex.sim_par_set(G(C.G_BSMOOTH), 40);
  ex.sim_preset(0); ex.sim_preset(64); ex.sim_preset(255);
  check(ex.sim_par_get(G(C.G_BSMOOTH)) === 40, 'SMOOTH: loading presets leaves it alone (no preset has its own)');
  const sine = () => ex.sim_par_set(L1(C.P_BR_AMP), 127);   // (INIT's sine, its loudness all breath)
  ex.sim_par_set(G(C.G_BSMOOTH), 0);
  const s0 = rise(sine);
  ex.sim_par_set(G(C.G_BSMOOTH), 5);
  const s5 = rise(sine);
  ex.sim_par_set(G(C.G_BSMOOTH), 40);
  const s40 = rise(sine);
  ex.sim_par_set(G(C.G_BSMOOTH), 0);                   // (the default again)
  // (audio time from the breath: the first frames after it may come from a block rendered just before, up to 5.8 ms)
  check(s0 <= s5 && s0 <= 8 && s5 <= 16 && s40 >= 60,
    `SMOOTH sets the lag: a sine to 90 % in ${s0} ms at 0 (none), ${s5} ms at 5 ms, ${s40} ms at 40 ms`);
  const eva = t.engine(C.ENG_ID_EVA);
  if (eva >= 0) {
    const fast = rise(() => { ex.sim_par_set(L1(C.P_ENGINE), eva); ex.sim_engine_preset(0, 0); ex.sim_par_set(L1(C.P_E0 + 15), 127); ex.sim_par_set(L1(C.P_E0 + 6), 0); });
    check(fast <= 24, `EVA at RESPONSE 127: to 90 % in ${fast} ms (SMOOTH as by default, 0, and its own 2 ms attack, nothing hidden)`);
  }
}
