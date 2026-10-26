// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// TONE (core/tone.c), the stage after every layer's engine: the page (EDIT's third), a type's defaults, and what each
// type does, measured on an ANALOG saw: FILTER darkens / takes the fundamental away, SWEEP bites bright and settles,
// TALK's vowel follows breath, SUB OCT adds the octave below; each silent after the note.
export default function (t) {
  const { ex, C, L1, SG, check, run } = t;
  const analog = t.engine(C.ENG_ID_ANALOG);
  if (analog < 0) { console.log('skip  TONE: no ANALOG in this build'); return; }
  const grab = (ms) => { const b = []; for (let i = 0; i < Math.round(ms * 44.1 / 128); i++) { ex.sim_render(128); b.push(...new Float32Array(ex.memory.buffer, ex.sim_out_l(), 128)); } return b; };
  const rms = (b) => Math.sqrt(b.reduce((a, v) => a + v * v, 0) / b.length);
  const bright = (b) => { let d = 0; for (let i = 1; i < b.length; i++) d += (b[i] - b[i - 1]) ** 2; return Math.sqrt(d / b.length) / (rms(b) || 1); };
  const fresh = (type, a, b, c) => {
    ex.sim_preset(255); ex.sim_par_set(SG(C.S_REV_MIX), 0); ex.sim_par_set(SG(C.S_LPF), 127);   // (the mix's LP open)
    ex.sim_par_set(L1(C.P_ENGINE), analog);
    ex.sim_par_set(L1(C.P_TONE), type);
    if (a !== undefined) ex.sim_par_set(L1(C.P_TONE_A), a);
    if (b !== undefined) ex.sim_par_set(L1(C.P_TONE_B), b);
    if (c !== undefined) ex.sim_par_set(L1(C.P_TONE_C), c);
  };
  const hold = (breath = 100, ms = 400) => { ex.sim_midi(0xB0, 2, breath); ex.sim_midi(0x90, 57, 80); run(ms); return grab(300); };
  const quiet = () => { t.off(57, 1500); return run(200) < 1e-3; };

  // the page: EDIT, EDIT, EDIT; a type's knobs start at its defaults
  ex.sim_preset(255);
  t.press(C.BTN_EDIT); t.press(C.BTN_EDIT); t.press(C.BTN_EDIT);
  const page = ex.sim_page_cur();
  ex.sim_par_set(L1(C.P_TONE), C.TN_FILTER);
  const a = ex.sim_par_get(L1(C.P_TONE_A)), mode = ex.sim_par_get(L1(C.P_TONE_C));
  ex.sim_par_set(L1(C.P_TONE), C.TN_SUB);
  const lev = ex.sim_par_get(L1(C.P_TONE_A));
  check(page === C.PG_TONE && a === 80 && mode === 0 && lev === 70, `TONE: EDIT's third page (${page}), FILTER starts at CUTOFF 80 / LP, SUB OCT at LEVEL 70`);

  // OFF leaves the engine as it is; FILTER LP darkens, HP takes the fundamental (most of a saw's level) away
  fresh(C.TN_OFF); const off = hold(); const ok0 = quiet();
  fresh(C.TN_FILTER, 30, 20, 0); const lp = hold(); const ok1 = quiet();
  fresh(C.TN_FILTER, 90, 20, 2); const hp = hold(); const ok2 = quiet();
  check(bright(lp) < bright(off) * 0.6 && rms(hp) < rms(off) * 0.8 && ok0 && ok1 && ok2,
    `TONE FILTER: LP darker (${bright(off).toFixed(2)} -> ${bright(lp).toFixed(2)}), HP thinner (${rms(off).toFixed(3)} -> ${rms(hp).toFixed(3)}), silent after`);

  // SWEEP: brighter just after breath jumps in than once it has settled
  fresh(C.TN_SWEEP, 30, 110, 60);
  ex.sim_midi(0xB0, 2, 0); ex.sim_midi(0x90, 57, 80); run(300);
  ex.sim_midi(0xB0, 2, 110); run(10);
  const bite = grab(60); run(1200);
  const settled = grab(200), ok3 = quiet();
  check(bright(bite) > bright(settled) * 1.3 && ok3, `TONE SWEEP: an attack bites (${bright(bite).toFixed(2)}), then settles (${bright(settled).toFixed(2)})`);

  // TALK: the vowel moves with breath: soft and hard breath sound different (brightness), both speak
  fresh(C.TN_TALK, 10, 63, 80);
  const soft = hold(40), hard = (ex.sim_midi(0xB0, 2, 120), run(300), grab(300)), ok4 = quiet();
  check(rms(soft) > 0.01 && rms(hard) > 0.01 && bright(hard) > bright(soft) * 1.2 && ok4,   // (U .. I: brighter as breath opens the vowel)
    `TONE TALK: the vowel follows breath (${bright(soft).toFixed(2)} -> ${bright(hard).toFixed(2)})`);

  // SUB OCT: a sine layer gains the octave below (A3 220 Hz: the period found is twice as long)
  ex.sim_preset(255); ex.sim_par_set(SG(C.S_REV_MIX), 0); ex.sim_par_set(L1(C.P_ENGINE), t.engine(C.ENG_ID_SINE));
  ex.sim_par_set(L1(C.P_TONE), C.TN_SUB); ex.sim_par_set(L1(C.P_TONE_A), 127); ex.sim_par_set(L1(C.P_TONE_B), 0);
  ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 69, 80); run(400);
  const f = t.pitch(300, 150, 600), ok5 = (t.off(69, 1500), run(200) < 1e-3);
  check(Math.abs(f - 220) < 8 && ok5, `TONE SUB OCT: A4 with the octave below: the period of ${f.toFixed(1)} Hz (220), silent after`);
}
