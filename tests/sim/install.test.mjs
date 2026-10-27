// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The installer's step before an install (web/editor/install.mjs, app/install_app.js): the ports asked in turn, the
// one typhoon answers on found, its user presets saved to a file as the editor's BACKUP makes it, or not (skip),
// or the install stopped (cancel); nothing asked of an FM-1 with none of yours, or of no typhoon at all.
import * as TSX from '../../web/presets/tsx.mjs';
import { Link, Editor } from '../../web/editor/core.mjs';
import { before } from '../../web/editor/install.mjs';

// a MIDI access as Web MIDI gives: the sim as "TYPHOON FM-1", and another device that never answers
function access(t, withSim = true) {
  const { ex } = t;
  const mk = (name) => {
    const input = { id: `${name}-in`, name, onmidimessage: null, open: async () => input };
    return { input, output: { id: `${name}-out`, name, open: async () => {}, send() {} } };
  };
  const other = mk('Some Keyboard'), sim = mk('TYPHOON FM-1');
  sim.output.send = (msg) => {
    const m = Uint8Array.from(msg);
    new Uint8Array(ex.memory.buffer, ex.sim_sx_in_buf(), m.length - 2).set(m.subarray(1, -1));
    ex.sim_sx_in(m.length - 2);
    t.wait(3);
    const n = ex.sim_sx_out_n();
    if (!n) return;
    const reply = new Uint8Array(ex.memory.buffer, ex.sim_sx_out_buf(), n).slice();
    setTimeout(() => sim.input.onmidimessage?.({ data: reply }), 0);
  };
  const ports = withSim ? [other, sim] : [other];
  return { inputs: new Map(ports.map((p) => [p.input.id, p.input])), outputs: new Map(ports.map((p) => [p.output.id, p.output])) };
}

const ui = (answer, asked = []) => ({
  say() {}, log() {}, progress() {},
  ask: async (title, body, choices) => { asked.push({ title, body, choices }); return answer; },
});

export default async function (t) {
  const { ex, check } = t;
  const midi = access(t);

  let asked = [];
  check(await before(midi, ui('save', asked), { save: () => check(false, 'install: nothing saved') }) === null && !asked.length,
    'install: an FM-1 with none of your presets: nothing asked');

  // two presets of yours (through the editor's own SAVE)
  const sim = midi.outputs.get('TYPHOON FM-1-out'), input = midi.inputs.get('TYPHOON FM-1-in');
  const link = new Link({ send: (b) => sim.send(b) }, { timeout: 100, busyWait: 50 });
  input.onmidimessage = (e) => link.receive(e.data);
  const ed = new Editor(link);
  await ed.connect();
  await ed.save(12, 'MINE A');
  await ed.save(40, 'MINE/B');
  link.close();
  input.onmidimessage = null;

  asked = [];
  let file = null, text = null;
  const r = await before(midi, ui('save', asked), { save: (x, f) => { text = x; file = f; } });
  const j = text && TSX.backupParse(text);
  check(asked.length === 1 && /<b>2<\/b>/.test(asked[0].body) && asked[0].body.includes('13 MINE A, 41 MINE/B'),
    'install: asked once, your 2 presets named');
  check(r && r.saved === 2 && /^typhoon-presets-\d{4}-\d\d-\d\d_\d{6}\.json$/.test(file) &&
    j.presets.map((p) => `${p.slot} ${p.name}`).join() === '13 MINE A,41 MINE/B',
    `install: SAVE: slots 13 and 41 in ${file}, as the editor's BACKUP writes them`);
  check(input.onmidimessage === null, 'install: the port left as it was found');

  let saved = 0;
  check(await before(midi, ui('skip'), { save: () => saved++ }) === null && !saved, 'install: SKIP: no file, the install goes on');
  check(await before(midi, ui('stop'), { save: () => saved++ }) === false && !saved, 'install: CANCEL: the install stopped');
  check(await before(access(t, false), ui('save')) === null, 'install: no typhoon on any port: nothing asked, the install goes on');
  check(ex.sim_preset_user(12) && ex.sim_preset_user(40), 'install: the presets still on the FM-1');
}
