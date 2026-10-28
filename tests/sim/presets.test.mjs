// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// Presets and the panel: the sound's global / own groups, FX's cycle and hold, SAVE, PRESETS, the questions,
// panic (hold PLAY), SAVE AS on the keys, the factory CLARINET (slot 3), VOLUME.
export default function (t) {
  const { ex, C, L1, LN, SP, SG, G, check, run } = t;
  const own = (g) => ex.sim_own() >> g & 1;

  // the factory DOUBLE REEDS (slot 3: core/factory.h, the user's own, 2026-10-29; each factory preset is checked value
  // for value against what it was made from by tools/factory_from_backup.mjs --check), loaded before the SAVE below
  // writes into slot 3: two ERS layers apart, the reverb and the volume its own
  ex.sim_preset(2);
  const pans = [0, 1].map((l) => ex.sim_par_get(LN(l, C.P_PAN))), engs = [0, 1].map((l) => ex.sim_par_get(LN(l, C.P_ENGINE)));
  check(ex.sim_preset_user(2) === 0 && t.name(2) === 'DOUBLE REEDS' && engs.every((e) => e === C.ENG_ID_ERS) &&
    pans.join() === '-18,18' && ex.sim_own() === ((1 << C.GR_REVERB) | (1 << C.GR_VOLUME)),
    `factory slot 3 is DOUBLE REEDS (ERS ${engs.join(' ')}, pans ${pans.join(' ')}, own ${ex.sim_own().toString(2)})`);

  // a sound group: the preset uses the global one until it is turned on a preset page; holding FX gives it back
  // (INIT: every factory preset owns some group)
  ex.sim_preset(255);
  check(ex.sim_preset_cur() === 95 && ex.sim_preset_edited() === 0 && ex.sim_own() === 0, 'INIT loads, not edited, uses the global sound');
  ex.sim_par_set(SG(C.S_DLY_MIX), 50);
  check(ex.sim_par_get(SP(C.S_DLY_MIX)) === 50, 'a global change reaches a preset that uses the global group');
  ex.sim_par_set(SP(C.S_REV_MIX), 90);
  check(own(C.GR_REVERB) === 1 && ex.sim_par_get(SG(C.S_REV_MIX)) === 24 && ex.sim_preset_edited() === 1,
    'turning REVERB MIX on the preset page makes the reverb its own (the global one stays 24), the preset is edited');
  ex.sim_par_set(SG(C.S_DLY_MIX), 0);
  ex.sim_page(C.PG_REVERB);
  t.hold(C.BTN_FX);
  check(own(C.GR_REVERB) === 0 && ex.sim_par_get(SP(C.S_REV_MIX)) === 24 && ex.sim_page_cur() === C.PG_REVERB,
    'holding FX on the reverb page: back to the global reverb, the page stays');
  const pages = [];
  for (let i = 0; i < 3; i++) { t.press(C.BTN_FX); pages.push(ex.sim_page_cur()); }
  check(pages.join() === [C.PG_REVERB2, C.PG_DRIVE, C.PG_DETUNE].join(), `FX again: REVERB -> REVERB + -> DRIVE -> DETUNE (pages ${pages.join()})`);

  // SAVE: the prompt, PRESETS picks the slot, SAVE again saves; loading it back brings the sound back
  ex.sim_par_set(L1(C.P_LEVEL), 77);
  ex.sim_par_set(SP(C.S_REV_MIX), 100);
  t.saveTo(2);
  check(ex.sim_preset_user(2) === 1 && ex.sim_preset_cur() === 2 && ex.sim_preset_edited() === 0, 'SAVE, PRESETS +1, SAVE: saved into slot 3');
  ex.sim_preset(0);
  ex.sim_preset(2);
  check(ex.sim_par_get(L1(C.P_LEVEL)) === 77 && ex.sim_par_get(SP(C.S_REV_MIX)) === 100 && own(C.GR_REVERB) === 1,
    'slot 3 loads back: the level, its own reverb');
  ex.sim_preset(1);
  check(ex.sim_preset_user(1) === 0 && ex.sim_par_get(L1(C.P_LEVEL)) !== 77, `slot 2 is still the factory ${t.name(1)}`);
  ex.sim_enc(C.ENC_PRESETS, 1); run(30);
  check(ex.sim_preset_cur() === 2 && ex.sim_par_get(L1(C.P_LEVEL)) === 77, 'PRESETS turns to slot 3: it loads');

  // holding REC: every note off (and no page change: the press acts only when let go before the hold). PLAY's hold
  // was the panic until 2026-10-24 (it solos now: buttons.test.mjs)
  t.note(64, 300);
  const pg = ex.sim_page_cur();
  ex.sim_button(C.BTN_REC, 1); run(700); ex.sim_button(C.BTN_REC, 0); run(5000);   // (slot 3's reverb tail)
  check(run(200) < 1e-3 && ex.sim_note() === -1 && ex.sim_page_cur() === pg && ex.sim_par_get(L1(C.P_MUTE)) === 0,
    'holding REC: all notes off, the page as it was, not muted');
  t.off(64, 300);

  // the questions: OCT+ is YES, OCT- is NO (and the OCT buttons do not transpose meanwhile)
  ex.sim_preset(1);
  ex.sim_par_set(L1(C.P_LEVEL), 66);
  t.press(C.BTN_SAVE); t.press(C.BTN_OCT_DOWN);
  check(ex.sim_preset_edited() === 1 && ex.sim_preset_user(1) === 0 && ex.sim_par_get(G(C.G_XPOSE)) === 0, 'SAVE, OCT-: not saved, not transposed');
  t.press(C.BTN_SAVE); ex.sim_enc(C.ENC_PRESETS, 3); run(30); t.press(C.BTN_OCT_UP);
  check(ex.sim_preset_user(4) === 1 && ex.sim_preset_cur() === 4 && ex.sim_par_get(G(C.G_XPOSE)) === 0,
    'SAVE, PRESETS +3, OCT+: saved into slot 5, not transposed');
  ex.sim_par_set(SG(C.S_REV_MIX), 99); ex.sim_par_set(G(C.G_XPOSE), 5); run(30);
  t.hold(C.BTN_GLO); t.press(C.BTN_OCT_DOWN);
  check(ex.sim_par_get(SG(C.S_REV_MIX)) === 99, 'hold GLO, OCT-: the globals stay');
  t.hold(C.BTN_GLO); t.press(C.BTN_OCT_UP);
  check(ex.sim_par_get(SG(C.S_REV_MIX)) === 24 && ex.sim_par_get(G(C.G_XPOSE)) === 0, 'hold GLO, OCT+: the globals back to factory');
  run(300);

  // SAVE AS (hold SAVE): the keys type, PLAY deletes, ALGORITHM turns a character, PRESETS the slot; SAVE does not
  // save, OCT- cancels, only OCT+ saves
  const type = (s) => { for (const ch of s) t.key(ch === ' ' ? 26 : ch.charCodeAt(0) - 65); };
  ex.sim_preset(2);
  t.hold(C.BTN_SAVE);
  for (let i = 0; i < 12; i++) t.press(C.BTN_PLAY);   // (the name was the slot's: gone)
  ex.sim_midi(0xB0, 2, 100); run(50);
  type('MY REED');
  const quiet = ex.sim_note() === -1;               // (typing plays no notes)
  ex.sim_midi(0xB0, 2, 0);
  t.press(C.BTN_PLAY);                              // MY REE
  ex.sim_enc(C.ENC_ALGORITHM, 28); run(30);         // a new character at the end: space +28 = '1'
  ex.sim_enc(C.ENC_PRESETS, 4); run(30);            // slot 7
  t.press(C.BTN_SAVE);
  const notYet = ex.sim_preset_user(6) === 0;
  t.press(C.BTN_OCT_UP);
  check(quiet && notYet && ex.sim_preset_user(6) === 1 && t.name(6) === 'MY REE1' && ex.sim_preset_cur() === 6,
    `SAVE AS: typed, deleted, turned, slot 7, only OCT+ saved it (${t.name(6)})`);
  t.hold(C.BTN_SAVE);
  type('XX');
  t.press(C.BTN_OCT_DOWN);
  check(t.name(6) === 'MY REE1' && ex.sim_par_get(G(C.G_XPOSE)) === 0, 'SAVE AS, OCT-: cancelled, nothing saved, not transposed');

  // VOLUME (MIX page, KNOB 4): the whole preset, its reverb too; the preset's own once turned; hold SEQ: global again
  ex.sim_preset(255);                               // (INIT: one sine; dry: the global reverb's sway would move the peaks)
  ex.sim_par_set(SG(C.S_REV_MIX), 0);
  const loud = () => { const p = (t.note(69, 300), run(300)); t.off(69, 1000); return p; };
  const full = loud();
  ex.sim_par_set(SP(C.S_VOLUME), 50);
  const half = loud(), owned = own(C.GR_VOLUME);
  t.press(C.BTN_SEQ);                               // the MIX page
  t.hold(C.BTN_SEQ);
  const back = ex.sim_par_get(SP(C.S_VOLUME)), after = own(C.GR_VOLUME);
  check(Math.abs(half / full - 0.5) < 0.03 && owned === 1 && back === 100 && after === 0,
    `VOLUME 50: half as loud (${full.toFixed(3)} -> ${half.toFixed(3)}), the preset's own; hold SEQ: global ${back} (own ${after})`);
}
