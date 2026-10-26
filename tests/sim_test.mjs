// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The browser simulator's checks: node tests/sim_test.mjs [FILTER] (after web/sim/build.sh; SIM=path for another
// wasm). Runs every tests/sim/*.test.mjs (or those whose name has FILTER), each on a fresh firmware (tests/sim/lib.mjs),
// so no file depends on what another left behind. Exit 1 on a failure.
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { boot, failures } from './sim/lib.mjs';

const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), 'sim');
const filter = process.argv[2] || '';
const t0 = performance.now();
let ms = 0;
for (const f of fs.readdirSync(dir).filter((f) => f.endsWith('.test.mjs') && f.includes(filter)).sort()) {
  console.log(`-- ${f.replace('.test.mjs', '')}`);
  const t = boot();
  await (await import(path.join(dir, f))).default(t);   // (a check may be async: webeditor's)
  ms += t.ex.sim_ms();
}
const wall = performance.now() - t0;
console.log(`      ${ms} ms simulated in ${wall.toFixed(0)} ms (${(ms / wall).toFixed(0)}x real time)`);
process.exit(failures() ? 1 : 0);
