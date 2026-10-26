// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// PLATE (core/plate.c) against the reverbs' contract (core/reverb.c): its tail
// dies to 0, stereo; DECAY's RT60 as the formula has it (0.2 s x 2^(6 DECAY / 127): half HALL's, as the old PLATE); MIX 0 leaves the dry
// bit for bit; its wet level near HALL's; full scale does not overflow; a SIZE sweep does not click; the largest SIZE
// and PRE-DLY fit the memory. Each take on a fresh firmware (the same take twice is the same sound: the wet is one take
// minus the same take at MIX 0). The input: INIT with NOISE on its layer (a burst, broadband) or its sine.
import { boot } from './lib.mjs';

export default function (t) {
  const { C, LN, SP, SG, check } = t;
  const KEYS = ['S_REV_SIZE', 'S_REV_DECAY', 'S_REV_DAMP', 'S_REV_PRE', 'S_REV_MIX'];
  const fresh = (type, s, noise = true) => {        // a firmware: INIT, the reverb so (s: SIZE/DECAY/DAMP/PRE/MIX)
    const f = boot(undefined, true), { ex } = f;
    ex.sim_preset(255);
    if (noise) f.engine(C.ENG_ID_NOISE);
    for (const g of [SP, SG]) {
      ex.sim_par_set(g(C.S_DLY_MIX), 0);
      ex.sim_par_set(g(C.S_REV_TYPE), type);
      KEYS.forEach((k, i) => ex.sim_par_set(g(C[k]), s[i]));
    }
    return f;
  };
  const take = (f, ms, L = [], R = []) => {         // ms of both sides, appended
    for (let i = 0; i < Math.round(ms * 44.1 / 128); i++) {
      f.ex.sim_render(128);
      L.push(...new Float32Array(f.ex.memory.buffer, f.ex.sim_out_l(), 128));
      R.push(...new Float32Array(f.ex.memory.buffer, f.ex.sim_out_r(), 128));
    }
    return { L, R };
  };
  const burst = (f, ms = 200, breath = 100) => {    // a note of ms, let go; the take from its start
    f.ex.sim_midi(0xB0, 2, breath); f.ex.sim_midi(0x90, 69, 100);
    const s = take(f, ms);
    f.ex.sim_midi(0x80, 69, 0); f.ex.sim_midi(0xB0, 2, 0);
    return s;
  };
  const rms = (x, a = 0, b = x.length) => {
    let e = 0;
    for (let i = a; i < b; i++) e += x[i] * x[i];
    return Math.sqrt(e / Math.max(1, b - a));
  };
  const dB = (v) => 20 * Math.log10(v + 1e-20);
  // (PLATE takes half the formula's time, as the old PLATE did: core/reverb.c rv_e_decay)
  const rtFormula = (decay) => 0.2 * 2 ** (6 * decay / 127);
  // RT60 from the energy decay curve (Schroeder's backward integral of both sides) from 150 ms after the burst ends:
  // the slope from -5 to -25 dB, x 3
  const rt60 = ({ L, R }, from) => {
    const e = [];
    for (let i = from; i < L.length; i++) e.push(L[i] * L[i] + R[i] * R[i]);
    let acc = 0;
    const edc = new Float64Array(e.length);
    for (let i = e.length - 1; i >= 0; i--) { acc += e[i]; edc[i] = acc; }
    let n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (let i = 0; i < e.length; i += 64) {
      const y = 10 * Math.log10(edc[i] / edc[0] + 1e-30), x = i / 44100;
      if (y <= -5 && y >= -25) { n++; sx += x; sy += y; sxx += x * x; sxy += x * y; }
    }
    const slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
    return n > 4 && slope < 0 ? -60 / slope : NaN;
  };

  // DECAY: the tail's RT60 as the formula has it (within 15 %), at a few DECAYs, DAMP 0 (the damping takes the highs
  // sooner) and others, the smallest SIZE and the largest; and the tail dies to exactly 0, both sides apart
  for (const [size, decay, damp] of [[90, 0, 0], [90, 40, 0], [90, 70, 0], [90, 100, 0], [90, 127, 0], [90, 40, 50],
    [90, 70, 50], [30, 40, 90], [0, 70, 50], [127, 70, 50]]) {
    const want = rtFormula(decay), f = fresh(C.RT_PLATE, [size, decay, damp, 0, 64]);
    const s = burst(f), end = s.L.length;
    take(f, want * 1300 + 500, s.L, s.R);
    // (the shortest: the diffusers ring ~0.3 .. 0.4 s by themselves, as the old PLATE's 0.29 s at DECAY 0: the floor)
    const rt = rt60(s, end + 6615), ok = want < 0.35 ? rt < 0.42 : Math.abs(rt / want - 1) < 0.15;
    check(ok, `PLATE SIZE ${size} DECAY ${decay} DAMP ${damp}: RT60 ${rt.toFixed(2)} s, the formula ${want.toFixed(2)} s ` +
      `(${((rt / want - 1) * 100).toFixed(0)} %)`);
  }
  {
    const f = fresh(C.RT_PLATE, [90, 70, 50, 20, 64]), s = burst(f), end = s.L.length;
    take(f, 1000, s.L, s.R);
    let sl = 0, sr = 0, slr = 0;
    for (let i = end + 4410; i < s.L.length; i++) { sl += s.L[i] ** 2; sr += s.R[i] ** 2; slr += s.L[i] * s.R[i]; }
    const corr = slr / Math.sqrt(sl * sr || 1), tail = take(f, 40000), last = take(f, 500);
    check(Math.abs(corr) < 0.5 && sl > 0 && sr > 0 && last.L.every((v) => v === 0) && last.R.every((v) => v === 0),
      `PLATE: stereo (sides alike ${corr.toFixed(2)}), and 40 s on it is exactly 0 both sides (left rms after 20 s ` +
      `${rms(tail.L, 20 * 44100).toExponential(1)})`);
  }

  // MIX 0: the dry bit for bit, the same as a firmware that never had a reverb on (one played PLATE loud first)
  {
    const a = fresh(C.RT_PLATE, [90, 70, 50, 20, 0]), b = fresh(C.RT_PLATE, [127, 127, 0, 0, 127]);
    burst(b, 300); take(b, 200);
    for (const g of [SP, SG]) b.ex.sim_par_set(g(C.S_REV_MIX), 0);
    take(a, 500); take(b, 500);
    const x = burst(a), y = burst(b);
    take(a, 300, x.L, x.R); take(b, 300, y.L, y.R);
    check(x.L.every((v, i) => v === y.L[i]) && x.R.every((v, i) => v === y.R[i]) && rms(x.L) > 0.01,
      'PLATE MIX 0: the dry bit for bit (after a loud tail)');
  }

  // the wet level: a noise burst at the default settings, MIX 64, the wet's rms over 1.5 s against HALL's
  const wet = (type, s, ms = 200, breath = 100, noise = true) => {
    const a = fresh(type, s, noise), d = fresh(type, [s[0], s[1], s[2], s[3], 0], noise);
    const x = burst(a, ms, breath), y = burst(d, ms, breath);
    take(a, 1300, x.L, x.R); take(d, 1300, y.L, y.R);
    return { L: x.L.map((v, i) => v - y.L[i]), R: x.R.map((v, i) => v - y.R[i]),
      peak: x.L.concat(x.R).reduce((m, v) => Math.max(m, Math.abs(v)), 0) };
  };
  {
    const p = wet(C.RT_PLATE, [90, 70, 50, 20, 64]), h = wet(C.RT_HALL, [90, 70, 50, 20, 64]);
    const lp = dB(Math.hypot(rms(p.L), rms(p.R))), lh = dB(Math.hypot(rms(h.L), rms(h.R)));
    // (within 4 dB: with half HALL's RT60, as the old PLATE, its tail carries less: ~3 dB under HALL here; against the
    // old PLATE, on a sine phrase: within ~1.5 dB)
    check(Math.abs(lp - lh) < 4,
      `PLATE's wet level near HALL's: ${lp.toFixed(1)} dB against ${lh.toFixed(1)} dB (noise, MIX 64)`);
  }

  // full scale: the loudest input (NOISE at full breath, the largest SIZE and DECAY, MIX 127) does not overflow:
  // the wet twice as loud as from half the input at most (a wrap would be far louder) and the output bounded
  {
    const s = [127, 127, 0, 0, 127], full = wet(C.RT_PLATE, s, 600, 127), half = wet(C.RT_PLATE, s, 600, 64);
    const r = rms(full.L) / rms(half.L);
    check(r > 0.5 && r < 2.5 && full.peak < 4 && full.L.every(Number.isFinite),
      `PLATE at full scale: no overflow (wet ${dB(rms(full.L)).toFixed(1)} dB, from half breath ` +
      `${dB(rms(half.L)).toFixed(1)} dB, peak ${full.peak.toFixed(2)})`);
  }

  // a SIZE sweep does not click: a sine held, SIZE 0 -> 127 -> 0 in steps of 8 every 30 ms; the wet's largest 2nd
  // difference against while SIZE holds (HALL's sweep for scale)
  const sweep = (type) => {
    const a = fresh(type, [64, 70, 50, 0, 127], false), d = fresh(type, [64, 70, 50, 0, 0], false);
    const w = [];
    const both = (ms) => { const x = take(a, ms), y = take(d, ms); w.push(...x.L.map((v, i) => v - y.L[i])); };
    for (const f of [a, d]) { f.ex.sim_midi(0xB0, 2, 100); f.ex.sim_midi(0x90, 69, 100); }
    both(1500);
    const n0 = w.length;
    both(500);
    const d2 = (x, i0, i1) => {
      let m = 0;
      for (let i = Math.max(2, i0); i < i1; i++) m = Math.max(m, Math.abs(x[i] - 2 * x[i - 1] + x[i - 2]));
      return m;
    };
    const still = d2(w, n0, w.length), n1 = w.length;
    const steps = [...Array(17).keys()];
    for (const v of steps.map((k) => Math.min(127, k * 8)).concat(steps.map((k) => Math.max(0, 127 - k * 8)))) {
      for (const g of [SP, SG]) a.ex.sim_par_set(g(C.S_REV_SIZE), v);
      both(30);
    }
    return d2(w, n1, w.length) / still;
  };
  {
    const p = sweep(C.RT_PLATE), h = sweep(C.RT_HALL);
    check(p < Math.max(2, h * 1.5),
      `PLATE: a SIZE sweep does not click (2nd difference ${p.toFixed(2)}x the still one's; HALL ${h.toFixed(2)}x)`);
  }

  // the memory: the largest SIZE with the longest PRE-DLY fits RV_MEM (else it stays silent), and the tail comes
  // the 200 ms later
  {
    const w = wet(C.RT_PLATE, [127, 70, 50, 200, 127], 100);
    const at = (ms) => rms(w.L, Math.round(ms * 44.1), Math.round((ms + 40) * 44.1));
    check(at(500) > 1e-3 && at(120) < 1e-5, `PLATE at SIZE 127, PRE-DLY 200 ms: it fits and sounds (the wet's rms ` +
      `${dB(at(120)).toFixed(0)} dB within the pre-delay, ${dB(at(500)).toFixed(0)} dB once it is past)`);
  }
}
