// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The installer's step before an install (app/install_app.js, fm1-base's web/installer.html APP.before): when the
// FM-1 runs typhoon, its user presets saved to a file first, the editor's BACKUP (the same JSON: tools/presets.py's).
// The presets stay on the FM-1 through an install; the file is in case they don't (a wiped store, another firmware).
//   await before(midi, ui)   -> {saved, file} | null (no typhoon answered, or not saved) | false (stop the install)
//   find(midi, log)          -> {input, output, link, ed} of the first port typhoon answers on, or null
import * as TSX from '../presets/tsx.mjs';
import { Link, Editor } from './core.mjs';

// the ports to ask: those whose name looks like an FM-1 first; an output with an input of the same name
function pairs(midi) {
  const ins = [...midi.inputs.values()], fm1 = (p) => /typhoon|felucca|fm-?1/i.test(p.name || '');
  return [...midi.outputs.values()].map((output) => ({ output, input: ins.find((i) => i.name === output.name) }))
    .filter((p) => p.input).sort((a, b) => fm1(b.output) - fm1(a.output));
}

export async function find(midi, log = () => {}, { timeout = 300, tries = 2 } = {}) {
  for (const { input, output } of pairs(midi)) {
    await input.open?.(); await output.open?.();
    const link = new Link({ send: (b) => output.send(b) }, { timeout, tries });
    const was = input.onmidimessage;
    input.onmidimessage = (e) => link.receive(e.data);
    const done = () => { link.close(); input.onmidimessage = was; };
    try {
      const r = await link.request(TSX.CMD.INFO, []);
      const info = r.status === 'OK' ? TSX.parseInfo(r.data) : null;
      if (info && info.name === 'TYPHOON' && info.ver === TSX.VER) {
        link.tries = 8;                           // (found: the reads as patient as the editor's)
        const ed = new Editor(link);
        ed.info = info;
        log(`presets: typhoon ${info.version} on ${output.name}`);
        return { input, output, link, ed, done };
      }
      log(`presets: ${output.name} says ${info ? `${info.name} ${info.ver}` : r.status}`);
    } catch (_) {
      log(`presets: no answer on ${output.name}`);
    }
    done();
  }
  return null;
}

const fileName = (d = new Date()) => {
  const p2 = (x) => String(x).padStart(2, '0');
  return `typhoon-presets-${d.getFullYear()}-${p2(d.getMonth() + 1)}-${p2(d.getDate())}_${p2(d.getHours())}${p2(d.getMinutes())}${p2(d.getSeconds())}.json`;
};

function download(text, file) {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
  a.download = file;
  document.body.append(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 10000);
}

export async function before(midi, ui, { save = download } = {}) {
  const dev = await find(midi, ui.log);
  if (!dev) return null;
  try {
    await dev.ed.listAll();
    const users = dev.ed.list.filter((s) => s?.user);
    if (!users.length) { ui.log('presets: none of yours on the FM-1'); return null; }
    const esc = (x) => x.replace(/[&<>"]/g, (c) => `&#${c.charCodeAt(0)};`);
    const names = users.slice(0, 8).map((s) => `${s.slot + 1} ${esc(s.name)}`).join(', ') + (users.length > 8 ? ', …' : '');
    const pick = await ui.ask('Your presets',
      `typhoon ${dev.ed.info.version} on this FM-1 has <b>${users.length}</b> preset(s) of yours (${names}). They stay ` +
      'on the FM-1 through an install; a file of them is in case they don\'t (the preset editor\'s RESTORE takes it).',
      [['save', 'Save them, then install'], ['skip', 'Install without'], ['stop', 'Cancel']]);
    if (pick === 'stop') return false;
    if (pick !== 'save') return null;
    const text = await dev.ed.backup(users.map((s) => s.slot), (i, n, k) => {
      ui.say(`Saving your presets: ${i + 1}/${n} (slot ${k + 1})…`);
      ui.progress(((i + 1) / n) * 100);
    });
    const file = fileName(), saved = JSON.parse(text).presets.length;
    save(text, file);
    ui.progress(0);
    ui.say(`${saved} preset(s) saved: ${file}`);
    return { saved, file };
  } finally {
    dev.done();
  }
}
