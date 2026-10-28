// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The presets over USB SysEx (core/sysex.c, web/presets/tsx.mjs): INFO, LIST, READ, WRITE (a retry inside, BUSY while
// a note sounds, broken records refused), RENAME, DELETE, STATS, a bad sum.
import fs from 'fs';
import * as TSX from '../../web/presets/tsx.mjs';

export default function (t) {
  const { ex, C, L1, check, run } = t;
  ex.sim_preset(255);                                // (a user preset in slot 3 to read)
  ex.sim_par_set(L1(C.P_LEVEL), 77);
  t.saveTo(2);
  const { send, req, next } = t.sysex(TSX);
  const listName = (k) => TSX.nameOf(req(TSX.CMD.LIST, [k, 1]).data.slice(1));

  const info = req(TSX.CMD.INFO), version = fs.readFileSync('VERSION', 'utf8').trim();
  check(info && info.status === 'OK' && info.data[0] === TSX.VER && info.data[1] === 96 && (info.data[2] | info.data[3] << 8) === 512 &&
    TSX.nameOf(info.data.slice(7, 15)) === 'TYPHOON' && [version, 'DEV'].includes(TSX.nameOf(info.data.slice(15, 23))),   // (fm1-base: a dev build is DEV, a release its X.Y)
    `sysex INFO: protocol ${TSX.VER}, 96 slots of 512 B, firmware ${info && TSX.nameOf(info.data.slice(15, 23))}`);
  check(Object.entries(TSX.CMD).every(([k, v]) => C[`TSX_${k}`] === v),
    'sysex: the JS command numbers are the firmware\'s (web/presets/tsx.mjs = core/sysex.c)');
  const list = req(TSX.CMD.LIST, [0, 4]);
  const n0 = TSX.nameOf(list.data.slice(1, 13)), u2 = list.data[2 * 13];
  check(list.status === 'OK' && list.data.length === 52 && n0 === t.name(0) && n0 === 'SWEEP LEAD' && list.data[0] === 0 && u2 === 1,
    `sysex LIST 1..4: 1 SWEEP LEAD factory, 3 the user's (${n0}, ${list.data[0]}, ${u2})`);
  check(req(TSX.CMD.READ, [0, 0, 0, 64]).status === 'EMPTY', 'sysex READ of a factory slot: EMPTY');
  const rec = [];
  for (let at = 0; at < 512; at += 64) rec.push(...req(TSX.CMD.READ, [2, ...TSX.u16(at), 64]).data);
  check(rec.length === 512 && TSX.crc32(rec.slice(8)) === (rec[4] | rec[5] << 8 | rec[6] << 16 | rec[7] << 24) >>> 0,
    'sysex READ slot 3: 512 B, its CRC good');

  const factory62 = listName(61);                   // (every slot has a factory preset: DELETE gives it back)
  // write it into slot 62 as COPY
  const w = rec.slice();
  w.splice(16, 12, ...TSX.nameBytes('COPY'));
  w.splice(4, 4, ...TSX.u32(TSX.crc32(w.slice(8))));
  const write = (slot, r) => {
    const st = [req(TSX.CMD.WBEGIN, [slot]).status];
    for (let at = 0; at < 512; at += 64) {
      const f = TSX.frame(TSX.CMD.WDATA, next(), [...TSX.u16(at), ...r.slice(at, at + 64)]);
      st.push(send(f).status);
      if (at === 128) st.push(send(f).status);      // (a retry of the same request: answered, not done twice)
    }
    return st;
  };
  const st = write(61, w);
  t.note(60, 100);
  const busy = req(TSX.CMD.WCOMMIT, TSX.u32(TSX.crc32(w))).status;
  t.off(60, 6000);                                  // (the reverb's tail: writes wait for silence)
  const commit = req(TSX.CMD.WCOMMIT, TSX.u32(TSX.crc32(w))).status;
  check(st.every((s) => s === 'OK') && busy === 'BUSY' && commit === 'OK' && ex.sim_preset_user(61) === 1 && listName(61) === 'COPY',
    `sysex WRITE slot 62 (a retry inside): ${st.length} OK, BUSY while a note sounds (${busy}), then ${commit}`);
  const back = [];
  for (let at = 0; at < 512; at += 64) back.push(...req(TSX.CMD.READ, [61, ...TSX.u16(at), 64]).data);
  check(back.every((b, i) => b === w[i]), 'sysex READ slot 62: the record as written');
  w[100] ^= 1;                                      // (a record whose own CRC is wrong)
  write(61, w);
  const bad = req(TSX.CMD.WCOMMIT, TSX.u32(TSX.crc32(w))).status;
  check(bad === 'BAD' && ex.sim_preset_user(61) === 1, `sysex WRITE of a broken record: refused, slot 62 kept (${bad})`);
  write(61, rec);
  check(req(TSX.CMD.WCOMMIT, [1, 2, 3, 4]).status === 'CRC', 'sysex WRITE COMMIT with a wrong CRC: CRC');
  const ren = req(TSX.CMD.RENAME, [61, ...TSX.nameBytes('RENAMED')]).status;
  check(ren === 'OK' && listName(61) === 'RENAMED', `sysex RENAME slot 62 (${ren})`);
  const del = req(TSX.CMD.DELETE, [61]).status;
  check(del === 'OK' && ex.sim_preset_user(61) === 0 && listName(61) === factory62, `sysex DELETE slot 62: its factory preset again, ${factory62} (${del})`);
  const stats = req(TSX.CMD.STATS);
  const xr = stats.data[4] | stats.data[5] << 8;
  check(stats.status === 'OK' && stats.data.length === 8 && xr === 0,
    `sysex STATS answers: the load, the xruns (${xr}: none on the sim), the worst block (${(stats.data[6] | stats.data[7] << 8) / 10} %)`);
  const f = TSX.frame(TSX.CMD.INFO, next()); f[f.length - 2] ^= 1;
  check(send(f) === null, 'sysex: a bad sum gets no reply');
}
