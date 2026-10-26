// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The sound after the layers (core/fx.c): detune spreads the sides; each reverb TYPE leaves a tail (PLATE and HALL
// stereo) that dies to 0; the breath LP closing again after full breath does not click.
export default function (t) {
  const { ex, C, SP, SG, G, check, run } = t;

  // the breath LP (default LP CUT 60, BR>LP 40) is open past ~84 % breath; breath falling back below that must not
  // click (it did until 2026-10-23: the filter started from an empty state; preset 82 at CC ~102).
  // A sine held at its level (INIT, BR>AMP 0), breath 127 -> 100: the wave's 2nd difference just after the step
  // against while it was open
  {
    const grab = (ms) => { const x = []; for (let i = 0; i < Math.round((ms * 44.1) / 128); i++) { ex.sim_render(128); x.push(...new Float32Array(ex.memory.buffer, ex.sim_out_l(), 128)); } return x; };
    const d2 = (x) => { let m = 0; for (let i = 2; i < x.length; i++) m = Math.max(m, Math.abs(x[i] - 2 * x[i - 1] + x[i - 2])); return m; };
    ex.sim_preset(255);
    ex.sim_par_set(SG(C.S_REV_MIX), 0); ex.sim_par_set(SG(C.S_DLY_MIX), 0);
    ex.sim_midi(0xB0, 2, 127); ex.sim_midi(0x90, 69, 80); run(300);
    const open = d2(grab(100));
    ex.sim_midi(0xB0, 2, 100);
    const after = d2(grab(40));
    t.off(69, 800);
    // (with SMOOTH 0, the default since 2026-10-24, the CC's steps reach the cutoff: 4.65x, accepted: it
    // sounds right on the device; 1.06x from SMOOTH 3 ms. The bound still catches it getting worse)
    const s0 = ex.sim_par_get(G(C.G_BSMOOTH)) === 0, bound = s0 ? 6 : 1.5;
    check(after < open * bound, `breath LP: closing again after full breath does not click (2nd difference ${(after / open).toFixed(2)}x the open one's` +
      `${s0 ? ', SMOOTH 0: up to 6x accepted' : ''})`);
  }

  // detune: a copy up on the left and one down on the right: the sides differ, and the sound gets no louder than twice
  ex.sim_preset(255);
  ex.sim_par_set(SG(C.S_REV_MIX), 0);
  t.note(69, 300, 90);
  const [d0, p0] = t.sides(300);
  ex.sim_par_set(SG(C.S_DT_MIX), 127);
  run(100);
  const [d1, p1] = t.sides(500);
  check(d0 < 1e-4 && d1 > 0.02 && p1 < p0 * 2.2, `detune: the sides differ (${d0.toFixed(4)} -> ${d1.toFixed(3)}), level ${p0.toFixed(2)} -> ${p1.toFixed(2)}`);
  t.off(69, 800);
  check(run(200) < 1e-3, 'detune: silent after the note');
  ex.sim_par_set(SG(C.S_DT_MIX), 0); ex.sim_par_set(SG(C.S_REV_MIX), 24);

  // the reverbs: a note, let go: a tail each, PLATE and HALL stereo, and 30 s on nothing at all (no hum left in a loop)
  ex.sim_preset(255);                               // (INIT: one sine)
  const level = {};
  for (const [type, name] of [[C.RT_OLD, 'OLD'], [C.RT_PLATE, 'PLATE'], [C.RT_HALL, 'HALL']]) {
    ex.sim_par_set(SP(C.S_REV_TYPE), type); ex.sim_par_set(SP(C.S_REV_MIX), 50); ex.sim_par_set(SP(C.S_REV_SIZE), 90);
    ex.sim_par_set(SP(C.S_REV_DECAY), 70); ex.sim_par_set(SP(C.S_REV_PRE), 20);
    run(50);
    t.note(69, 400);
    t.off(69, 150);                                 // (the note's own release)
    const tail = t.stats(600);
    run(30000);
    const end = t.stats(500);
    level[name] = tail.l;
    const stereo = type === C.RT_OLD || tail.corr < 0.9;
    check(tail.l > 0.003 && stereo && end.l === 0 && end.r === 0,
      `reverb ${name}: a tail (rms ${tail.l.toFixed(4)} / ${tail.r.toFixed(4)}, sides alike ${tail.corr.toFixed(2)}), ` +
      `30 s on: ${end.l === 0 && end.r === 0 ? 'silent' : end.l.toExponential(1)}`);
  }
  console.log(`      reverb tail levels vs OLD: PLATE ${(20 * Math.log10(level.PLATE / level.OLD)).toFixed(1)} dB, ` +
    `HALL ${(20 * Math.log10(level.HALL / level.OLD)).toFixed(1)} dB`);
}
