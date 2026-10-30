// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The routes (core/routes.c, notes/05-controls.md): a layer's defaults (BEND -> PITCH; an engine's own, what breath
// moved in it); B and AO (A past OVER) on a target; a moved knob put back after its block (the UI never sees it);
// no room past NCELL; saved and loaded with the preset; HOME's grid drawn.
export default function (t) {
  const { ex, C, G, SG, LN, CELL, check, run, freq } = t;
  const cents = (f, f0) => 1200 * Math.log2(f / f0);
  const cell = (l, s, tg) => ex.sim_par_get(CELL(l, s, tg));
  const fresh = () => {
    ex.sim_preset(255);
    ex.sim_par_set(SG(C.S_REV_MIX), 0); ex.sim_par_set(SG(C.S_DLY_MIX), 0);
  };

  // defaults: INIT's sine BEND -> PITCH only; ERS: A -> EXCITE 100 %, A -> AMOUNT 31 % (BR>SHP 63, BR>TONE 40 as were)
  fresh();
  const init = [cell(0, C.SRC_BEND, C.T_PITCH), cell(0, C.SRC_A, C.T_EXCITE)];
  ex.sim_par_set(LN(0, C.P_ENGINE), t.engine(C.ENG_ID_ERS));
  const ers = [cell(0, C.SRC_BEND, C.T_PITCH), cell(0, C.SRC_A, C.T_EXCITE), cell(0, C.SRC_A, C.T_KNOB + 7)];
  ex.sim_par_set(LN(0, C.P_TONE), C.TN_FILTER);
  const flt = cell(0, C.SRC_A, C.T_TONE_A);
  ex.sim_par_set(LN(0, C.P_TONE), C.TN_TALK);
  const talk = cell(0, C.SRC_A, C.T_TONE_A);
  check(init.join() === '100,0' && ers.join() === '100,100,31' && flt === 31 && talk === 0,
    `routes: INIT BEND>PITCH ${init[0]}, no EXCITE (${init[1]}); ERS ${ers.join(' ')} (PITCH, EXCITE, AMOUNT); TONE FILTER A>CUTOFF ${flt}, TALK ${talk}`);

  // B -> PITCH 50 %: B (a CC of its own) bends by half the BEND range (4 st: +200 cents), BEND's tilt off
  fresh();
  ex.sim_par_set(G(C.G_TILT_CC), 0);
  ex.sim_par_set(G(C.G_CC_B), 20);
  ex.sim_par_set(CELL(0, C.SRC_B, C.T_PITCH), 50);
  ex.sim_midi(0xB0, 20, 0);
  t.note(69, 300);
  const f0 = freq(300);
  ex.sim_midi(0xB0, 20, 127); run(100);
  const f1 = freq(300);
  t.off(69, 800);
  check(Math.abs(cents(f1, f0) - 200) < 10, `routes: B > PITCH 50 % at full B: +${cents(f1, f0).toFixed(0)} cents (BEND 4 st)`);

  // AO (A past OVER 100) -> PITCH 100 %: nothing below it, the whole range at the top; A itself unchanged
  fresh();
  ex.sim_par_set(G(C.G_HI_A), 127);
  ex.sim_par_set(G(C.G_OVER_A), 100);
  ex.sim_par_set(CELL(0, C.SRC_AO, C.T_PITCH), 100);
  t.note(69, 300, 90);
  const g0 = freq(300);
  ex.sim_midi(0xB0, 2, 127); run(100);
  const g1 = freq(300);
  t.off(69, 800);
  ex.sim_par_set(G(C.G_OVER_A), 0); ex.sim_par_set(G(C.G_HI_A), 120);
  check(Math.abs(cents(g0, 440)) < 10 && Math.abs(cents(g1, g0) - 400) < 10,
    `routes: AO > PITCH: breath 90 under OVER 100 in tune (${cents(g0, 440).toFixed(1)}), 127: +${cents(g1, g0).toFixed(0)} cents`);

  // a moved knob is put back: A -> PAN 100 % under full breath, PAN still 0 for the UI and storage
  fresh();
  ex.sim_par_set(CELL(0, C.SRC_A, C.T_PAN), 100);
  t.note(69, 200, 127);
  const { l, r } = t.stats(200);
  const pan = ex.sim_par_get(LN(0, C.P_PAN));
  t.off(69, 800);
  check(pan === 0 && r > l * 2, `routes: A > PAN moves the sound right (L ${l.toFixed(3)}, R ${r.toFixed(3)}), the knob stays ${pan}`);

  // no room past NCELL
  fresh();
  let n = 0;
  for (let k = 0; k < 20; k++) for (const s of [C.SRC_B, C.SRC_C]) { ex.sim_par_set(CELL(1, s, C.T_KNOB + k), 10); n += cell(1, s, C.T_KNOB + k) === 10; }
  const used = n;                                   // (INIT has 5: BEND > PITCH on its 3 layers, A > EXCITE on the 2 OFF)
  check(used === 27, `routes: ${used} more fit beside INIT's 5, then no room (32 a preset)`);

  // saved with the preset, loaded back
  fresh();
  ex.sim_par_set(CELL(0, C.SRC_C, C.T_LEVEL), -40);
  t.saveTo(70);
  ex.sim_preset(0);
  ex.sim_preset(70);
  check(cell(0, C.SRC_C, C.T_LEVEL) === -40 && cell(0, C.SRC_BEND, C.T_PITCH) === 100,
    `routes: saved and loaded (C > LEVEL ${cell(0, C.SRC_C, C.T_LEVEL)}, BEND > PITCH ${cell(0, C.SRC_BEND, C.T_PITCH)})`);

  // HOME: the grid; KNOB 1 / 2 move the cursor, KNOB 3 the amount of the route under it
  fresh();
  ex.sim_page(C.PG_BREATH); run(60);
  ex.sim_enc(C.ENC_KNOB1 + 1, 1); run(30);        // (column 2: A)
  ex.sim_enc(C.ENC_KNOB1 + 2, 5); run(60);        // (row 1: LEVEL)
  check(cell(0, C.SRC_A, C.T_LEVEL) > 0, `routes: the grid's KNOB 3 sets A > LEVEL (${cell(0, C.SRC_A, C.T_LEVEL)} %)`);
}
