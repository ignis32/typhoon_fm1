// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// QUIRKS (core/layers.c layer_quirk): each fingered note a little off, the same way every time. On INIT's sine: QUIRKS
// 0 in tune; at 100 % a note is off by up to 12 cents, the same tongued again and slurred into; another note, another
// offset; another QUIRK SET, another; transposed, the fingering keeps its own (keyed on the note that comes in).
export default function (t) {
  const { ex, C, L1, G, check } = t;
  const grab = (ms) => { const x = []; for (let i = 0; i < Math.round(ms * 44.1 / 128); i++) { ex.sim_render(128); x.push(...new Float32Array(ex.memory.buffer, ex.sim_out_l(), 128)); } return x; };
  const hz = (x) => {                               // upward zero crossings, interpolated: a pure tone to ~0.1 cent
    const at = [];
    for (let i = 1; i < x.length; i++) if (x[i - 1] < 0 && x[i] >= 0) at.push(i - 1 + x[i - 1] / (x[i - 1] - x[i]));
    return (at.length - 1) * 44100 / (at[at.length - 1] - at[0]);
  };
  const cents = (f, note) => 1200 * Math.log2(f / (440 * 2 ** ((note - 69) / 12)));
  const fresh = (q, set = 0) => {
    ex.sim_preset(255); ex.sim_par_set(t.SG(C.S_REV_MIX), 0); ex.sim_par_set(t.SG(C.S_DLY_MIX), 0);
    ex.sim_par_set(L1(C.P_E0 + 16), 127);          // (FM's breath LP open)
    ex.sim_par_set(L1(C.P_QUIRKS), q); ex.sim_par_set(L1(C.P_QSET), set);
  };
  const tongued = (note) => { ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, note, 80); t.run(200); const f = hz(grab(400)); t.off(note, 300); return f; };

  // (B4, 71: off by ~8 cents in set 0, a clear one; A4, 69: another)
  fresh(0);
  const c0 = cents(tongued(71), 71);
  fresh(100);
  const a = cents(tongued(71), 71), a2 = cents(tongued(71), 71), b = cents(tongued(69), 69);
  // slurred: A4 held, into B4
  ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 69, 80); t.run(200); grab(100);
  ex.sim_midi(0x90, 71, 80); ex.sim_midi(0x80, 69, 0); t.run(100); const sl = cents(hz(grab(400)), 71);
  t.off(71, 300);
  check(Math.abs(c0) < 0.3 && Math.abs(a) > 3 && Math.abs(a) <= 12.2 && Math.abs(a - a2) < 0.3 && Math.abs(a - sl) < 0.3 && Math.abs(a - b) > 0.3,
    `QUIRKS: 0 in tune (${c0.toFixed(2)} ct); 100 % B4 ${a.toFixed(2)} ct, again ${a2.toFixed(2)}, slurred into ${sl.toFixed(2)}; A4 its own ${b.toFixed(2)}`);
  fresh(100, 1);
  const s1 = cents(tongued(71), 71);
  fresh(50);
  const half = cents(tongued(71), 71);
  check(Math.abs(s1 - a) > 0.3 && Math.abs(half - a / 2) < 0.3,
    `QUIRK SET 1: B4 another (${s1.toFixed(2)} ct); QUIRKS 50 %: half of 100 %'s (${half.toFixed(2)} ct of ${a.toFixed(2)})`);
  fresh(100);
  ex.sim_par_set(G(C.G_XPOSE), 2); t.run(100);    // (a transpose cuts the sounding note: settled first)
  const tr = cents(tongued(71), 73);               // (fingered B4, sounding C#5)
  ex.sim_par_set(G(C.G_XPOSE), 0); t.run(100);
  check(Math.abs(tr - a) < 0.3, `QUIRKS follow the fingering: B4 transposed +2 is off as B4 is (${tr.toFixed(2)} ct, B4 ${a.toFixed(2)})`);
}
