// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The firmware's numbers by name, for the tests and the tools: every enum in the sources below, read from the C
// (so a new parameter never means recounting by hand), and the parameter handles (core/params.c).
//   import { C, LN, L, SP, SG, G } from '../tools/consts.mjs';   C.S_REV_TYPE, C.PG_REVERB2, C.BTN_SEQ ...
//   node tools/consts.mjs [NAME ...]                             print them (all, or these)
// An enum value is its place, or the number it is given (= N); a name given twice fails. The sim's checks compare
// the counts (P_COUNT, S_COUNT, G_COUNT, NPAGES) with the firmware's own (sim_count), so a misread shows at once.
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const ROOT = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const SOURCES = ['app/app.c', 'firmware/src/core/layers.h', 'firmware/src/engines/engines.h',
  'firmware/src/ui/layout.h', 'firmware/src/core/reverb.c', 'firmware/src/core/sysex.c', 'firmware/src/core/tone.c',
  'firmware/src/engines/felucca_engine.h', 'firmware/src/engines/ers/ers.h', 'firmware/src/engines/ers/eng_ers.c',
  'firmware/src/engines/eva/eng_eva.c'];

function read() {
  const out = {};
  for (const f of SOURCES) {
    const src = fs.readFileSync(path.join(ROOT, f), 'utf8').replace(/\/\*[\s\S]*?\*\//g, ' ').replace(/\/\/.*$/gm, ' ');
    for (const m of src.matchAll(/\benum\s*(?:\w+\s*)?\{([^}]*)\}/g)) {
      let next = 0;
      for (const item of m[1].split(',').map((s) => s.trim()).filter(Boolean)) {
        const e = item.match(/^([A-Za-z_]\w*)\s*(?:=\s*(.+))?$/);
        if (!e) throw new Error(`consts: ${f}: cannot read "${item}"`);
        let v = next;
        if (e[2] !== undefined) {
          const t = e[2].trim();
          v = /^-?(0x[0-9a-f]+|\d+)u?$/i.test(t) ? parseInt(t, /^-?0x/i.test(t) ? 16 : 10) : out[t];
          if (v === undefined) throw new Error(`consts: ${f}: ${e[1]} = ${t}: not a number or a known name`);
        }
        if (e[1] in out && out[e[1]] !== v) throw new Error(`consts: ${e[1]} twice (${out[e[1]]}, ${v})`);
        out[e[1]] = v;
        next = v + 1;
      }
    }
  }
  return Object.freeze(out);
}

export const C = read();
// the parameter handles, as core/params.c makes them
export const LN = (n, i) => (n << 8) | i;              // layer n's parameter i (P_*)
export const L = (i) => (0x0F << 8) | i;               // the current layer's (PR_SEL)
export const SP = (i) => 0x4000 | i;                   // the sound's i as the preset plays it (S_*)
export const SG = (i) => 0xC000 | i;                   // its global copy
export const G = (i) => 0x8000 | i;                    // a global (G_*)

if (process.argv[1] && fileURLToPath(import.meta.url) === path.resolve(process.argv[1])) {
  const want = process.argv.slice(2);
  for (const [k, v] of Object.entries(C)) if (!want.length || want.includes(k)) console.log(`${k} ${v}`);
}
