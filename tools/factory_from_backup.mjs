// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The factory presets from the user's own: a web editor backup (tools/presets.py's format) in, firmware/src/core/
// factory.h out, the FACTORY[] table core/presets.c plays. Each preset as factory_apply builds it: INIT, then per
// layer its engine with the engine preset nearest it (ENG), then every value that differs from that (PAR), then the
// sound groups it owns (SND: every value of an owned group that is not its default, at least one to own it). A slot
// the backup does not have takes the build's own factory preset in that slot (the sim plays it, it is read back).
// Layers with no engine are left out (OFF plays nothing). Then it checks itself: tools/factory_from_backup.mjs
// --check (after a build of the sim with the new table) loads every factory preset and compares it, value for value,
// with what it was made from (build/factory_src.json, written by the first step).
//   node tools/factory_from_backup.mjs BACKUP.json SLOTS > firmware/src/core/factory.h   (SLOTS: 1-26, or 1,3,5-9)
//   sh web/sim/build.sh build/sim && node tools/factory_from_backup.mjs --check
// (In the sim container, from the tree's top: it boots build/sim/typhoon.wasm.)
import fs from 'fs';
import { boot } from '../tests/sim/lib.mjs';

const t = boot(process.env.WASM, true), { ex, LN, SP } = t;   /* (WASM: another build's sim, its factory presets) */
const C = { ...t.C, FACTORY_OPS_MAX: 48 };         /* (core/presets.c FACTORY_OPS) */
const EIDX = {};                                   /* an engine's id -> its place in this build's ENGINES[] */
for (let e = 0; e < ex.sim_nengines(); e++) { ex.sim_preset(255); ex.sim_par_set(LN(0, C.P_ENGINE), e); EIDX[ex.sim_par_get(LN(0, C.P_ENGINE))] = e; }
const NL = 3, NE = C.P_COUNT - C.P_E0, SRC = 'build/factory_src.json';
const names = (pre, max) => { const m = {}; for (const [k, v] of Object.entries(C)) if (k.startsWith(pre) && v < max && !(v in m)) m[v] = k; return m; };
const PN = names('P_', C.P_COUNT), SN = names('S_', C.S_COUNT), EN = {};
for (const [k, v] of Object.entries(C)) if (k.startsWith('ENG_ID_')) EN[v] = k;
const GROUP = []; for (let i = 0; i < C.S_COUNT; i++) GROUP.push(null);
const name = (k) => { const p = ex.sim_preset_name(k), m = new Uint8Array(ex.memory.buffer, p, 13); return String.fromCharCode(...m.slice(0, m.indexOf(0))); };

// what plays now (layers, the sound as the preset plays it, its own groups)
const state = () => ({
  p: [...Array(NL)].map((_, l) => [...Array(C.P_COUNT)].map((__, i) => ex.sim_par_get(LN(l, i)))),
  s: [...Array(C.S_COUNT)].map((_, i) => ex.sim_par_get(SP(i))),
  own: ex.sim_own(),
});

if (process.argv[2] === '--check') {
  const want = JSON.parse(fs.readFileSync(SRC, 'utf8'));
  let bad = 0;
  for (const [k, w] of want.entries()) {
    ex.sim_preset(k); t.run(20);
    const got = state(), diff = [];
    if (name(k) !== w.name) diff.push(`name ${name(k)}`);
    for (let l = 0; l < NL; l++) {
      if (w.p[l][C.P_ENGINE] === C.ENG_ID_OFF) { if (got.p[l][C.P_ENGINE] !== C.ENG_ID_OFF) diff.push(`layer ${l + 1} not OFF`); continue; }
      for (let i = 0; i < C.P_COUNT; i++) if (got.p[l][i] !== w.p[l][i]) diff.push(`L${l + 1} ${PN[i] ?? i} ${got.p[l][i]} (want ${w.p[l][i]})`);
    }
    for (let i = 0; i < C.S_COUNT; i++) if (got.s[i] !== w.s[i]) diff.push(`${SN[i] ?? i} ${got.s[i]} (want ${w.s[i]})`);
    if (got.own !== w.own) diff.push(`own ${got.own.toString(2)} (want ${w.own.toString(2)})`);
    console.log(`${diff.length ? 'FAIL' : 'ok  '}  factory ${k + 1} ${w.name}${diff.length ? ': ' + diff.join(', ') : ''}`);
    bad += diff.length ? 1 : 0;
  }
  process.exit(bad ? 1 : 0);
}

const [file, spec] = process.argv.slice(2);
const backup = JSON.parse(fs.readFileSync(file, 'utf8'));
// a slot's number: the backup's (else the build's factory preset there); fN: the build's factory preset N, whatever
// the backup has (a dropped one taken back: WASM=an older build's sim)
const slots = spec.split(',').flatMap((r) => {
  if (r.startsWith('f')) return [{ f: Number(r.slice(1)) }];
  const [a, b] = r.split('-').map(Number); return [...Array((b ?? a) - a + 1)].map((_, i) => a + i);
});

