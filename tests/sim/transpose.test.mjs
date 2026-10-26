// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// Transpose: a held note is cut, the next plays in the new key, nothing sticks; OCT- / OCT+ press, hold and both;
// tilt -> pitch bend.
export default function (t) {
  const { ex, C, L1, SG, G, check, run, freq } = t;
  ex.sim_preset(255);
  ex.sim_par_set(SG(C.S_REV_MIX), 0);               // (a dry sine, for the pitch: no reverb, breath leaves 2ND alone)
  ex.sim_par_set(L1(C.P_BR_SHP), 0);
  t.note(69, 300);
  const f0 = freq(300);
  ex.sim_par_set(G(C.G_XPOSE), 3);
  run(100);
  const cut = run(100);
  ex.sim_midi(0x90, 71, 80); run(300);              // (69 still held: a new note now)
  const f1 = freq(300);
  check(Math.abs(f0 - 440) < 8 && cut < 1e-3 && Math.abs(f1 - 587.3) < 10,
    `transpose +3: the held note is cut (${cut.toFixed(4)}), the next plays transposed (${f0.toFixed(0)} Hz -> ${f1.toFixed(0)} Hz, B4+3 = 587)`);
  ex.sim_midi(0x80, 71, 0); t.off(69, 800);
  check(ex.sim_note() === -1 && run(200) < 1e-3, 'transpose: silent after the note-offs (none stuck)');
  ex.sim_par_set(G(C.G_XPOSE), 0);
  run(50);

  // OCT- / OCT+: a press a semitone (when let go), a hold an octave; both together: 0 at once, nothing when let go
  const xp = () => ex.sim_par_get(G(C.G_XPOSE));
  const seen = [];
  t.press(C.BTN_OCT_UP); seen.push(xp());           // 1
  t.hold(C.BTN_OCT_UP); seen.push(xp());            // 13
  t.hold(C.BTN_OCT_DOWN); seen.push(xp());          // 1
  t.press(C.BTN_OCT_DOWN); t.press(C.BTN_OCT_DOWN); seen.push(xp());   // -1
  ex.sim_button(C.BTN_OCT_UP, 1); run(30); ex.sim_button(C.BTN_OCT_DOWN, 1); run(30); seen.push(xp());   // 0 at once
  ex.sim_button(C.BTN_OCT_UP, 0); run(30); ex.sim_button(C.BTN_OCT_DOWN, 0); run(30); seen.push(xp());   // still 0
  check(seen.join() === '1,13,1,-1,0,0', `OCT: press +1, hold +12, hold -12, 2 presses -2, both 0, let go 0 (${seen.join()})`);

  // tilt -> pitch bend (GLO, TILT>BEND): the re.corder's rotation CC 52 (centre 64), a dead zone, a ramp to RANGE % of BEND
  const bend = (cc) => { ex.sim_midi(0xB0, 52, cc); run(20); return ex.sim_bend16(); };
  ex.sim_par_set(G(C.G_BEND), 2);
  const c = bend(64), dflt = bend(127);
  ex.sim_par_set(G(C.G_TILT_RANGE), 100);
  const dz = bend(70), out = bend(71), outf = ex.sim_bend_fine(), up = bend(127), down = bend(0);
  ex.sim_par_set(G(C.G_TILT_INV), 1);
  const inv = bend(127);
  ex.sim_par_set(G(C.G_TILT_INV), 0);
  ex.sim_par_set(G(C.G_TILT_CC), 0);
  const before = bend(64); ex.sim_midi(0xB0, 52, 127); run(20); const off = ex.sim_bend16() - before;
  check(c === 0 && dflt === 3 && dz === 0 && out === 0 && outf > 0 && up === 31 && down === -32 && inv === -31 && off === 0,
    `tilt: centre ${c}, default 10% ${dflt}, dead zone ${dz}, just past it ${out} + fine ${outf}, full ${up} / ${down}, inverted ${inv}, off: no change ${off} (1/16 st, BEND 2)`);
}
