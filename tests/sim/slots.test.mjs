// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The 96 slots in the store (core/presets.c ps_sec): slots 1 .. 64 in sectors 0 .. 7 and their copy in 8 .. 15, as
// always; sector 16 the BT stack's; slots 65 .. 96 in 17 .. 20 and their copy in 21 .. 24. A save into slot 70 lands in
// its sector and its copy's, and touches nothing else (sector 16, slots 1 .. 64); with its main record spoilt it
// loads from the copy; slot 96 too (the last sector). Read straight from the sim's flash (sim_store: fm1-base's
// simulated data area at the store).
export default function (t) {
  const { ex, C, L1, check, run } = t;
  const SEC = 4096, SLOT = 512;   // (sim_store: the store itself, at 0x97000 of fm1-base's simulated data area)
  const flash = () => new Uint8Array(ex.memory.buffer, ex.sim_store(), 25 * SEC);
  const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
  const MAGIC = 0x31505254;
  const at = (k, copy) => (k < 64 ? (k >> 3) * SEC + (copy ? 8 * SEC : 0) : (17 + ((k - 64) >> 3)) * SEC + (copy ? 4 * SEC : 0)) + (k & 7) * SLOT;
  const sum = (b, a, n) => { let s = 0; for (let i = a; i < a + n; i++) s = (s * 31 + b[i]) >>> 0; return s; };
  const save = (k, level) => {                      // INIT, a level to tell it by, SAVE into slot k
    ex.sim_preset(255);
    ex.sim_par_set(L1(C.P_LEVEL), level);
    t.saveTo(k);
    run(50);
  };

  const before = flash().slice(), lo = sum(before, 0, 16 * SEC), bt = sum(before, 16 * SEC, SEC);
  save(69, 71);
  let f = flash();
  const main = u32(f, at(69, false)) === MAGIC, copy = u32(f, at(69, true)) === MAGIC;
  check(main && copy && ex.sim_preset_user(69) === 1 && sum(f, 0, 16 * SEC) === lo && sum(f, 16 * SEC, SEC) === bt,
    `slot 70: its record in sector 17 and its copy in 21 (main ${main}, copy ${copy}); slots 1 .. 64 and sector 16 untouched`);
  ex.sim_preset(0);
  ex.sim_preset(69);
  check(ex.sim_par_get(L1(C.P_LEVEL)) === 71, 'slot 70 loads back');
  f[at(69, false) + 40] ^= 0xFF;                    // (its main record spoilt: the CRC fails)
  ex.sim_preset(0);
  ex.sim_preset(69);
  check(ex.sim_par_get(L1(C.P_LEVEL)) === 71, 'slot 70, the main record spoilt: loads from the copy');
  save(95, 33);
  f = flash();
  check(u32(f, at(95, false)) === MAGIC && u32(f, at(95, true)) === MAGIC && sum(f, 16 * SEC, SEC) === bt,
    'slot 96: the last sector (20) and its copy (24); sector 16 untouched');
  ex.sim_preset(0);
  ex.sim_preset(95);
  check(ex.sim_par_get(L1(C.P_LEVEL)) === 33, 'slot 96 loads back');
}
