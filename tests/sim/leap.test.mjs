// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// LEAPS HOLD (core/leap.c): a slurred note-on at least OVER semitones above the note sounding
// waits up to WAIT ms. As the re.corder sends them: the new note-on, then the old note-off. Watched a millisecond at a
// time through the note that sounds (sim_note: -1 = none). The sounding note moves once an audio buffer (256 frames,
// 5.8 ms, as on the device): every change is seen up to B ms after it happens.
export default function (t) {
  const { ex, C, G, check } = t;
  let acc = 0;
  const run = (ms) => { for (let i = 0; i < ms; i++) { acc += 44.1; const n = Math.floor(acc); acc -= n; ex.sim_render(n); } };   // (exact ms: lib's run renders 128-frame blocks)
  const on = (n) => ex.sim_midi(0x90, n, 80), off = (n) => ex.sim_midi(0x80, n, 64);
  const watch = (ms) => { const s = []; for (let i = 0; i < ms; i++) { run(1); s.push(ex.sim_note()); } return s; };
  const B = 7;
  const fresh = () => { ex.sim_midi(0xB0, 123, 0); run(30); ex.sim_preset(255); ex.sim_midi(0xB0, 2, 100); run(20); };
  check(ex.sim_par_get(G(C.G_LEAP_MODE)) === 1 && ex.sim_par_get(G(C.G_LEAP_ST)) === 13 && ex.sim_par_get(G(C.G_LEAP_MS)) === 30,
    'LEAPS: HOLD, over 13 semitones, 30 ms by default');

  // a squeak from a recorded take: D6 -> A7 (11 ms) -> A5. A7 never sounds, and the slur goes straight on: no gap
  fresh(); on(86); run(50);
  on(105); off(86);
  const a = watch(11);
  on(81); off(105);
  const b = watch(40);
  check(!a.includes(105) && !b.includes(105) && a.every((n) => n === 86) && b.slice(B).every((n) => n === 81) && !a.concat(b).includes(-1),
    `LEAPS: D6 -> A7 (11 ms) -> A5: A7 never sounds, D6 slurs to A5 with no gap (${[...new Set(a.concat(b))].join(' ')})`);

  // a real leap (nothing follows it): it plays WAIT late, the old note sounding until then
  on(100); off(81);
  const c = watch(45);
  const at = c.indexOf(100);
  check(c.slice(0, 25).every((n) => n === 81) && at >= 28 && at <= 30 + B && c.slice(at).every((n) => n === 100),
    `LEAPS: a real leap A5 -> E7 (+19) plays after the wait (at ${at} ms), A5 sounding until then`);

  // what is never held: a downward jump, an octave up, a step
  on(76); off(100); const d = watch(B);
  on(88); off(76); const e = watch(B);
  on(90); off(88); const f = watch(B);
  check(d[B - 1] === 76 && e[B - 1] === 88 && f[B - 1] === 90, `LEAPS: a jump down (-24), an octave up (+12), a step (+2): at once, within a buffer (${d[B - 1]} ${e[B - 1]} ${f[B - 1]})`);

  // a passing jump, then the player stops: it never sounds, the old note ends
  on(109); off(90); run(5); off(109);
  const g = watch(20);
  check(!g.includes(109) && g[g.length - 1] === -1, `LEAPS: a passing jump, then nothing: it never sounds, the note ends (${g[g.length - 1]})`);

  // two big jumps in a row (a passing note, then a real leap): the first never sounds, the second waits its own time
  fresh(); on(74); run(30);
  on(93); off(74); run(10);
  on(95); off(93);
  const h = watch(45);
  const at2 = h.indexOf(95);
  check(!h.includes(93) && h.slice(0, 25).every((n) => n === 74) && at2 >= 28 && at2 <= 30 + B,
    `LEAPS: D5 -> A6 (passing) -> B6 (a real leap): A6 never sounds, B6 after its own wait (at ${at2} ms)`);

  // OFF: everything at once, as before
  ex.sim_par_set(G(C.G_LEAP_MODE), 0);
  fresh(); on(86); run(30);
  on(105); off(86);
  const k = watch(B);
  check(k[B - 1] === 105, `LEAPS OFF: D6 -> A7 at once, within a buffer (${k[B - 1]})`);
  ex.sim_par_set(G(C.G_LEAP_MODE), 1);
  ex.sim_midi(0xB0, 123, 0); run(30);
}
