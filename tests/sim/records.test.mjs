// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// Preset records across format versions (core/presets.c ps_nepar): a version 2 record holds NEPAR (20) engine values a
// layer, a version 1 record (until 2026-10-28) 16. A preset saved now, rewritten in the version 1 layout, written to
// another slot and loaded, plays every value as it was: the commons, P_E0 .. P_E15, by its own counts.
import * as TSX from '../../web/presets/tsx.mjs';
import { Link, Editor } from '../../web/editor/core.mjs';

const P_AT = 28, PS_NP = 40;                         // (ps_rec_t: the layers' values from byte 28, 40 a layer)

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
  const before = values();
  await ed.save(59, 'REC V2');
  const v2 = await ed.read(59);
  check(v2[8] === 2 && v2[10] === C.P_COUNT && C.P_COUNT === C.P_E0 + 20,
    `records: saved as version ${v2[8]}, ${v2[10]} values a layer (${C.P_E0} commons, 20 engine knobs)`);

  // the version 1 layout: the commons, then P_E0 .. P_E15 only; np = P_E0 + 16
  const v1 = v2.slice(), np1 = C.P_E0 + 16;
  const at = (l, i) => P_AT + (l * PS_NP + i) * 2;
  for (let l = 0; l < v2[9]; l++)
    for (let i = 0; i < PS_NP; i++) {
      const val = i < np1 ? (v2[at(l, i)] | (v2[at(l, i) + 1] << 8)) : 0;
      v1[at(l, i)] = val & 255; v1[at(l, i) + 1] = (val >> 8) & 255;
    }
  v1[8] = 1; v1[10] = np1;
  const crc = TSX.crc32(v1.slice(8));
  v1[4] = crc & 255; v1[5] = (crc >>> 8) & 255; v1[6] = (crc >>> 16) & 255; v1[7] = (crc >>> 24) & 255;
  await ed.write(60, v1);
  ex.sim_preset(0);
  ex.sim_preset(60);
  const after = values(), back = await ed.read(60);
  const diff = [];
  for (let l = 0; l < 3; l++)
    for (let i = 0; i < C.P_E0 + 16; i++)
      if (before[l * C.P_COUNT + i] !== after[l * C.P_COUNT + i]) diff.push(`L${l + 1}:${i} ${before[l * C.P_COUNT + i]}->${after[l * C.P_COUNT + i]}`);
  check(back[8] === 1 && diff.length === 0,
    `records: a version 1 record (16 engine knobs) loads as it was saved, every common and P_E0 .. P_E15${diff.length ? ` (${diff.slice(0, 6).join(', ')})` : ''}`);
  link.close();
}
