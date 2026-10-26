// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The page buttons' cycles and PLAY / REC (ui/layout.h BUTTONS, since 2026-10-24): GLO goes through the global settings
// and the global breath curve (beside BREATH IN), REC through the global effects (the copies a preset without its own
// group plays); PLAY mutes, its hold solos; REC's hold is the panic.
export default function (t) {
  const { ex, C, LN, check, press, hold, run } = t;
  const cycle = (b, n) => { const s = []; for (let i = 0; i < n; i++) { press(b); s.push(ex.sim_page_cur()); } return s; };
  const name = (pg) => Object.keys(C).find((k) => k.startsWith('PG_') && C[k] === pg) || pg;

  const glo = cycle(C.BTN_GLO, 8);
  const wantGlo = [C.PG_GLOBAL, C.PG_BT, C.PG_TILT, C.PG_G_BREATH, C.PG_G_CURVE, C.PG_LEAP, C.PG_USB, C.PG_GLOBAL];
  check(glo.join() === wantGlo.join(), `GLO: the global settings and the breath curve, round (${glo.map(name).join(' ')})`);

  const rec = cycle(C.BTN_REC, 7);
  const wantRec = [C.PG_G_LP, C.PG_G_DRIVE, C.PG_G_DETUNE, C.PG_G_DELAY, C.PG_G_REVERB, C.PG_G_REVERB2, C.PG_G_LP];
  check(rec.join() === wantRec.join(), `REC: the global effects' pages, round (${rec.map(name).join(' ')})`);

  const muted = () => [0, 1, 2].map((l) => ex.sim_par_get(LN(l, C.P_MUTE)));
  ex.sim_preset(0); run(100);
  press(C.BTN_PLAY); const m1 = muted(); press(C.BTN_PLAY); const m0 = muted();
  hold(C.BTN_PLAY); const solo = muted(); hold(C.BTN_PLAY); const back = muted();
  check(m1[0] === 1 && m0[0] === 0 && solo.join() === '0,1,1' && back.join() === m0.join(),
    `PLAY: mutes the layer (${m1}), its hold solos it (${solo}) and back (${back})`);

  ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 69, 100); run(200);
  const sounding = ex.sim_note(); hold(C.BTN_REC); run(50);
  check(sounding === 69 && ex.sim_note() === -1, `REC's hold: panic, every note off (${sounding} -> ${ex.sim_note()})`);
}
