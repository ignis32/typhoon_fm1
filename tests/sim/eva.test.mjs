// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// EVA (breath shaped per destination): every preset speaks with breath and is silent without it and after the note;
// breath is the envelope (a ramp: louder and brighter, step by step); a slur does not dip; the sweep kicks on an
// articulated note only; vibrato stays out below its onset and comes in at full breath; in tune.
export default function (t) {
  const { ex, C, L1, SG, check, run } = t;
  const eva = t.engine(C.ENG_ID_EVA);
  if (eva < 0) { console.log('skip  EVA: not in this build'); return; }
  const grab = (ms) => { const x = []; for (let i = 0; i < Math.round((ms * 44.1) / 128); i++) { ex.sim_render(128); x.push(...new Float32Array(ex.memory.buffer, ex.sim_out_l(), 128)); } return x; };
  const rms = (x) => Math.sqrt(x.reduce((s, v) => s + v * v, 0) / x.length);
  const zc = (x) => { let n = 0; for (let i = 1; i < x.length; i++) if ((x[i - 1] < 0) !== (x[i] < 0)) n++; return n; };   // (brightness: crossings)
  const hf2 = (x) => { let s = 0; for (let i = 2; i < x.length; i++) s += (x[i] - 2 * x[i - 1] + x[i - 2]) ** 2; return Math.sqrt(s / x.length) / (rms(x) || 1); };   // (the top: second difference)
  const hf = (x) => { let s = 0; for (let i = 1; i < x.length; i++) s += (x[i] - x[i - 1]) ** 2; return Math.sqrt(s / x.length) / (rms(x) || 1); };
  const fresh = (k) => {
    ex.sim_preset(255);
    ex.sim_par_set(SG(C.S_REV_MIX), 0); ex.sim_par_set(SG(C.S_DLY_MIX), 0);
    ex.sim_par_set(L1(C.P_ENGINE), eva);
    return ex.sim_engine_preset(0, k);
  };
  let k = 0;
  for (; fresh(k); k++) {
    ex.sim_midi(0xB0, 2, 0); ex.sim_midi(0x90, 69, 80);
    const none = run(300);
    ex.sim_midi(0xB0, 2, 110); run(300);
    const pk = run(300);
    t.off(69, 1500);
    check(none < 1e-4 && pk > 0.05 && pk < 1 && run(200) < 1e-4, `EVA preset ${k}: silent without breath (${none.toExponential(1)}), speaks (${pk.toFixed(3)}), silent after`);
  }
  check(k === 18, `EVA: ${k} presets`);
  // breath is the envelope: a ramp 0 -> 127 in 8 steps, louder and brighter each step
  fresh(0);
  ex.sim_midi(0x90, 57, 80);
  const lv = [], br = [];
  for (let b = 16; b <= 127; b += 16) { ex.sim_midi(0xB0, 2, b); run(150); const x = grab(150); lv.push(rms(x)); br.push(hf(x)); }
  t.off(57, 1000);
  const up = (a) => a.every((v, i) => i === 0 || v > a[i - 1] * 0.98);
  check(up(lv) && up(br) && lv[lv.length - 1] > 4 * lv[0], `EVA: breath is the envelope, louder (${lv.map((v) => v.toFixed(3)).join(' ')}) and brighter (${br.map((v) => v.toFixed(2)).join(' ')})`);
  // a slur: no dip at the change; the sweep kicks an articulated note (brighter than with SWEEP off, the same
  // moment), not the slur
  const kickOf = (sweep) => {
    fresh(0);
    ex.sim_par_set(L1(C.P_E0 + 6), 0);            // (no vibrato in the way)
    ex.sim_par_set(L1(C.P_E0 + 5), sweep);
    ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 60, 80);
    run(25);
    const kick = hf(grab(25));
    run(400);
    ex.sim_midi(0x90, 64, 80); ex.sim_midi(0x80, 60, 64);
    run(25);
    const slur = hf(grab(25));
    let low = 1;
    for (let i = 0; i < 10; i++) low = Math.min(low, run(10));
    t.off(64, 1000);
    return { kick, slur, low };
  };
  const on = kickOf(100), off = kickOf(64);
  check(on.low > 0.05 && on.kick > off.kick * 1.15 && Math.abs(on.slur / off.slur - 1) < 0.05,
    `EVA: a slur keeps sounding (lowest ${on.low.toFixed(3)}); the sweep kicks a fresh note (${on.kick.toFixed(3)} vs ${off.kick.toFixed(3)} without), not the slur (${on.slur.toFixed(3)} vs ${off.slur.toFixed(3)})`);
  // vibrato: none at 40 % breath, there at full
  const wob = (b) => {
    fresh(0); ex.sim_midi(0xB0, 2, b); ex.sim_midi(0x90, 69, 80); run(1500);
    const f = []; for (let i = 0; i < 12; i++) f.push(t.pitch(40, 380, 510));
    t.off(69, 800);
    return Math.max(...f) / Math.min(...f);
  };
  const w40 = wob(50), w100 = wob(127);
  check(w40 < 1.004 && w100 > 1.008, `EVA: vibrato out below its onset (x${w40.toFixed(4)}), in at full breath (x${w100.toFixed(4)})`);
  fresh(0); ex.sim_par_set(L1(C.P_E0 + 6), 0);
  ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 69, 80); run(500);
  const f = t.pitch(300, 400, 480);
  t.off(69, 800);
  check(Math.abs(1200 * Math.log2(f / 440)) < 15, `EVA: A4 at ${f.toFixed(1)} Hz`);
  // EDIT 3 and 4: EVA's (EDIT x5 goes 1 2 3 4 TONE; an engine with 8 knobs skips them: tone.test.mjs)
  fresh(0);
  const pages = []; for (let i = 0; i < 5; i++) { t.press(C.BTN_EDIT); pages.push(ex.sim_page_cur()); }
  check(pages.join() === [C.PG_ENG1, C.PG_ENG2, C.PG_ENG3, C.PG_ENG4, C.PG_TONE].join(), `EVA: EDIT goes through its four pages, then TONE (${pages.join(' ')})`);
  // the new knobs each do something: a held note at 100, one knob moved from the init sound
  const E = (k) => L1(C.P_E0 + k);
  const take = (set, ms = 300, skip = 400) => {
    fresh(0); ex.sim_par_set(E(6), 0); set();
    ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 57, 80); run(skip);
    const x = grab(ms); t.off(57, 1200); return x;
  };
  const bh = hf2(take(() => {}));
  const flt = [1, 2, 3].map((f) => hf2(take(() => ex.sim_par_set(E(8), f))));
  check(flt[0] < bh * 0.9 && flt[1] > bh * 1.1 && flt[2] > bh * 1.5, `EVA FILTER: LP4 darker (${flt[0].toFixed(3)}), BP and HP brighter (${flt[1].toFixed(3)} ${flt[2].toFixed(3)}) than LP2 (${bh.toFixed(3)})`);
  const vo = (v) => hf2(take(() => { ex.sim_par_set(E(10), 127); ex.sim_par_set(E(9), v); }));
  const va = vo(0), vi = vo(64), vu = vo(127);
  check(vi > va * 1.1 && va > vu * 1.3, `EVA FORMANT: "i" brightest, "a", then "u" darkest (${vi.toFixed(3)} ${va.toFixed(3)} ${vu.toFixed(3)})`);
  const env = (x) => { const r = []; for (let i = 0; i + 441 <= x.length; i += 441) r.push(rms(x.slice(i, i + 441))); return Math.max(...r) / Math.min(...r); };
  const tm = env(take(() => { ex.sim_par_set(E(11), 127); ex.sim_midi(0xB0, 2, 127); }, 600, 1500)), t0 = env(take(() => ex.sim_midi(0xB0, 2, 127), 600, 1500));
  check(tm > 1.4 && t0 < 1.1, `EVA TREM: the level moves (x${tm.toFixed(2)}, without x${t0.toFixed(2)})`);
  const c1 = hf2(take(() => ex.sim_par_set(E(14), 127), 20, 5)), c0 = hf2(take(() => {}, 20, 5));
  check(c1 > c0 * 1.3, `EVA CHIFF: noise at the start (${c1.toFixed(3)} vs ${c0.toFixed(3)})`);
  const r0 = rms(take(() => ex.sim_par_set(E(15), 0), 30, 15)), r1 = rms(take(() => ex.sim_par_set(E(15), 127), 30, 15));
  check(r1 > r0 * 1.5, `EVA RESPONSE: fast comes up sooner (${r1.toFixed(3)} vs ${r0.toFixed(3)} after 15 ms)`);
  // the waves breath plays: SYNC, BR PWM, HARMONIC brighter blown harder (more than SAW gets from the filter alone,
  // the filter held open); SUB an octave down; SUPERSAW in tune
  const harm = (w, b) => {                        // the harmonics 1 .. 8 of A3 (Goertzel), the filter open far above
    fresh(0); ex.sim_par_set(E(6), 0); ex.sim_par_set(E(1), w); ex.sim_par_set(E(3), 127);
    ex.sim_midi(0xB0, 2, b); ex.sim_midi(0x90, 57, 80); run(600);
    const x = grab(400); t.off(57, 1200);
    return [1, 2, 3, 4, 5, 6, 7, 8].map((k) => {
      const c = 2 * Math.cos(2 * Math.PI * k * 220 / 44100); let s1 = 0, s2 = 0;
      for (const v of x) { const s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
      return Math.sqrt(s1 * s1 + s2 * s2 - c * s1 * s2);
    });
  };
  const cen = (h) => h.reduce((s, v, i) => s + (i + 1) * v, 0) / h.reduce((s, v) => s + v, 0);   // (in harmonics)
  const grow = (w) => cen(harm(w, 127)) / cen(harm(w, 30));
  const gs = grow(0), gy = grow(6), gh = grow(8);
  // (through the global breath curve: on LOW 0, the default since 2026-10-24, breath 30 is already a quarter blown, so
  // the growth is x1.21 / x1.26 SAW's; x1.3 and more on LOW 8)
  check(gy > gs * 1.15 && gh > gs * 1.15, `EVA SYNC, HARMONIC: higher harmonics blown harder (the centroid x${gy.toFixed(2)}, x${gh.toFixed(2)}; SAW x${gs.toFixed(2)})`);
  const pw = (b) => { const h = harm(7, b); return h[1] / h[0]; };
  const p45 = pw(45), p127 = pw(127);
  check(p45 > 3 * p127, `EVA BR PWM: thin blown softly (2nd harmonic ${p45.toFixed(2)} of the 1st), square blown hard (${p127.toFixed(3)})`);
  const pf = (w) => { fresh(0); ex.sim_par_set(E(6), 0); ex.sim_par_set(E(1), w); ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 69, 80); run(500); const f = t.pitch(300, 150, 600); t.off(69, 800); return f; };
  const fsub = pf(9), fsup = pf(10);
  check(Math.abs(1200 * Math.log2(fsub / 220)) < 15 && Math.abs(1200 * Math.log2(fsup / 440)) < 15, `EVA SUB at A3 (${fsub.toFixed(1)} Hz), SUPERSAW at A4 (${fsup.toFixed(1)} Hz)`);

  // the noises: WHITE, PINK, BROWN each about as loud as SAW (the filter open far above), darker in that order;
  // TUNED rings on the note blown hard, wider (brighter) blown softly
  const W_WHITE = 11, W_PINK = 12, W_BROWN = 13, W_TUNED = 14;
  const play = (w, b, note = 57) => {
    fresh(0); ex.sim_par_set(E(6), 0); ex.sim_par_set(E(1), w); ex.sim_par_set(E(3), 127); ex.sim_par_set(E(5), 64);
    ex.sim_midi(0xB0, 2, b); ex.sim_midi(0x90, note, 80); run(500);
    const x = grab(300); t.off(note, 1200);
    return x;
  };
  const saw = rms(play(0, 110)), nz = [W_WHITE, W_PINK, W_BROWN].map((w) => play(w, 110));
  const db = nz.map((x) => 20 * Math.log10(rms(x) / saw)), tops = nz.map(hf);
  check(db.every((d) => Math.abs(d) < 4.5) && tops[0] > tops[1] && tops[1] > tops[2],
    `EVA WHITE, PINK, BROWN: as loud as SAW (${db.map((d) => d.toFixed(1)).join(' ')} dB), darker in turn (${tops.map((v) => v.toFixed(2)).join(' ')})`);
  const hard = play(W_TUNED, 127), soft = play(W_TUNED, 35);
  const on220 = (x) => {                         // how much of it is on the note: A3's Goertzel bin against the whole
    const c = 2 * Math.cos(2 * Math.PI * 220 / 44100); let s1 = 0, s2 = 0;
    for (const v of x) { const s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    return Math.sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / (rms(x) * x.length);
  };
  const ft = zc(hard) / 2 / 0.3, dt = 20 * Math.log10(rms(hard) / saw), th = on220(hard), ts = on220(soft);
  check(Math.abs(1200 * Math.log2(ft / 220)) < 120 && th > 1.5 * ts && Math.abs(dt) < 4.5,
    `EVA TUNED: on A3 blown hard (${ft.toFixed(0)} Hz by its crossings, ${dt.toFixed(1)} dB of SAW), more of it on the note than blown softly (${th.toFixed(3)} vs ${ts.toFixed(3)})`);
  // a puff: PINK with the layer's ENVELOPE (ATTACK 0, a short DECAY, SUSTAIN 0): there on a tongued note, gone while
  // the breath holds, not again on a slur, again on the next tongued note
  fresh(0); ex.sim_par_set(E(1), W_PINK); ex.sim_par_set(L1(C.P_ATK), 0); ex.sim_par_set(L1(C.P_DEC), 30); ex.sim_par_set(L1(C.P_SUS), 0);
  ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 60, 80);
  const p0 = run(20); run(400); const p1 = run(100);
  ex.sim_midi(0x90, 62, 80); ex.sim_midi(0x80, 60, 64); const p2 = run(20); run(300);
  t.off(62, 800);
  ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 64, 80); const p3 = run(20); t.off(64, 800);
  check(p0 > 0.03 && p1 < p0 / 30 && p2 < p0 / 30 && p3 > 0.03,
    `EVA PINK as a puff: on a tongued note (${p0.toFixed(3)}), gone while blown (${p1.toExponential(1)}), not on a slur (${p2.toExponential(1)}), again tongued (${p3.toFixed(3)})`);
}
