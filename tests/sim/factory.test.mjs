// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// Every factory preset (slots 1 .. NFACTORY, as INFO counts them): loads, speaks at C5 and C6 with breath, stays out
// of the soft clip at full breath, and is silent once its tails have died (no hum, no stuck note).
import * as TSX from '../../web/presets/tsx.mjs';

export default function (t) {
  const { ex, check, run } = t;
  const n = t.sysex(TSX).req(TSX.CMD.INFO).data[6];
  check(n === 28, `factory presets: ${n} (core/factory.h: the user's 26, CLARINET and TOY PANFLUTE, 2026-10-29)`);
  const bad = [], quiet = [];
  for (let k = 0; k < n; k++) {
    ex.sim_preset(k);
    const name = t.name(k);
    let worst = 1, top = 0;
    for (const note of [72, 84]) {
      ex.sim_midi(0xB0, 2, 110); ex.sim_midi(0x90, note, 80); run(500);
      const pk = run(400);
      worst = Math.min(worst, pk); top = Math.max(top, pk);
      t.off(note, 300);
    }
    run(12000);                                     // (the longest tails: HALL at DECAY 115)
    const after = run(300);
    if (!(worst > 0.02 && after < 1e-3)) bad.push(`${k + 1} ${name} (${worst.toFixed(3)} .. ${top.toFixed(3)}, after ${after.toExponential(1)})`);
    if (worst < 0.06) quiet.push(`${k + 1} ${name} ${worst.toFixed(3)}`);
    if (top > 0.98) bad.push(`${k + 1} ${name} clips (${top.toFixed(3)})`);
  }
  check(bad.length === 0, `factory presets: all ${n} speak at C5 and C6, silent after, unclipped at MASTER unity` +
    (bad.length ? '; not: ' + bad.join(', ') : ''));
  if (quiet.length) console.log(`      quiet (under 0.06 at C5 or C6): ${quiet.join(', ')}`);
}
