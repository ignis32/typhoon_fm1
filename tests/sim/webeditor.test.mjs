// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The web preset editor's logic (web/editor/core.mjs, the page's Link and Editor) against the firmware: the same code
// the page runs in Chrome, with the sim in place of WebMIDI. Connect, describe, SET as a knob, engine changes, the
// groups, the slots, backup / restore (tools/presets.py's files), and what the FM-1's own knobs change.
import fs from 'fs';
import * as TSX from '../../web/presets/tsx.mjs';
import { Link, Editor } from '../../web/editor/core.mjs';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// the sim as a MIDI port: a frame in, the firmware runs a moment, its reply comes back (later, as from a port)
function simPort(t) {
  const { ex } = t;
  const port = {
    link: null, renderMs: 3, drop: 0, frames: 0,
    send(msg) {
      port.frames++;
      new Uint8Array(ex.memory.buffer, ex.sim_sx_in_buf(), msg.length - 2).set(msg.subarray(1, -1));
      ex.sim_sx_in(msg.length - 2);
      t.wait(port.renderMs);
      const n = ex.sim_sx_out_n();
      if (!n) return;
      const reply = new Uint8Array(ex.memory.buffer, ex.sim_sx_out_buf(), n).slice();
      if (port.drop > 0) { port.drop--; return; }   // (a lost reply)
      setTimeout(() => {
        port.link.receive(Uint8Array.of(0xF8));     // (MIDI clock and notes around it: ignored)
        port.link.receive(Uint8Array.of(0x90, 60, 100));
        port.link.receive(reply);
      }, 0);
    },
  };
  return port;
}