// a record (core/presets.c ps_rec_t, version 4) as a state, the sound as it plays: an owned group's values, the
// others the global defaults (as a fresh FM-1's)
ex.sim_preset(255); t.run(10);
const sdef = [...Array(C.S_COUNT)].map((_, i) => ex.sim_par_get(t.SG(i)));
for (let i = 0; i < C.S_COUNT; i++) {             // each S_*'s group: owning one value owns it (sim_own's bit)
  ex.sim_preset(255); t.run(5);
  ex.sim_par_set(SP(i), sdef[i] === 0 ? 1 : sdef[i] - 1); t.run(5);
  GROUP[i] = Math.log2(ex.sim_own());
}
function decode(hex) {
  const b = Buffer.from(hex, 'hex'), ver = b[8], nl = b[9], np = b[10], ns = b[11], own = b[12];
  if (ver !== 4 || np !== C.P_COUNT || ns !== C.S_COUNT || nl !== NL) throw new Error(`a record version ${ver}, ${nl} x ${np}, ${ns}: not this build's`);
  const p = [...Array(NL)].map((_, l) => [...Array(C.P_COUNT)].map((__, i) => b.readInt16LE(28 + (l * 48 + i) * 2)));
  const s = [...Array(C.S_COUNT)].map((_, i) => ((own >> GROUP[i]) & 1 ? b.readInt16LE(28 + 4 * 48 * 2 + i * 2) : sdef[i]));
  const nm = b.subarray(16, 28); return { name: nm.subarray(0, nm.indexOf(0) < 0 ? 12 : nm.indexOf(0)).toString('latin1'), p, s, own };
}
const src = slots.map((k) => {
  if (typeof k === 'object') { ex.sim_preset(k.f - 1); t.run(20); return { ...state(), name: name(k.f - 1), from: `factory ${k.f}` }; }
  const r = backup.presets.find((q) => q.slot === k);
  if (r) return { ...decode(r.record), from: `backup slot ${k}` };
  ex.sim_preset(k - 1); t.run(20);
  return { ...state(), name: name(k - 1), from: `factory slot ${k}` };
});
fs.writeFileSync(SRC, JSON.stringify(src));

// one preset as ops
function ops(w) {
  const o = [];
  for (let l = 0; l < NL; l++) {
    const id = w.p[l][C.P_ENGINE];
    if (id === C.ENG_ID_OFF) continue;
    let best = null;
    for (let k = 0; ; k++) {                       // the engine preset nearest it, from INIT as factory_apply starts
      ex.sim_preset(255); ex.sim_par_set(LN(l, C.P_ENGINE), EIDX[id]);
      if (!ex.sim_engine_preset(l, k)) break;
      const base = [...Array(C.P_COUNT)].map((_, i) => ex.sim_par_get(LN(l, i)));
      const d = base.reduce((n, v, i) => n + (v !== w.p[l][i] ? 1 : 0), 0);
      if (!best || d < best.d) best = { k, d, base };
    }
    o.push(`ENG(${l}, ${EN[id]}, ${best.k})`);
    for (let i = 0; i < C.P_COUNT; i++) {
      if (i === C.P_ENGINE || best.base[i] === w.p[l][i]) continue;
      const v = i === C.P_TONE ? Object.keys(C).find((k) => k.startsWith('TN_') && C[k] === w.p[l][i]) ?? w.p[l][i] : w.p[l][i];
      o.push(`PAR(${l}, ${PN[i]}, ${v})`);   /* (TONE by name: TN_SWEEP ...) */
    }
  }
  for (let g = 0; g < 8; g++) {
    if (!((w.own >> g) & 1)) continue;
    const mine = [...Array(C.S_COUNT).keys()].filter((i) => GROUP[i] === g);
    const set = mine.filter((i) => w.s[i] !== sdef[i]);
    for (const i of set.length ? set : mine.slice(0, 1)) o.push(`SND(${SN[i]}, ${w.s[i]})`);
  }
  return o;
}
const rows = src.map((w, k) => ({ w, k, o: ops(w) }));
const most = Math.max(...rows.map((r) => r.o.length));
if (most >= C.FACTORY_OPS_MAX) throw new Error(`a preset of ${most} ops: core/presets.c FACTORY_OPS (${C.FACTORY_OPS_MAX}) has room for ${C.FACTORY_OPS_MAX - 1}`);
const out = [
  '/* SPDX-License-Identifier: MPL-2.0 */',
  '/* Copyright (C) 2026 ignis32 */',
  `/* The factory presets (core/presets.c FACTORY[]): written by tools/factory_from_backup.mjs from the user's own,`,
  ` * picked and sorted on the FM-1 (${backup.taken}, slots ${spec}); a slot the backup did not have kept the factory`,
  ' * preset it had. Each: INIT, then per layer its engine and that engine\'s nearest preset (ENG), the values that',
  ' * differ (PAR), the sound groups it owns (SND). Edit here by hand, or make it again from a new backup. */',
  `/* (the most ops a preset has here: ${most}; room for FACTORY_OPS - 1) */`,
  'static const factory_t FACTORY[] = {',
];
for (const { w, k, o } of rows) {
  const lines = []; let cur = `    {"${w.name}", {`;
  for (const [j, s] of o.entries()) {
    const piece = s + (j < o.length - 1 ? ', ' : '}},');
    if (cur.length + piece.length > 118) { lines.push(cur.trimEnd()); cur = ' '.repeat(6 + w.name.length) + piece; } else cur += piece;
  }
  lines.push(`${cur}${' '.repeat(Math.max(1, 116 - cur.length))}/* ${k + 1} */`);
  out.push(...lines);
}
out.push('};', '');
process.stdout.write(out.join('\n'));
console.error(`${rows.length} presets, at most ${most} ops; ${SRC} written for --check`);
