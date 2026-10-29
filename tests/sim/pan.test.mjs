// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// PANFLUTE (Denis Rosset's pan flute, engines/pan/): silent without breath, speaks with it and stops with it; in tune;
// keys outside his pipes folded by octaves into them; a hop (a slur) sounds the new pipe; one layer at a time (a
// second is refused); his bend follows the controller BEND CC names.
export default function (t) {
  const { ex, C, L1, SG, check, run } = t;
  const pan = t.engine(C.ENG_ID_PAN);
  if (pan < 0) { console.log('skip  PANFLUTE: not in this build'); return; }
  const cents = (f, f0) => 1200 * Math.log2(f / f0);
  const hz = (k) => 440 * Math.pow(2, (k - 69) / 12);
  const fresh = () => {
    ex.sim_preset(255);
    ex.sim_par_set(SG(C.S_REV_MIX), 0); ex.sim_par_set(SG(C.S_DLY_MIX), 0);
    ex.sim_par_set(L1(C.P_ENGINE), pan);
  };
  fresh();
  check(ex.sim_par_get(L1(C.P_ENGINE)) === C.ENG_ID_PAN, 'PANFLUTE: layer 1 takes it');
  ex.sim_midi(0xB0, 2, 0); ex.sim_midi(0x90, 79, 80);
  const none = run(300);
  ex.sim_midi(0xB0, 2, 60); run(400);
  const pk = run(300);
  const f = t.pitch(300, 600, 1000);
  ex.sim_midi(0xB0, 2, 0); run(400);
  ex.sim_midi(0x80, 79, 0);
  const after = run(1500), tail = run(300);
  check(none < 1e-4 && pk > 0.02 && pk < 1 && tail < 1e-3, `PANFLUTE: silent without breath (${none.toExponential(1)}), speaks (${pk.toFixed(3)}), stops with it (${tail.toExponential(1)}, ${after.toFixed(3)} on the way)`);
  check(Math.abs(cents(f, hz(79))) < 25, `PANFLUTE: G5 in tune (${f.toFixed(1)} Hz, ${cents(f, hz(79)).toFixed(1)} cents)`);
  // folded: C4 (60) is under his G4: it plays C5
  fresh();
  ex.sim_midi(0x90, 60, 80); ex.sim_midi(0xB0, 2, 60); run(400);
  const fc = t.pitch(300, 400, 700);
  ex.sim_midi(0xB0, 2, 0); ex.sim_midi(0x80, 60, 0); run(1500);
  check(Math.abs(cents(fc, hz(72))) < 25, `PANFLUTE: C4 folded to C5 (${fc.toFixed(1)} Hz)`);
  // a hop: a slur under held breath lands on the new pipe
  fresh();
  ex.sim_midi(0x90, 74, 80); ex.sim_midi(0xB0, 2, 60); run(400);
  ex.sim_midi(0x90, 79, 80); ex.sim_midi(0x80, 74, 0); run(300);
  const fh = t.pitch(300, 600, 1000);
  ex.sim_midi(0xB0, 2, 0); ex.sim_midi(0x80, 79, 0); run(1500);
  check(Math.abs(cents(fh, hz(79))) < 25, `PANFLUTE: a slur D5 -> G5 lands on G5 (${fh.toFixed(1)} Hz)`);
  // one layer at a time
  fresh();
  ex.sim_par_set(t.L2(C.P_ENGINE), pan);
  check(ex.sim_par_get(t.L2(C.P_ENGINE)) !== C.ENG_ID_PAN, 'PANFLUTE: a second layer is refused');
  // the bend: BEND CC 1, the lip from his markers (rest 0 .. full 90 = a whole tone down)
  fresh();
  ex.sim_par_set(L1(C.P_E0 + 8), 1);
  ex.sim_midi(0xB0, 1, 0); ex.sim_midi(0x90, 79, 80); ex.sim_midi(0xB0, 2, 60); run(400);
  const f0 = t.pitch(300, 600, 1000);
  ex.sim_midi(0xB0, 1, 33); run(400);
  const f1 = t.pitch(300, 600, 1000);
  ex.sim_midi(0xB0, 2, 0); ex.sim_midi(0x80, 79, 0); run(1500);
  check(cents(f1, f0) < -60 && cents(f1, f0) > -140, `PANFLUTE: BEND CC 1 at the semitone's plateau bends ${cents(f1, f0).toFixed(0)} cents`);
}
