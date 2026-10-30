// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// Preset records (core/presets.c ps_rec_t): version 5 (2026-10-30, the sources) holds 3 layers of room for 48 values
// (the commons, then NEPAR = 20 engine values), the routes, the sound's. A preset saved, written back to another slot
// and loaded plays every value as it was. Older versions are not read (no user presets to keep then): written over
// SysEx they are refused, and a slot holding one plays its factory preset.
import * as TSX from '../../web/presets/tsx.mjs';
import { Link, Editor } from '../../web/editor/core.mjs';

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

  ex.sim_preset(2);                                  // two ERS layers, engine knobs well away from defaults
  const values = () => { const v = []; for (let l = 0; l < 3; l++) for (let i = 0; i < C.P_COUNT; i++) v.push(ex.sim_par_get(t.LN(l, i))); return v; };
  const sound = () => { const v = []; for (let i = 0; i < C.S_COUNT; i++) v.push(ex.sim_par_get(t.SP(i))); return v; };
  const before = values(), sbefore = sound();
  await ed.save(59, 'REC V5');
  const v5 = await ed.read(59);
  check(v5[8] === 5 && v5[9] === 3 && v5[10] === C.P_COUNT && v5[11] === C.S_COUNT,
    `records: saved as version ${v5[8]}, ${v5[9]} layers of ${v5[10]} values (${C.P_E0} commons, 20 engine knobs), ${v5[11]} sound values`);
  await ed.write(60, v5);
  ex.sim_preset(0);
  ex.sim_preset(60);
  const after = values(), safter = sound();
  const diff = before.map((v, i) => (v !== after[i] ? `L${Math.floor(i / C.P_COUNT) + 1}:${i % C.P_COUNT} ${v}->${after[i]}` : '')).filter(Boolean);
  const sdiff = sbefore.filter((v, i) => v !== safter[i]).length;
  check(diff.length === 0 && sdiff === 0,
    `records: written to another slot, it loads as it was saved${diff.length || sdiff ? ` (${diff.slice(0, 6).join(', ')}; sound ${sdiff} differ)` : ''}`);
  // a version 4 record (before the sources): refused
  const v4 = v5.slice();
  v4[8] = 4;
  const crc = TSX.crc32(v4.slice(8));
  v4[4] = crc & 255; v4[5] = (crc >>> 8) & 255; v4[6] = (crc >>> 16) & 255; v4[7] = (crc >>> 24) & 255;
  let refused = false;
  try { await ed.write(61, v4); } catch (e) { refused = /BAD/.test(e.message); }
  check(refused, 'records: a version 4 record (before 2026-10-30) is refused');
  link.close();
}
