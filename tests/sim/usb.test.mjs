// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// GLOBAL > USB AUDIO (app/usbaudio.c): what typhoon asks of the platform (fm1-base plat_usb_audio_set, patches/fm1-base/
// 0001; the host keeps it: sim_usb_audio). OFF by default, and so at power-on (before USB starts); levels, MUTE and the
// source at once; a change to or from OFF once the knob has rested 0.8 s (it re-presents the FM-1 on USB).
export default function (t) {
  const { ex, C, G, check, run } = t;
  const OFF = 0, MUTE = 1, ON = 2, MIX = 0, APP = 1;                 // (base/include/plat.h USB_AUDIO_*, USB_REC_*)
  const asked = () => ({ rec: ex.sim_usb_audio(0), recLv: ex.sim_usb_audio(1), play: ex.sim_usb_audio(2),
    playLv: ex.sim_usb_audio(3), from: ex.sim_usb_audio(4), calls: ex.sim_usb_audio(5) });
  const set = (g, v) => ex.sim_par_set(G(g), v);

  let a = asked();
  check(ex.sim_par_get(G(C.G_USB_IN)) === 0 && ex.sim_par_get(G(C.G_USB_OUT)) === 0 && a.calls >= 1 && a.rec === OFF && a.play === OFF,
    `USB AUDIO: OFF both ways by default, so at power-on (asked ${a.calls}x: rec ${a.rec}, play ${a.play})`);

  set(C.G_USB_OUT, 2); set(C.G_USB_OUT_LV, 150); run(100);
  const early = asked(); run(900); a = asked();
  check(early.rec === OFF && early.recLv === 6144 && a.rec === ON && a.recLv === 6144 && a.from === MIX,
    `USB OUT -> MIX at 150 %: the level at once (${early.recLv}), presented once the knob rests (${early.rec} -> ${a.rec}, from ${a.from})`);

  set(C.G_USB_OUT, 3); run(50); a = asked();
  check(a.rec === ON && a.from === APP, `USB OUT -> SYNTH: typhoon alone, at once (from ${a.from})`);
  set(C.G_USB_OUT, 1); run(50); a = asked();
  check(a.rec === MUTE, `USB OUT -> MUTE: at once, still presented (${a.rec})`);

  set(C.G_USB_OUT, 0); run(100); const waiting = asked(); run(900); a = asked();
  check(waiting.rec === MUTE && a.rec === OFF, `USB OUT -> OFF: silent while the knob may still move, then not presented (${waiting.rec} -> ${a.rec})`);

  set(C.G_USB_IN, 2); set(C.G_USB_IN_LV, 0); run(1000); a = asked();
  check(a.play === ON && a.playLv === 0 && a.rec === OFF, `USB IN -> ON at 0 %: presented, silent by its level (play ${a.play}, ${a.playLv})`);
  set(C.G_USB_IN_LV, 200); run(50); a = asked();
  check(a.playLv === 8192, `IN LEVEL 200 %: 8192 (Q12) at once (${a.playLv})`);

  ex.sim_page(C.PG_USB); run(100);
  check(ex.sim_page_cur() === C.PG_USB, `the USB AUDIO page (${C.PG_USB}) opens`);
}
