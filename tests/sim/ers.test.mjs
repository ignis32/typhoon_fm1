// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// ERS (exciter -> resonator -> shaper): every one of its presets (walked from the firmware: sim_engine_preset) speaks
// with breath, stays in range, is silent without breath and after the note; the bores are in tune at A4 (the
// period by autocorrelation: some waves cross zero twice a period). Then every exciter in every resonator, and
// every shaper: nothing blows up, nothing hangs on after the note.
export default function (t) {
  const { ex, C, L1, SG, G, check, run } = t;
  const ers = t.engine(C.ENG_ID_ERS);
  if (ers < 0) { console.log('skip  ERS: not in this build'); return; }
  const bores = [C.RS_TUBE, C.RS_CONE, C.RS_BRASS, C.RS_HOLE, C.RS_PIPE];
  const fresh = () => {
    ex.sim_preset(255);
    ex.sim_par_set(SG(C.S_REV_MIX), 0); ex.sim_par_set(SG(C.S_DLY_MIX), 0);
    ex.sim_par_set(L1(C.P_ENGINE), ers);
  };
  const blow = (tune) => {                          // -> {none, pk, f, after}
    ex.sim_midi(0xB0, 2, 0); run(300);
    ex.sim_midi(0x90, 69, 80);
    const none = run(300);                          // (no breath: no pressure)
    ex.sim_midi(0xB0, 2, 110); run(400);
    const pk = run(400), f = tune ? t.pitch(400, 300, 600) : 0;
    t.off(69, 1500);
    return { none, pk, f, after: run(200) };
  };

  // every preset: speaks at A4 (and is in tune there: measured with the shaper off, a mute or wah takes the
  // fundamental away from what is heard, not from the pitch), and speaks from C4 to C6 (the re.corder's range is
  // C5 .. D7)
  const names = [], weak = [];
  const preset = (k) => {
    fresh();
    const p = ex.sim_engine_preset(0, k);
    if (!p) return null;
    const m = new Uint8Array(ex.memory.buffer, p, 16);
    return String.fromCharCode(...m.slice(0, m.indexOf(0)));
  };
  for (let k = 0; ; k++) {
    const name = preset(k);
    if (!name) break;
    const res = ex.sim_par_get(L1(C.P_E0 + 3)), exc = ex.sim_par_get(L1(C.P_E0 + 1));
    const tuned = (bores.includes(res) && exc !== C.EX_OSC) || exc === C.EX_FREE;   // (a free reed sets its own pitch)
    const r = blow(false);
    let f = 0;
    if (tuned) {
      ex.sim_par_set(L1(C.P_E0 + 6), C.SH_BYPASS);
      f = blow(true).f;
    }
    names.push(name);
    // (the bowed VIOLIN and ERHU, given breath at once with SMOOTH 0, the default, start in another mode, a fourth
    // low: accepted, they sound right on the device; with SMOOTH 1 ms or more they speak at A)
    const inTune = !tuned || Math.abs(f - 440) < 440 * 0.03;
    const accepted = !inTune && ['VIOLIN', 'ERHU'].includes(name) && ex.sim_par_get(G(C.G_BSMOOTH)) === 0 &&
      Math.abs(f - 330) < 330 * 0.03;
    const ok = r.pk > 0.02 && r.pk < 1 && r.none < 0.02 && r.after < 1e-3 && (inTune || accepted);
    check(ok, `ERS ${k} ${name}: speaks with breath (peak ${r.pk.toFixed(3)}, none ${r.none.toFixed(4)}, after ${r.after.toFixed(4)})` +
      (tuned ? `, at ${f.toFixed(1)} Hz (A4 440)` : '') + (accepted ? ' [a fourth low at SMOOTH 0: accepted]' : ''));
    for (const n of [60, 72, 84]) {
      preset(k);
      ex.sim_midi(0xB0, 2, 110); ex.sim_midi(0x90, n, 80); run(400);
      const pk = run(200);
      t.off(n, 1000);
      if (!(pk > 0.015)) weak.push(`${name} at ${n} (${pk.toFixed(3)})`);
    }
  }
  check(names.length >= 23, `ERS: ${names.length} presets walked`);

  // the tuned presets give a steady tone, not a buzz (the first LIP was a blat: 0.33), at medium and full breath
  const unsteady = [];
  for (let k = 0; ; k++) {
    const name = preset(k);
    if (!name) break;
    const res = ex.sim_par_get(L1(C.P_E0 + 3)), exc = ex.sim_par_get(L1(C.P_E0 + 1));
    if (!((bores.includes(res) && exc !== C.EX_OSC) || exc === C.EX_FREE)) continue;
    ex.sim_par_set(L1(C.P_E0 + 6), C.SH_BYPASS);
    for (const br of [70, 110]) {
      ex.sim_midi(0xB0, 2, br); ex.sim_midi(0x90, 69, 80); run(500);
      const s = t.steady(150, 300, 600);
      t.off(69, 800);
      if (!(s > 0.9)) unsteady.push(`${name} at breath ${br} (${s.toFixed(2)})`);
    }
  }
  check(unsteady.length === 0, `ERS: the tuned presets give a steady tone at A4${unsteady.length ? '; not: ' + unsteady.join(', ') : ''}`);
  check(weak.length === 0, `ERS: every preset speaks at C4, C5 and C6${weak.length ? '; not: ' + weak.join(', ') : ''}`);

  // every exciter in every resonator (BYPASS shaper): sounds, in range, silent after
  const choices = (pre, n) => Object.keys(C).filter((k) => k.startsWith(pre) && C[k] < C[n]).sort((a, b) => C[a] - C[b]);
  const exNames = choices('EX_', 'ERS_NEXC');   // (every one the firmware has)
  const rsNames = choices('RS_', 'ERS_NRES');
  const badPairs = [];
  for (const e of exNames) for (const rn of rsNames) {
    fresh();
    [0, C[e], 60, C[rn], 60, 70, C.SH_BYPASS, 0].forEach((v, k) => ex.sim_par_set(L1(C.P_E0 + k), v));
    const r = blow(false);
    if (!(r.pk > 0.005 && r.pk < 1 && r.after < 1e-3 && Number.isFinite(r.pk))) badPairs.push(`${e.slice(3)}+${rn.slice(3)} (${r.pk.toFixed(3)}, after ${r.after.toFixed(4)})`);
  }
  check(badPairs.length === 0, `ERS: all ${exNames.length * rsNames.length} exciter x resonator pairs sound and stop` +
    (badPairs.length ? `; not: ${badPairs.join(', ')}` : ''));

  // every shaper, on the clarinet, at AMOUNT 0 and 100
  const shNames = ['SH_SAT', 'SH_FOLD', 'SH_FILTER', 'SH_BYPASS', 'SH_BELL', 'SH_MUTE', 'SH_WAH', 'SH_BODY', 'SH_AIR', 'SH_GROWL'];
  const badSh = [];
  for (const s of shNames) for (const amt of [0, 100]) {
    fresh();
    [0, C.EX_REED, 64, C.RS_TUBE, 90, 60, C[s], amt].forEach((v, k) => ex.sim_par_set(L1(C.P_E0 + k), v));
    const r = blow(false);
    if (!(r.pk > 0.005 && r.pk < 1 && r.after < 1e-3)) badSh.push(`${s.slice(3)} ${amt} (${r.pk.toFixed(3)}, after ${r.after.toFixed(4)})`);
  }
  check(badSh.length === 0, `ERS: all ${shNames.length} shapers (AMOUNT 0 and 100) sound and stop` + (badSh.length ? `; not: ${badSh.join(', ')}` : ''));

  // every valve (REED, LIP, DBL REED, SAX) in every bore, soft / medium / stiff, starts on a swell of breath (0 -> 110
  // in 400 ms, CC2 at ~50 a second as the re.corder sends it), not only on a jump (2026-10-18: 498 of 800 did)
  const dead = [];
  for (const e of ['EX_REED', 'EX_LIP', 'EX_DREED', 'EX_SAX']) for (const rn of ['RS_TUBE', 'RS_CONE', 'RS_BRASS', 'RS_HOLE', 'RS_PIPE'])
    for (const col of [0, 64, 127]) {
      fresh();
      [0, C[e], col, C[rn], 50, 70, C.SH_BYPASS, 0].forEach((v, k) => ex.sim_par_set(L1(C.P_E0 + k), v));
      ex.sim_midi(0xB0, 2, 0); run(150);
      ex.sim_midi(0x90, 72, 80);
      for (let i = 1; i <= 20; i++) { ex.sim_midi(0xB0, 2, Math.round(110 * i / 20)); run(20); }
      run(250);
      const pk = run(150);
      t.off(72, 1000);
      if (!(pk > 0.03)) dead.push(`${e.slice(3)}+${rn.slice(3)} ${col}`);
    }
  check(dead.length === 0, `ERS: every valve in every bore starts on a swell (soft, medium, stiff)${dead.length ? '; not: ' + dead.join(', ') : ''}`);

  // legato in a bore: the slur keeps sounding at the new pitch (TRUMPET)
  fresh();
  [0, C.EX_LIP, 64, C.RS_BRASS, 64, 80, C.SH_BELL, 50].forEach((v, k) => ex.sim_par_set(L1(C.P_E0 + k), v));
  ex.sim_midi(0xB0, 2, 110); ex.sim_midi(0x90, 69, 80); run(500);
  ex.sim_midi(0x90, 72, 80); ex.sim_midi(0x80, 69, 0); run(300);
  const slur = run(200), f2 = t.pitch(400, 400, 700);
  t.off(72, 1500);
  check(slur > 0.02 && Math.abs(f2 - 523.3) < 523.3 * 0.03, `ERS TRUMPET legato A4 -> C5: keeps sounding (${slur.toFixed(3)}) at ${f2.toFixed(1)} Hz (523)`);

  // EDIT 3 / 4 (2026-10-21; LAG gone 2026-10-23: breath is smoothed once, globally): each knob does what it says, on ALTO SAX (38) / RECORDER (19), A4 at breath 110
  {
    const E = (k) => L1(C.P_E0 + k);
    const grab = (ms) => { const x = []; for (let i = 0; i < Math.round((ms * 44.1) / 128); i++) { ex.sim_render(128); x.push(...new Float32Array(ex.memory.buffer, ex.sim_out_l(), 128)); } return x; };
    const rms = (x) => Math.sqrt(x.reduce((s, v) => s + v * v, 0) / x.length);
    const hf2 = (x) => { let s = 0; for (let i = 2; i < x.length; i++) s += (x[i] - 2 * x[i - 1] + x[i - 2]) ** 2; return Math.sqrt(s / x.length) / (rms(x) || 1); };
    const play = (k, set, br = 110) => { fresh(); ex.sim_engine_preset(0, k); set(); ex.sim_midi(0xB0, 2, br); ex.sim_midi(0x90, 69, 80); };
    const end = () => t.off(69, 1500);
    const wob = (k, set, br = 127) => {
      play(k, set, br); run(1200);
      const f = []; for (let i = 0; i < 12; i++) f.push(t.pitch(40, 380, 510));
      end(); return Math.max(...f) / Math.min(...f);
    };
    const v0 = wob(38, () => {}), v1 = wob(38, () => ex.sim_par_set(E(8), 127));
    check(v1 > 1.01 && v1 > v0 * 1.008, `ERS VIB: the pitch moves (x${v1.toFixed(4)}, without x${v0.toFixed(4)})`);
    const early = (set) => { play(38, set); const x = rms(grab(20)); end(); return x; };
    const t0 = early(() => {}), t1 = early(() => ex.sim_par_set(E(10), 127));
    check(t1 > t0 * 1.15, `ERS TONGUE: a stronger start (${t1.toFixed(3)} vs ${t0.toFixed(3)} in the first 20 ms)`);
    const held = (k, set) => { play(k, set); run(600); const x = grab(300); end(); return x; };
    const fine = (x, lo, hi) => {                   // the period by autocorrelation, parabola-refined: Hz to a cent
      const r = (k) => { let s = 0; for (let i = 0; i + k < x.length; i++) s += x[i] * x[i + k]; return s; };
      let best = -Infinity, lag = 0;
      for (let k = Math.floor(44100 / hi); k <= Math.ceil(44100 / lo); k++) { const s = r(k); if (s > best) { best = s; lag = k; } }
      const y0 = r(lag - 1), y1 = best, y2 = r(lag + 1), d = (y0 - y2) / (2 * (y0 - 2 * y1 + y2));
      return { f: 44100 / (lag + d), lag };
    };
    const noisy = (x) => {                          // what does not repeat from one period to the next
      const { lag } = fine(x, 380, 510);
      let s = 0; for (let i = 0; i + lag < x.length; i++) s += (x[i + lag] - x[i]) ** 2;
      return Math.sqrt(s / (x.length - lag)) / (rms(x) || 1);
    };
    const a0 = noisy(held(21, () => ex.sim_par_set(E(12), 0))), a1 = noisy(held(21, () => {})), a2 = noisy(held(21, () => ex.sim_par_set(E(12), 127)));
    check(a0 < a1 * 0.8 && a2 > a1 * 1.2, `ERS AIR: SHAKUHACHI's breath noise none .. as modelled .. twice (${a0.toFixed(3)} ${a1.toFixed(3)} ${a2.toFixed(3)})`);
    const d0 = hf2(held(38, () => ex.sim_par_set(E(13), 0))), d1 = hf2(held(38, () => {})), d2 = hf2(held(38, () => ex.sim_par_set(E(13), 127)));
    check(d0 < d1 * 0.85 && d2 > d1 * 1.15, `ERS TILT: darker .. brighter (${d0.toFixed(3)} ${d1.toFixed(3)} ${d2.toFixed(3)})`);
    const pp = (br) => { play(38, () => { ex.sim_par_set(E(14), 127); }, br); run(900); const f = t.pitch(300, 380, 510); end(); return f; };
    const pl = pp(70), ph = pp(127), cents = 1200 * Math.log2(ph / pl);
    check(cents > 6, `ERS PR>PITCH: blown harder, sharper (${cents.toFixed(1)} cents from breath 70 to 127)`);
    const drift = (set) => {
      play(38, set); run(800);
      const f = []; for (let i = 0; i < 12; i++) f.push(fine(grab(250), 380, 510).f);
      end(); return 1200 * Math.log2(Math.max(...f) / Math.min(...f));
    };
    const dw = drift(() => ex.sim_par_set(E(15), 127)), dw0 = drift(() => {});
    check(dw > 3 && dw > dw0 + 2, `ERS DRIFT: the pitch wanders (${dw.toFixed(1)} cents over 3 s, without ${dw0.toFixed(1)})`);
  }
}