export default async function (t) {
  const { ex, C, LN, SP, SG, G, check } = t;
  const port = simPort(t);
  const link = new Link(port, { timeout: 100, busyWait: 50 });
  port.link = link;
  const ed = new Editor(link, { poll: 60, quiet: 100 });
  const events = { layer: [], busy: [], lost: 0 };
  ed.on('layer', (n) => events.layer.push(n)).on('busy', (b) => events.busy.push(b)).on('lost', () => events.lost++)
    .on('error', (e) => check(false, `webeditor: no error (${e.message})`));
  const dev = (h) => ex.sim_par_get(h);
  const devDesc = (h) => {                         // the firmware's own DESCRIBE of h, straight
    const { req } = t.sysex(TSX);
    const r = req(TSX.CMD.DESCRIBE, TSX.u16(h));
    return TSX.describe(r.data);
  };
  ex.sim_preset(1);

  // the page's tables are the firmware's
  check(TSX.FMT.every((n, i) => C[`F_${n}`] === i) && TSX.FMT.length === C.F_STEPS + 1 && TSX.P_ENGINE === C.P_ENGINE &&
    TSX.ENG_OFF === C.ENG_ID_OFF && TSX.GROUPS.length === C.NGROUP &&
    TSX.GROUPS.every((n, g) => n.endsWith(Object.keys(C).find((k) => k.startsWith('GR_') && C[k] === g).slice(3))),
    'webeditor: F_* formats, P_ENGINE, ENG_ID_OFF, the groups\' names: tsx.mjs = the C');

  // connect: everything from the device
  const t0 = performance.now(), f0 = port.frames;
  await ed.connect();
  const all = ed.allHandles();
  check(ed.info.name === 'TYPHOON' && ed.layout.layers === 3 && ed.layout.p === C.P_COUNT && ed.layout.s === C.S_COUNT &&
    ed.layout.g === C.G_COUNT && ed.engines.length === ex.sim_nengines() && ed.list.length === ed.info.slots,
    `webeditor connect: ${ed.engines.length} engines, ${all.length} values, ${ed.list.length} slots in ${port.frames - f0} requests (${(performance.now() - t0).toFixed(0)} ms)`);
  check(all.every((h) => ed.d(h) && ed.val.get(h) === dev(h)), 'webeditor connect: every parameter described, every value the device\'s');
  const rt = ed.d(SP(C.S_REV_TYPE)), lv = ed.d(LN(0, C.P_LEVEL)), eng = ed.d(LN(0, C.P_ENGINE));
  check([...rt.enum.values()].join() === 'PLATE,HALL' && rt.group === C.GR_REVERB && lv.label === 'LEVEL' &&
    eng.enum.get(C.ENG_ID_OFF) === 'OFF' && eng.enum.get(C.ENG_ID_FM) === 'FM' && ed.d(SG(C.S_REV_TYPE)) === rt,
    'webeditor DESCRIBE: REVERB TYPE\'s names by TEXT, the engines by ENGINES (and OFF), a global copy as its preset\'s');
  check(ed.list.every((s, k) => s.name === t.name(k) && s.user === !!ex.sim_preset_user(k)), 'webeditor LIST: the 96 slots, names and user flags');

  // SET as a knob: the latest value only, a few SETs for many moves, clamped
  let f = port.frames;
  for (let v = 0; v <= 127; v++) ed.set(LN(0, C.P_LEVEL), v);
  ed.set(SP(C.S_DLY_MIX), 999);
  await ed.flush();
  check(dev(LN(0, C.P_LEVEL)) === 127 && dev(SP(C.S_DLY_MIX)) === 127 && port.frames - f <= 3,
    `webeditor SET: 128 moves + 1 -> ${port.frames - f} request(s), LEVEL 127, DELAY MIX 999 clamped to 127`);
  f = port.frames;
  const tA = Date.now();
  for (let k = 0; k < 20; k++) { ed.set(LN(0, C.P_PAN), k); await sleep(5); }
  await ed.flush();
  const rate = (port.frames - f) / ((Date.now() - tA) / 1000);
  check(dev(LN(0, C.P_PAN)) === 19 && rate <= 40, `webeditor SET: a drag of 20 steps in ${Date.now() - tA} ms: ${port.frames - f} SETs (${rate.toFixed(0)}/s, at most ~30)`);
  ed.text(LN(0, C.P_LEVEL), 127);
  await sleep(30);
  check(ed.text(LN(0, C.P_LEVEL), 127) === '100%', `webeditor TEXT: LEVEL 127 shows ${ed.text(LN(0, C.P_LEVEL), 127)}`);

  // an engine change: the knobs are described again; ERS's labels follow its EXCITER / RESONATOR / SHAPER
  const knobsOk = (n) => ed.knobHandles(n).every((h) => { const a = ed.d(h), b = devDesc(h); return a.label === b.label && a.min === b.min && a.max === b.max && a.fmt === b.fmt; });
  if (t.engine(C.ENG_ID_ERS) >= 0) {
    ex.sim_preset(1);
    await ed.load(1);
    events.layer.length = 0;
    ed.set(LN(1, C.P_ENGINE), C.ENG_ID_ERS);
    await ed.flush();
    check(ed.d(LN(1, C.P_E0 + 1)).label === 'EXCITER' && events.layer.includes(1) && knobsOk(1) &&
      ed.knobHandles(1).every((h) => ed.val.get(h) === dev(h)),
      `webeditor engine -> ERS: layer 2's knobs described again (knob 2 ${ed.d(LN(1, C.P_E0 + 1)).label}), their values read`);
    const ex1 = ed.knobHandles(1).find((h) => ed.d(h).label === 'EXCITER'), d1 = ed.d(ex1);
    const before = ed.knobHandles(1).map((h) => ed.d(h).label).join();
    let moved = false;
    for (let v = d1.min; v <= d1.max; v++) {
      ed.set(ex1, v);
      await ed.flush();
      if (!knobsOk(1)) { moved = false; break; }
      if (ed.knobHandles(1).map((h) => ed.d(h).label).join() !== before) moved = true;
    }
    check(knobsOk(1), `webeditor ERS: after each EXCITER the labels are the device's${moved ? ' (they changed)' : ' (they stayed)'}: ${ed.knobHandles(1).map((h) => ed.d(h).label).join(' ')}`);
    ed.set(LN(1, C.P_ENGINE), C.ENG_ID_FM);
    await ed.flush();
    check(ed.d(LN(1, C.P_E0)).label === devDesc(LN(1, C.P_E0)).label && knobsOk(1), `webeditor engine -> FM: knob 1 ${ed.d(LN(1, C.P_E0)).label}`);
    // TONE (core/tone.c): its three knobs follow its type, labels and values (a new type sets its defaults)
    const tk = ed.toneKnobs(1), toneOk = () => tk.every((h) => ed.d(h).label === devDesc(h).label && ed.val.get(h) === dev(h));
    const seen = [];
    let allOk = ed.toneHandle(1) === LN(1, C.P_TONE) && tk.length === 3;
    for (let ty = 0; ty < C.TN_COUNT; ty++) {
      ed.set(LN(1, C.P_TONE), ty);
      await ed.flush();
      if (!toneOk()) allOk = false;
      seen.push(tk.map((h) => ed.d(h).label).join('/'));
    }
    ed.set(LN(1, C.P_TONE), C.TN_OFF);
    await ed.flush();
    check(allOk, `webeditor TONE: each type's knobs as the device has them (${seen.join(', ')})`);
    ed.set(LN(1, C.P_ENGINE), C.ENG_ID_OFF);
    await ed.flush();
    check(dev(LN(1, C.P_ENGINE)) === C.ENG_ID_OFF && knobsOk(1), 'webeditor engine -> OFF');
  }

  // groups: own / global, the global copy edited as such, a preset edit makes it own
  await ed.load(1);
  await ed.group(C.GR_REVERB, false);
  check(!ed.ownGroup(C.GR_REVERB) && !(ex.sim_own() >> C.GR_REVERB & 1), 'webeditor GROUP: reverb global');
  ed.set(SG(C.S_REV_MIX), 50);
  await ed.flush();
  await ed.readState();
  check(dev(SG(C.S_REV_MIX)) === 50 && dev(SP(C.S_REV_MIX)) === 50 && !ed.ownGroup(C.GR_REVERB),
    'webeditor: the global copy edited (0xC000), the preset still uses it');
  ed.set(SP(C.S_REV_MIX), 70);
  await ed.flush();
  await ed.readState();
  check(ed.ownGroup(C.GR_REVERB) && dev(SG(C.S_REV_MIX)) === 50 && dev(SP(C.S_REV_MIX)) === 70,
    'webeditor: a preset edit of a global group makes it the preset\'s own (as on the device)');
  await ed.group(C.GR_REVERB, false);
  check(!ed.ownGroup(C.GR_REVERB) && ed.val.get(SP(C.S_REV_MIX)) === 50, 'webeditor GROUP 0: back to the global one, its values read');
  await ed.group(C.GR_DELAY, true);
  check(ed.ownGroup(C.GR_DELAY), 'webeditor GROUP 1: delay own');

  // SAVE, LOAD, names
  ed.set(LN(0, C.P_LEVEL), 44);
  await ed.save(9, 'web test');
  check(t.name(9) === 'WEB TEST' && ex.sim_preset_user(9) && ed.list[9].user && ed.state.slot === 9 && !ed.state.edited,
    'webeditor SAVE: slot 10 WEB TEST (upper case), plays, not edited');
  let threw = false;
  try { await ed.save(10, 'bad!'); } catch { threw = true; }
  check(threw && !ex.sim_preset_user(10), 'webeditor SAVE: a name the device cannot type is refused');
  ed.set(LN(0, C.P_LEVEL), 99);
  await ed.flush();
  await ed.readState();
  const edited = ed.state.edited;
  await ed.load(9);
  check(edited && ed.val.get(LN(0, C.P_LEVEL)) === 44 && !ed.state.edited && ex.sim_preset_cur() === 9, 'webeditor LOAD: slot 10 back as saved');
  await ed.save(9, null);
  check(t.name(9) === 'WEB TEST', 'webeditor SAVE: a null name keeps it');
  await ed.rename(9, 'RENAMED 1');
  check(t.name(9) === 'RENAMED 1' && ed.list[9].name === 'RENAMED 1', 'webeditor RENAME');
  const free = ed.nextFree(0);
  check(free >= 0 && !ex.sim_preset_user(free), `webeditor: the next free slot (none empty: one not the user's) is ${free + 1}`);

  // BUSY: a save while a note sounds waits for silence
  t.note(60, 200);
  ex.sim_midi(0x80, 60, 0); ex.sim_midi(0xB0, 2, 0);
  port.renderMs = 400;
  events.busy.length = 0;
  await ed.save(free, 'AFTER NOTE');
  port.renderMs = 3;
  check(t.name(free) === 'AFTER NOTE' && events.busy.join() === 'true,false', `webeditor SAVE: BUSY while the note dies away (${events.busy.join()}), then saved`);

  // a lost reply: the same frame again, answered from the device's cache
  f = port.frames;
  port.drop = 2;
  const g = await ed.getMany([LN(0, C.P_LEVEL)]);
  check(g[0] === dev(LN(0, C.P_LEVEL)) && port.frames - f === 3, `webeditor: 2 replies lost, ${port.frames - f} sends, the value read`);

  // copies: a user preset (its record), a factory one (LOAD + SAVE)
  await ed.copy(9, 20);
  const r9 = await ed.read(9), r20 = await ed.read(20);
  check(t.name(20) === 'RENAMED 1' && TSX.hex(r9) === TSX.hex(r20), 'webeditor COPY: slot 10 into 21, the same record');
  const fname = t.name(0);
  await ed.copy(0, 30);
  check(ex.sim_preset_user(30) && t.name(30) === fname && ed.list[30].user, `webeditor COPY: factory slot 1 (${fname}) into 31`);

  // backup and restore: tools/presets.py's JSON both ways
  const text = await ed.backup();
  const j = JSON.parse(text), users = ed.list.filter((s) => s.user).map((s) => s.slot + 1);
  check(j.format === 'typhoon-presets' && j.version === 1 && /^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d$/.test(j.taken) &&
    j.presets.map((p) => p.slot).join() === users.join() && j.presets.every((p) => p.record.length === 1024 && Object.keys(p).join() === 'slot,name,record'),
    `webeditor BACKUP: ${j.presets.length} user presets (${users.join(' ')}), as presets.py writes them`);
  const parsed = TSX.backupParse(text);
  await ed.remove(20);
  await ed.remove(9);
  check(!ex.sim_preset_user(20) && !ex.sim_preset_user(9) && !ed.list[20].user, 'webeditor DELETE: slots 10, 21 back to factory');
  const done = await ed.restore(parsed);
  check(done.length === j.presets.length && ex.sim_preset_user(20) && t.name(9) === 'RENAMED 1' &&
    TSX.hex(await ed.read(20)) === TSX.hex(r20), 'webeditor RESTORE: every preset back, records as they were');
  const to = parsed.presets.map((p) => (p.slot === 10 ? 50 : null));
  await ed.restore(parsed, to);
  check(t.name(50) === 'RENAMED 1', 'webeditor RESTORE: one preset into another slot (51)');
  const pyFile = new URL('fixtures/tornado_backup.json', import.meta.url);
  if (fs.existsSync(pyFile)) {
    const py = TSX.backupParse(fs.readFileSync(pyFile, 'utf8'));
    const k = 55;
    await ed.restore(py, py.presets.map((_, i) => (i === 0 ? k : null)));
    const back = await ed.read(k);
    check(py.presets.every((p) => !p.bad) && TSX.hex(back) === TSX.hex(py.presets[0].record) && t.name(k) === py.presets[0].name,
      `webeditor RESTORE: a backup in the predecessor's format (fixtures/tornado_backup.json: ${py.presets[0].name}) into slot ${k + 1}`);
  }
  let bad = null;
  try { TSX.backupParse('{"format":"other"}'); } catch (e) { bad = e.message; }
  const dmg = JSON.parse(text);
  const rec0 = dmg.presets[0].record;                // (a byte the CRC covers: 100)
  dmg.presets[0].record = rec0.slice(0, 200) + (rec0.slice(200, 202) === 'ff' ? '00' : 'ff') + rec0.slice(202);
  check(bad && TSX.backupParse(JSON.stringify(dmg)).presets[0].bad === 'damaged record', 'webeditor RESTORE: not a backup, a damaged record: refused');

  // swaps: two user presets (their records), the one playing follows; a factory one; INIT past the factory slots
  await ed.load(9);
  const s9 = await ed.read(9), s30 = await ed.read(30);
  await ed.swap(9, 30);
  check(TSX.hex(await ed.read(9)) === TSX.hex(s30) && TSX.hex(await ed.read(30)) === TSX.hex(s9) && ed.state.slot === 30 &&
    t.name(30) === 'RENAMED 1', 'webeditor SWAP: user slots 10 and 31 trade records; RENAMED 1 plays on, from 31');
  const fk = 15, fn = t.name(fk), un = t.name(9);
  const fkFactory = ed.kind(fk) === 'factory';
  await ed.swap(fk, 9);
  check(fkFactory && t.name(9) === fn && t.name(fk) === un && ex.sim_preset_user(9) && ex.sim_preset_user(fk) && ed.state.slot === 30,
    `webeditor SWAP: factory slot ${fk + 1} (${fn}) and user slot 10 (${un}); what played (31) loaded again`);
  const e1 = 94, e2 = 95, emptyOk = ed.kind(e1) === 'empty' && ed.kind(e2) === 'empty';
  await ed.swap(50, e1);                            // (51 is past the factory slots: the INIT that comes back is no record)
  check(emptyOk && t.name(e1) === 'RENAMED 1' && !ex.sim_preset_user(50) && ed.kind(50) === 'empty',
    `webeditor SWAP: user slot 51 into empty ${e1 + 1}; 51 empty again`);
  await ed.swap(fk, 93);                            // (a factory slot's preset out to an empty one: INIT stays as a record)
  check(ex.sim_preset_user(fk) && t.name(fk) === 'INIT' && ex.sim_preset_user(93),
    `webeditor SWAP: factory-range slot ${fk + 1} with empty 94: ${fk + 1} holds INIT (its factory preset would come back otherwise: ${ex.sim_preset_user(fk) ? 'a record' : 'none'} "${t.name(fk)}"), 94 ${t.name(93)}`);
  await ed.swap(e1, e2);
  check(t.name(e2) === 'RENAMED 1' && !ex.sim_preset_user(e1) && ed.kind(e1) === 'empty',
    `webeditor SWAP: ${e1 + 1} and empty ${e2 + 1}: ${e1 + 1} empty again (no INIT record past the factory slots)`);

  // INIT and copy of a layer
  await ed.load(1);
  ed.set(LN(0, C.P_LEVEL), 5); ed.set(LN(0, C.P_TRANS), 7); ed.set(LN(0, C.P_E0), 99);
  await ed.initLayer(0);
  const defOk = ed.layerHandles(0).filter((h) => !ed.isEngine(h)).every((h) => dev(h) === Math.max(ed.d(h).min, Math.min(ed.d(h).max, ed.d(h).def)));
  check(defOk, 'webeditor INIT: layer 1 at DESCRIBE\'s defaults, its engine kept');
  ed.set(LN(0, C.P_TRANS), -5);
  await ed.copyLayer(0, 2);
  check(ed.layerHandles(0).every((h, i) => dev(h) === dev(LN(2, i))), 'webeditor: layer 1 copied into layer 3');

  // polling: a knob turned on the FM-1 shows here
  ed.start();
  await sleep(200);
  ex.sim_page(C.PG_LAYER);
  t.run(30);
  const lvH = LN(ex.sim_par_get(G(C.G_LAYER)), C.P_LEVEL), was = dev(lvH);
  ex.sim_enc(C.ENC_KNOB2, -5);
  t.run(30);
  await sleep(500);
  check(dev(lvH) !== was && ed.val.get(lvH) === dev(lvH), `webeditor STATE: KNOB 2 on the FM-1 (LEVEL ${was} -> ${dev(lvH)}) shows here`);
  ex.sim_enc(C.ENC_ALGORITHM, 1);                  // the engine turned on the device: the knobs follow
  t.run(30);
  await sleep(500);
  const ln = ex.sim_par_get(G(C.G_LAYER));
  check(ed.val.get(LN(ln, C.P_ENGINE)) === dev(LN(ln, C.P_ENGINE)) && knobsOk(ln),
    `webeditor STATE: ALGORITHM on the FM-1 (${ed.engineName(dev(LN(ln, C.P_ENGINE)))}): the layer's knobs described again`);
  ex.sim_preset(5);                                 // a preset turned to on the device
  t.run(30);
  await sleep(500);
  check(ed.state.slot === 5 && all.every((h) => ed.val.get(h) === dev(h)), 'webeditor STATE: a preset loaded on the FM-1: every value here');
  ed.stop();
  check(events.lost === 0, 'webeditor: never lost the link');
}
