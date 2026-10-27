// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The page buttons' cycles and PLAY / REC (ui/layout.h BUTTONS, since 2026-10-24): GLO goes through the global settings
// and the global breath curve (beside BREATH IN), REC through the global effects (the copies a preset without its own
// group plays); PLAY mutes, its hold solos; REC's hold is the panic.
export default function (t) {
  const { ex, C, LN, check, press, hold, run } = t;
  const cycle = (b, n) => { const s = []; for (let i = 0; i < n; i++) { press(b); s.push(ex.sim_page_cur()); } return s; };
  const name = (pg) => Object.keys(C).find((k) => k.startsWith('PG_') && C[k] === pg) || pg;

  const glo = cycle(C.BTN_GLO, 9);
  const wantGlo = [C.PG_GLOBAL, C.PG_BT, C.PG_TILT, C.PG_G_BREATH, C.PG_G_CURVE, C.PG_LEAP, C.PG_USB, C.PG_USBH, C.PG_GLOBAL];
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

  // OCT+ / OCT-: the transpose's splash, the value in big digits over the page (up orange, down blue), gone 1.2 s
  // after the last press
  const lit = (c) => {                            // pixels of colour c where the digits go (rows 84 .. 211; RGB565)
    const fb = new Uint16Array(ex.memory.buffer, ex.sim_fb(), 240 * 240);
    let n = 0;
    for (let i = 84 * 240; i < 212 * 240; i++) if (fb[i] === c) n++;
    return n;
  };
  const UP = ((255 >> 3) << 11) | ((150 >> 2) << 5) | (40 >> 3), DOWN = ((70 >> 3) << 11) | ((170 >> 2) << 5) | (255 >> 3);
  const x0 = ex.sim_par_get(t.G(C.G_XPOSE));
  t.press(C.BTN_OCT_UP); run(100);
  const on = lit(UP), xp = ex.sim_par_get(t.G(C.G_XPOSE));
  run(1400);
  const gone = lit(UP);
  t.press(C.BTN_OCT_DOWN); t.press(C.BTN_OCT_DOWN); run(100);
  const down = lit(DOWN), xd = ex.sim_par_get(t.G(C.G_XPOSE));
  check(xp === x0 + 1 && on > 1500 && gone < 100 && xd === x0 - 1 && down > 1500,
    `OCT+ / OCT-: TRANSPOSE big on screen, ${xp} orange (${on} px), ${xd} blue (${down} px); the page back after 1.2 s (${gone})`);
  t.press(C.BTN_OCT_UP); run(1400);
}
