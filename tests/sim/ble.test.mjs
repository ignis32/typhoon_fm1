// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// Bluetooth (app/ble.c, ui/page_bt.c) on fm1-base's host stand-in: one device, "BLE MIDI", found half a second into a
// scan, connected 0.3 s after. BLE-P (advertising), BLE-C OFF / FIRST / CHOOSE, AUTO_RECONNECT; the list on KNOB 3
// (REFRESH, the devices, FORGET; OCT+ yes, OCT- shut). A change applies once the knob has rested 0.8 s.
export default function (t) {
  const { ex, C, G, check, run, press } = t;
  const OFF = 0, READY = 2, DEV_OFF = 0, DEV_READY = 4;              // (plat.h BLE_*, BLE_DEV_*)
  const str = (p) => { const m = new Uint8Array(ex.memory.buffer); let e = p; while (m[e]) e++; return new TextDecoder().decode(m.subarray(p, e)); };
  const word = (i) => str(ex.sim_ble_text(i));
  const knob3 = (n) => { ex.sim_enc(C.ENC_KNOB1 + 2, n); run(60); };
  ex.sim_page(C.PG_BT); run(1500);

  check(ex.sim_par_get(G(C.G_BLE_P)) === 1 && ex.sim_par_get(G(C.G_BLE_C)) === 2 && ex.sim_par_get(G(C.G_BLE_AUTO)) === 1 &&
    ex.sim_ble(0) === READY && ex.sim_ble(1) === DEV_OFF && /NONE CHOSEN/.test(word(1)) && /VISIBLE AS TYPHOON/.test(word(0)),
    `BLE defaults: BLE-P ON, BLE-C CHOOSE, AUTO_RECONNECT ON; nothing chosen, no scan (${word(0)} / ${word(1)})`);

  knob3(1); run(700);
  const listed = ex.sim_ble(4);
  knob3(1); press(C.BTN_OCT_UP); run(500);
  check(listed === 1 && ex.sim_ble(1) === DEV_READY && word(2) === 'BLE MIDI' && /CONNECTED BLE MIDI/.test(word(1)),
    `the list (KNOB 3): BLE MIDI picked with OCT+, connected and remembered (${word(1)}, remembered "${word(2)}")`);

  ex.sim_ble_set(0, 0); run(300);
  const gone = word(1); run(3500);
  check(/GONE, LOOKING AGAIN/.test(gone) && ex.sim_ble(1) === DEV_READY,
    `AUTO_RECONNECT: the device gone, found again (${gone} -> ${word(1)})`);

  ex.sim_par_set(G(C.G_BLE_P), 0); ex.sim_par_set(G(C.G_BLE_C), 0); run(1500);
  const radio = ex.sim_ble(0);
  ex.sim_par_set(G(C.G_BLE_P), 1); ex.sim_par_set(G(C.G_BLE_C), 2); run(4500);
  check(radio === OFF && ex.sim_ble(0) === READY && ex.sim_ble(1) === DEV_READY,
    `both OFF: the radio off (${radio}); back on: it finds the remembered one again, as after a power-on (${word(1)})`);

  knob3(1); run(700); knob3(5); press(C.BTN_OCT_UP); run(200);     // (the last entry: FORGET)
  const forgot = word(2);
  ex.sim_par_set(G(C.G_BLE_C), 1); run(2500);
  check(forgot === '' && ex.sim_ble(1) === DEV_READY && /CONNECTED BLE MIDI/.test(word(1)),
    `FORGET in the list, then BLE-C FIRST: the first device found (${word(1)})`);

  ex.sim_par_set(G(C.G_BLE_AUTO), 0); run(100);
  ex.sim_ble_set(0, 0); run(4000);
  check(ex.sim_ble(1) === DEV_OFF && /GONE/.test(word(1)), `AUTO_RECONNECT OFF: a device gone stays gone (${word(1)})`);

  ex.sim_ble_set(1, 1); run(100);
  const comp = word(0);
  ex.sim_par_set(G(C.G_BLE_P), 0); run(1200);
  check(/CONNECTED/.test(comp) && ex.sim_ble(2) === 0 && ex.sim_ble(3) === 0 && word(0) === 'OFF' && ex.sim_ble(0) === READY,
    `BLE-P OFF: advertising off, the computer let go, the radio still up for BLE-C (${comp} -> ${word(0)})`);
}
