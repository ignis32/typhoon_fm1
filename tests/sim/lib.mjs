// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The sim checks' common parts (tests/sim_test.mjs runs tests/sim/*.test.mjs): a fresh firmware for each file, the
// numbers by name (tools/consts.mjs), and ways to play it and listen to it.
//   export default function (t) { t.check(ok, 'what') ... }   t: see boot() below
import fs from 'fs';
import { WASI } from 'node:wasi';
import { C, LN, L, SP, SG, G } from '../../tools/consts.mjs';

let module;
let fails = 0;
export const failures = () => fails;

export function boot(wasm = process.env.SIM || 'build/sim/typhoon.wasm', quiet = false) {   // (quiet: no 'boots' line)
  module ??= new WebAssembly.Module(fs.readFileSync(wasm));
  const wasi = new WASI({ version: 'preview1', returnOnExit: true });   // (web/sim/typhoon_sim.c: wasm32-wasi, a reactor)
  const inst = new WebAssembly.Instance(module, { wasi_snapshot_preview1: wasi.wasiImport });
  wasi.initialize(inst);
  const ex = inst.exports;
  const frames = (ms) => Math.round(ms * 44.1 / 128);
  const side = (k) => new Float32Array(ex.memory.buffer, k ? ex.sim_out_r() : ex.sim_out_l(), 128);
  const t = {
    ex, C, LN, L, SP, SG, G,
    L1: (p) => LN(0, p), L2: (p) => LN(1, p),
    check(ok, what) { console.log(`${ok ? 'ok  ' : 'FAIL'}  ${what}`); if (!ok) fails++; },
    wait(ms) {                                      // ms of the device's own clock (sim_ms): the frames handed out may
      const t0 = ex.sim_ms();                       // come from a block rendered before, with no time passing
      do ex.sim_render(128); while (ex.sim_ms() < t0 + ms);
    },
    run(ms) {                                       // play ms; the peak of the left side
      let pk = 0;
      for (let i = 0; i < frames(ms); i++) { ex.sim_render(128); for (const v of side(0)) pk = Math.max(pk, Math.abs(v)); }
      return pk;
    },
    freq(ms) {                                      // upward zero crossings of the left side, as Hz
      let n = 0, prev = 0, f = 0;
      for (let i = 0; i < frames(ms); i++) { ex.sim_render(128); for (const v of side(0)) { if (prev < 0 && v >= 0) n++; prev = v; f++; } }
      return n * 44100 / f;
    },
    pitch(ms, lo, hi) {                             // the period by autocorrelation, searched in lo .. hi Hz, as Hz
      const buf = [];                               // (for waves with two upward crossings a period: a bowed string)
      for (let i = 0; i < frames(ms); i++) { ex.sim_render(128); buf.push(...side(0)); }
      let best = -Infinity, lag = 0;
      for (let k = Math.floor(44100 / hi); k <= Math.ceil(44100 / lo); k++) {
        let s = 0;
        for (let i = 0; i + k < buf.length; i++) s += buf[i] * buf[i + k];
        if (s > best) { best = s; lag = k; }
      }
      return 44100 / lag;
    },
    steady(ms, lo, hi) {                            // how steady a tone is: the normalized autocorrelation at the
      const buf = [];                               // best lag in lo .. hi Hz (1: a steady tone; a buzz or noise: low)
      for (let i = 0; i < frames(ms); i++) { ex.sim_render(128); buf.push(...side(0)); }
      let best = -1;
      for (let k = Math.floor(44100 / hi); k <= Math.ceil(44100 / lo); k++) {
        let s = 0, e1 = 0, e2 = 0;
        for (let i = 0; i + k < buf.length; i++) { s += buf[i] * buf[i + k]; e1 += buf[i] * buf[i]; e2 += buf[i + k] * buf[i + k]; }
        best = Math.max(best, s / Math.sqrt(e1 * e2 || 1));
      }
      return best;
    },
    stats(ms) {                                     // rms of each side and how alike they are (correlation)
      let sl = 0, sr = 0, slr = 0, n = 0;
      for (let i = 0; i < frames(ms); i++) {
        ex.sim_render(128);
        const l = side(0), r = side(1);
        for (let j = 0; j < 128; j++) { sl += l[j] * l[j]; sr += r[j] * r[j]; slr += l[j] * r[j]; n++; }
      }
      return { l: Math.sqrt(sl / n), r: Math.sqrt(sr / n), corr: slr / Math.sqrt(sl * sr || 1) };
    },
    sides(ms) {                                     // the largest left / right difference, the left's peak
      let d = 0, pk = 0;
      for (let i = 0; i < frames(ms); i++) {
        ex.sim_render(128);
        const l = side(0), r = side(1);
        for (let j = 0; j < 128; j++) { d = Math.max(d, Math.abs(l[j] - r[j])); pk = Math.max(pk, Math.abs(l[j])); }
      }
      return [d, pk];
    },
    press(b, ms = 30) { ex.sim_button(b, 1); t.run(ms); ex.sim_button(b, 0); t.run(30); },
    hold(b) { ex.sim_button(b, 1); t.run(700); ex.sim_button(b, 0); t.run(30); },
    key(k) { ex.sim_key(k, 1); t.run(20); ex.sim_key(k, 0); t.run(20); },
    note(n, ms, breath = 100) { ex.sim_midi(0xB0, 2, breath); ex.sim_midi(0x90, n, 80); return t.run(ms); },
    off(n, ms = 0) { ex.sim_midi(0x80, n, 0); ex.sim_midi(0xB0, 2, 0); return t.run(ms); },
    name(k) { const m = new Uint8Array(ex.memory.buffer, ex.sim_preset_name(k), 13); return String.fromCharCode(...m.slice(0, m.indexOf(0))); },
    engine(id) {                                    // an engine's place in this build's list (P_ENGINE takes it), -1: absent
      for (let e = 0; e < ex.sim_nengines(); e++) { ex.sim_par_set(LN(0, C.P_ENGINE), e); if (ex.sim_par_get(LN(0, C.P_ENGINE)) === id) return e; }
      return -1;
    },
    sysex(TSX) {                                    // the presets protocol (web/presets/tsx.mjs): {send(msg), req(cmd, data)}
      let seq = 0;
      const send = (msg) => {                       // the whole message F0 .. F7 in; the reply (parsed, or null) out
        new Uint8Array(ex.memory.buffer, ex.sim_sx_in_buf(), msg.length - 2).set(msg.subarray(1, -1));
        ex.sim_sx_in(msg.length - 2); t.wait(3);
        const n = ex.sim_sx_out_n();
        return n ? TSX.parse(new Uint8Array(ex.memory.buffer, ex.sim_sx_out_buf(), n).slice()) : null;
      };
      return { send, req: (cmd, data = []) => send(TSX.frame(cmd, ++seq, data)), next: () => ++seq };
    },
    saveTo(slot) {                                  // SAVE, PRESETS to slot (0-based), SAVE: what plays into it
      t.press(C.BTN_SAVE);
      ex.sim_enc(C.ENC_PRESETS, slot - ex.sim_preset_cur()); t.run(30);
      t.press(C.BTN_SAVE);
    },
  };
  if (quiet && ex.sim_init(44100) !== 0) throw new Error('sim_init failed');
  if (!quiet) t.check(ex.sim_init(44100) === 0, 'boots');
  // the tests (and the recorded takes) give breath on CC2: BREATH CC set so; the firmware's own default kept here
  t.breathCcDefault = ex.sim_par_get(G(C.G_BREATH_CC));
  ex.sim_par_set(G(C.G_BREATH_CC), 2);
  return t;
}
