// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The editor over SysEx (core/sysex.c LAYOUT .. STATE): a computer reads and turns every parameter as the knobs do.
import * as TSX from '../../web/presets/tsx.mjs';

export default function (t) {
  const { ex, C, LN, SP, SG, G, check, run } = t;
  const { req } = t.sysex(TSX);
  const ok = (r) => r && r.status === 'OK';
  const get = (...hs) => { const r = req(TSX.CMD.GET, hs.flatMap(TSX.u16)); return hs.map((_, k) => TSX.s16(r.data, k * 2)); };
  const set = (...pairs) => req(TSX.CMD.SET, pairs.flatMap(([h, v]) => [...TSX.u16(h), ...TSX.i16(v)]));
  const desc = (h) => { const r = req(TSX.CMD.DESCRIBE, TSX.u16(h)); return ok(r) ? TSX.describe(r.data) : null; };
  const text = (h, v) => TSX.nameOf(req(TSX.CMD.TEXT, [...TSX.u16(h), ...TSX.i16(v)]).data);
  const state = () => req(TSX.CMD.STATE).data;
  ex.sim_preset(255);

  const lay = req(TSX.CMD.LAYOUT).data;
  check(lay.slice(0, 7).join() === [3, C.P_COUNT, C.S_COUNT, C.G_COUNT, C.NGROUP, ex.sim_nengines(), C.P_E0].join(),
    `editor LAYOUT: 3 layers, ${lay[1]} P, ${lay[2]} S, ${lay[3]} G, ${lay[4]} groups, ${lay[5]} engines, engine knobs from ${lay[6]}`);

  // every parameter describes itself sensibly
  const handles = [];
  for (let l = 0; l < 3; l++) for (let i = 0; i < C.P_COUNT; i++) handles.push(LN(l, i));
  for (let i = 0; i < C.S_COUNT; i++) handles.push(SP(i), SG(i));
  for (let i = 0; i < C.G_COUNT; i++) handles.push(G(i));
  const bad = handles.filter((h) => { const d = desc(h); return !d || !d.label || d.min > d.max || d.def < d.min || d.def > d.max; });
  check(bad.length === 0, `editor DESCRIBE: all ${handles.length} parameters (label, range, default)${bad.length ? ' bad: ' + bad.map((h) => h.toString(16)).join(' ') : ''}`);
  const lv = desc(LN(0, C.P_LEVEL)), rt = desc(SP(C.S_REV_TYPE));
  check(lv.label === 'LEVEL' && lv.min === 0 && lv.max === 127 && lv.def === 100 && lv.group === null &&
    rt.label === 'TYPE' && rt.names === 2 && rt.group === C.GR_REVERB,
    `editor DESCRIBE: LEVEL 0..127 (100), REVERB TYPE: 2 names (PLATE HALL: OLD dropped 2026-10-23), group ${rt.group}`);
  check(req(TSX.CMD.DESCRIBE, TSX.u16(0x3000)).status === 'BAD' && req(TSX.CMD.GET, TSX.u16(LN(5, 0))).status === 'BAD',
    'editor: a handle that is no parameter: BAD');
  check(text(SP(C.S_REV_TYPE), C.RT_HALL) === 'HALL' && text(LN(0, C.P_ENGINE), C.ENG_ID_ERS) === 'ERS' && text(LN(0, C.P_LEVEL), 127) === '100%',
    'editor TEXT: any value as the screen shows it (HALL, ERS, 100%)');

  // the engines, and a layer's engine set by its id: its knobs follow
  const en = req(TSX.CMD.ENGINES, [0]).data;
  const names = [];
  for (let k = 0; k < Math.min(6, en[0]); k++) names.push(`${en[1 + k * 9]}:${TSX.nameOf(en.slice(2 + k * 9, 10 + k * 9))}`);
  check(en[0] === ex.sim_nengines() && names[0] === `${C.ENG_ID_SINE}:SINE`, `editor ENGINES: ${en[0]} (${names.join(' ')} ...)`);
  if (t.engine(C.ENG_ID_ERS) >= 0) {
    ex.sim_preset(255);
    set([LN(1, C.P_ENGINE), C.ENG_ID_ERS]);
    const e = get(LN(1, C.P_ENGINE))[0], k1 = desc(LN(1, C.P_E0 + 1)).label;
    set([LN(1, C.P_ENGINE), C.ENG_ID_OFF]);
    check(e === C.ENG_ID_ERS && k1 === 'EXCITER' && get(LN(1, C.P_ENGINE))[0] === C.ENG_ID_OFF,
      `editor SET a layer's engine by id: ERS (its knob 2: ${k1}), then OFF`);
  }

  // SET as the knobs do: clamped, the group becomes the preset's own; GROUP gives it back; STATE moves
  ex.sim_preset(1);
  const s0 = state();
  const r = set([LN(0, C.P_LEVEL), 500], [SP(C.S_REV_MIX), 80]);
  const s1 = state();
  check(ok(r) && TSX.s16(r.data, 0) === 127 && TSX.s16(r.data, 2) === 80 && (s1[6] >> C.GR_REVERB & 1) === 1 && s1[5] === 1 &&
    s1.slice(0, 4).join() !== s0.slice(0, 4).join(), 'editor SET: LEVEL 500 -> 127, REVERB MIX makes the reverb its own, edited, STATE moved');
  const g = req(TSX.CMD.GROUP, [C.GR_REVERB, 0]);
  check(ok(g) && (g.data[0] >> C.GR_REVERB & 1) === 0 && get(SP(C.S_REV_MIX))[0] === get(SG(C.S_REV_MIX))[0],
    'editor GROUP 0: the reverb back to the global one');
  const before = state().slice(0, 4).join();
  ex.sim_enc(C.ENC_KNOB1, 3); run(30);              // a knob on the device: the editor sees it in STATE
  check(state().slice(0, 4).join() !== before, 'editor STATE: moves when a knob is turned on the FM-1');

  // SAVE and LOAD
  set([LN(0, C.P_LEVEL), 33]);
  t.note(60, 100);
  const busy = req(TSX.CMD.SAVE, [9, ...TSX.nameBytes('EDITED')]).status;
  t.off(60, 6000);
  const saved = req(TSX.CMD.SAVE, [9, ...TSX.nameBytes('EDITED')]).status;
  check(busy === 'BUSY' && saved === 'OK' && ex.sim_preset_user(9) === 1 && t.name(9) === 'EDITED' && state()[4] === 9 && state()[5] === 0,
    `editor SAVE: BUSY while a note sounds (${busy}), then slot 10 EDITED (${saved})`);
  const ld = req(TSX.CMD.LOAD, [1]);
  check(ok(ld) && ex.sim_preset_cur() === 1 && get(LN(0, C.P_LEVEL))[0] !== 33, 'editor LOAD: slot 2 plays');
  req(TSX.CMD.LOAD, [9]);
  check(get(LN(0, C.P_LEVEL))[0] === 33, 'editor LOAD: slot 10 back, as saved');
}
