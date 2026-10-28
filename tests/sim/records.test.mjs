// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// Preset records across format versions (core/presets.c ps_nepar, ps_val): version 4 holds room for 48 values a
// layer (the commons, QUIRKS among them, then NEPAR = 20 engine values); versions 1 .. 3 room for 40, version 1 with 19
// commons and 16 engine values (until 2026-10-28). A preset saved now, rewritten in the version 1 layout, written to
// another slot and loaded, plays every value as it was: the commons, P_E0 .. P_E15, the sound's, by its own counts.
import * as TSX from '../../web/presets/tsx.mjs';
import { Link, Editor } from '../../web/editor/core.mjs';

const P_AT = 28, NL = 4, NP4 = 48, NP3 = 40, NS = 48;   // (ps_rec_t: the layers' values from byte 28, then the sound's)
const NC1 = 19;                                        // (version 1's commons: before QUIRKS, QUIRK SET)

export default async function (t) {
  const { ex, C, check } = t;
  const port = {
    send(msg) {
      const m = Uint8Array.from(msg);
      new Uint8Array(ex.memory.buffer, ex.sim_sx_in_buf(), m.length - 2).set(m.subarray(1, -1));
      ex.sim_sx_in(m.length - 2);
      t.wait(3);
      const n = ex.sim_sx_out_n();
      if (!n) return;
      const reply = new Uint8Array(ex.memory.buffer, ex.sim_sx_out_buf(), n).slice();
      setTimeout(() => link.receive(reply), 0);
    },
  };
  const link = new Link(port, { timeout: 100, busyWait: 50 });
  const ed = new Editor(link);
  await ed.connect();

  ex.sim_preset(2);                                  // CLARINET: two ERS layers, engine knobs well away from defaults
  const values = () => { const v = []; for (let l = 0; l < 3; l++) for (let i = 0; i < C.P_COUNT; i++) v.push(ex.sim_par_get(t.LN(l, i))); return v; };
  const sound = () => { const v = []; for (let i = 0; i < C.S_COUNT; i++) v.push(ex.sim_par_get(t.SP(i))); return v; };
  const before = values(), sbefore = sound();
  await ed.save(59, 'REC V4');
  const v4 = await ed.read(59);
  check(v4[8] === 4 && v4[10] === C.P_COUNT && C.P_COUNT === C.P_E0 + 20 && C.P_E0 === NC1 + 2,
    `records: saved as version ${v4[8]}, ${v4[10]} values a layer (${C.P_E0} commons, 20 engine knobs)`);

  // the version 1 layout: room for 40 a layer; the old 19 commons, then P_E0 .. P_E15; the sound's after the layers
  const get = (b, o) => (b[o] | (b[o + 1] << 8)) << 16 >> 16, put = (b, o, v) => { b[o] = v & 255; b[o + 1] = (v >> 8) & 255; };
  const v1 = v4.slice();
  for (let o = P_AT; o < P_AT + NL * NP4 * 2 + NS * 2; o++) v1[o] = 0;
  for (let l = 0; l < v4[9]; l++) {
    for (let i = 0; i < NC1; i++) put(v1, P_AT + (l * NP3 + i) * 2, get(v4, P_AT + (l * NP4 + i) * 2));
    for (let k = 0; k < 16; k++) put(v1, P_AT + (l * NP3 + NC1 + k) * 2, get(v4, P_AT + (l * NP4 + C.P_E0 + k) * 2));
  }
  for (let i = 0; i < NS; i++) put(v1, P_AT + NL * NP3 * 2 + i * 2, get(v4, P_AT + NL * NP4 * 2 + i * 2));
  v1[8] = 1; v1[10] = NC1 + 16;
  const crc = TSX.crc32(v1.slice(8));
  v1[4] = crc & 255; v1[5] = (crc >>> 8) & 255; v1[6] = (crc >>> 16) & 255; v1[7] = (crc >>> 24) & 255;
  await ed.write(60, v1);
  ex.sim_preset(0);
  ex.sim_preset(60);
  const after = values(), safter = sound(), back = await ed.read(60);
  const diff = [];
  for (let l = 0; l < 3; l++)
    for (const i of [...Array(NC1).keys(), ...Array.from({ length: 16 }, (_, k) => C.P_E0 + k)])
      if (before[l * C.P_COUNT + i] !== after[l * C.P_COUNT + i]) diff.push(`L${l + 1}:${i} ${before[l * C.P_COUNT + i]}->${after[l * C.P_COUNT + i]}`);
  const sdiff = sbefore.filter((v, i) => v !== safter[i]).length;
  check(back[8] === 1 && diff.length === 0 && sdiff === 0,
    `records: a version 1 record (40 a layer, 16 engine knobs) loads as it was saved: every common, P_E0 .. P_E15, the sound${diff.length || sdiff ? ` (${diff.slice(0, 6).join(', ')}; sound ${sdiff} differ)` : ''}`);
  // what it had no value for: QUIRKS off; EDIT 5 the engine's defaults (ERS: its breath LP at the mix's old one, 60 20
  // +40; VIB SWELL 64, its old fixed swell)
  const e5 = [0, 1].map((l) => [16, 17, 18, 19].map((k) => ex.sim_par_get(t.LN(l, C.P_E0 + k))).join(' '));
  check(e5.every((s) => s === '60 20 40 64') && ex.sim_par_get(t.LN(0, C.P_QUIRKS)) === 0,
    `records: a version 1 record's ERS layers get EDIT 5 as they played (${e5.join(' / ')}), QUIRKS off`);
  link.close();
}
