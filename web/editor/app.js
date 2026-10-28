// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// typhoon editor page: WebMIDI, and the controls built from what the FM-1 describes (core.mjs does the talking).
import * as TSX from '../presets/tsx.mjs';
import { Link, Editor } from './core.mjs';
import { simMidi } from './simport.mjs';

// ?sim: the firmware in this page (the latest build's wasm, no sound) in place of an FM-1
const SIM = new URLSearchParams(location.search).has('sim');
const SIM_WASM = 'typhoon.wasm';                 // (?sim: the firmware for the page, beside it: tools/site.sh)

const $ = (id) => document.getElementById(id);
const store = { get(k) { try { return localStorage.getItem(k); } catch { return null; } },
                set(k, v) { try { localStorage.setItem(k, v); } catch { /* (private window) */ } } };
function el(tag, attrs = {}, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (v === undefined || v === null || v === false) continue;
    if (k === 'class') e.className = v; else if (k.startsWith('on')) e.addEventListener(k.slice(2), v); else e.setAttribute(k, v === true ? '' : v);
  }
  for (const k of kids) if (k !== null && k !== undefined) e.append(k);
  return e;
}
if (SIM) {
  $('try-sim').textContent = 'with the FM-1';
  $('try-sim').href = location.pathname;
  $('sim-note').hidden = false;
}

// where a layer's common parameters go (the device's LAYER, BREATH, ENVELOPE pages), by label; others: MORE
const LAYER_PAGES = [
  ['LAYER', ['LEVEL', 'MUTE', 'TRANSP', 'FINE', 'PAN']],
  ['BREATH', ['BR>TONE', 'BR>SHP', 'F.TYPE']],
  ['ENVELOPE', ['DELAY', 'ATTACK', 'DECAY', 'SUSTAIN', 'RELEASE', 'BR>AMP']],
];
// the sound groups in the order the FX button goes through them (by name; a new one at the end)
const GROUP_ORDER = ['CURVE', 'BREATH LP', 'DRIVE', 'DETUNE', 'DELAY', 'REVERB', 'VOLUME'];

let midi = null, link = null, ed = null, port = null;
const ctls = new Map();          // handle -> Set of controls showing it
const gcopy = new Set();         // groups shown as their global copy
let dragging = null;             // the handle under the mouse
const touched = new Map();       // handle -> when the user last moved it here
let actions = Promise.resolve();

// ------------------------------------------------------------------ messages ---
let msgTimer = null;
function say(t, cls = '') {
  const m = $('msg');
  m.textContent = t; m.className = `grow ${cls}`;
  clearTimeout(msgTimer);
  if (t && cls !== 'bad') msgTimer = setTimeout(() => { m.textContent = ''; }, 6000);
  if (!$('editor').classList.contains('on') && t) $('dev').textContent = t;
}
const slotText = (k) => { const s = ed?.list[k]; return `${String(k + 1).padStart(2, '0')} ${s?.name || '—'}`; };

// ---------------------------------------------------------------------- MIDI ---
async function ports() {
  if (SIM) {
    try { midi ??= await simMidi(SIM_WASM); } catch (e) { $('dev').textContent = `the sim did not load (${e.message})`; return; }
  } else if (!navigator.requestMIDIAccess) { $('dev').textContent = 'This browser has no Web MIDI: Chrome or Edge.'; return; }
  try { midi ??= await navigator.requestMIDIAccess({ sysex: true }); } catch (e) {
    $('dev').textContent = `MIDI with SysEx was not allowed (${e.message}): allow it for this site and reload.`; return;
  }
  const fill = (sel, list, key) => {
    const want = store.get(key), cur = sel.value;
    sel.replaceChildren(...list.map((p) => el('option', { value: p.id }, p.name || p.id)));
    const pick = list.find((p) => p.id === cur) ?? list.find((p) => p.name === want) ??
      list.find((p) => /typhoon|felucca|fm-?1/i.test(p.name || '')) ?? list[0];
    if (pick) sel.value = pick.id;
    if (!list.length) sel.append(el('option', { value: '' }, 'no MIDI port'));
  };
  fill($('in'), [...midi.inputs.values()], 'typhoon-editor-in');
  fill($('out'), [...midi.outputs.values()], 'typhoon-editor-out');
  midi.onstatechange = (e) => {
    if (port && e.port.state === 'disconnected' && (e.port.id === port.input.id || e.port.id === port.output.id)) lost(new Error('the MIDI port went away'));
    if (!port) fill($('in'), [...midi.inputs.values()], 'typhoon-editor-in'), fill($('out'), [...midi.outputs.values()], 'typhoon-editor-out');
  };
}

async function connect() {
  await ports();
  if (!midi) return;
  const input = midi.inputs.get($('in').value), output = midi.outputs.get($('out').value);
  if (!input || !output) { say('pick the FM-1\'s MIDI in and out', 'bad'); return; }
  disconnect(true);
  store.set('typhoon-editor-in', input.name); store.set('typhoon-editor-out', output.name);
  $('connect').disabled = true;
  $('dev').textContent = `asking ${output.name}…`;
  await input.open?.(); await output.open?.();
  link = new Link({ send: (b) => output.send(b) });
  input.onmidimessage = (e) => link?.receive(e.data);
  port = { input, output };
  ed = new Editor(link);
  ed.on('values', (hs) => hs.forEach(update))
    .on('text', (h, v) => { for (const c of ctls.get(h) ?? []) c.text(v); })
    .on('layer', (n) => buildLayer(n))
    .on('state', showState)
    .on('list', showList)
    .on('busy', (b) => $('busy').classList.toggle('on', b))
    .on('lost', (e) => lost(e))
    .on('error', (e) => say(e.message, 'bad'));
  try {
    const t0 = performance.now();
    await ed.connect();
    $('dev').replaceChildren(el('span', { class: 'ok' }, `typhoon ${ed.info.version}`),
      ` · protocol ${ed.info.ver} · ${ed.engines.length} engines (${ed.engines.map((e) => e.name).join(' ')}) · ${ed.info.slots} slots · read in ${((performance.now() - t0) / 1000).toFixed(1)} s`);
    build();
    window.typhoon = { ed, midi };                 // (for the console: typhoon.ed.val, typhoon.midi.ex in the sim)
    $('editor').classList.add('on');
    $('lost').classList.remove('on');
    $('connect').hidden = true; $('disconnect').hidden = false;
    ed.start();
  } catch (e) {
    console.error(e);
    $('dev').replaceChildren(el('span', { class: 'bad' }, e.message), ' — is it the FM-1, with typhoon?');
    disconnect(true);
  } finally {
    $('connect').disabled = false;
  }
}

function disconnect(quiet = false) {
  ed?.stop(); link?.close();
  if (port) port.input.onmidimessage = null;
  ed = link = port = null;
  $('editor').classList.remove('on');
  $('connect').hidden = false; $('disconnect').hidden = true;
  if (!quiet) $('dev').textContent = 'not connected';
}

function lost(e) {
  if (!ed) return;
  disconnect(true);
  $('lost-why').textContent = e.message;
  $('lost').classList.add('on');
  $('dev').textContent = 'not connected';
}

// ------------------------------------------------------------------ controls ---
// one control for handle h: label, then a slider / select / switch, then the value as the FM-1 shows it
function control(h) {
  const d = ed.d(h);
  if (!d) return el('div');
  if (d.label === '-' && d.min === d.max) return el('div', { class: 'ctl blank' }, el('span', { class: 'lab' }, '—'));
  const box = el('div', { class: 'ctl' }), lab = el('span', { class: 'lab', title: d.label }, d.label);
  const range = `${d.min}..${d.max}, default ${d.def}`;
  let input, val = null;
  const now = () => ed.value(h);
  // self: from this control's own input (it shows the value already; not overwritten while the hand is on it)
  const setTo = (v, self = false) => {
    if (self) { touched.set(h, Date.now()); setTimeout(() => c.show(), 600); }
    ed.set(h, v);
  };
  if (d.enum) {
    input = el('select', { 'aria-label': d.label });
    for (const [v, name] of d.enum) input.append(el('option', { value: v }, name));
    input.addEventListener('change', () => setTo(Number(input.value), true));
    box.classList.add('wide');
  } else if (d.fmt === TSX.F.ONOFF && d.min === 0 && d.max === 1) {
    input = el('input', { type: 'checkbox', 'aria-label': d.label });
    input.addEventListener('change', () => setTo(input.checked ? 1 : 0, true));
    val = el('span', { class: 'val' });
    box.append(lab, el('span', {}, input), val);
  } else {
    input = el('input', { type: 'range', min: d.min, max: d.max, step: 1, 'aria-label': d.label });
    input.addEventListener('input', () => setTo(Number(input.value), true));
    input.addEventListener('pointerdown', () => { dragging = h; });
    input.addEventListener('wheel', (e) => {
      e.preventDefault();
      const step = (e.shiftKey ? 10 : 1) * (e.deltaY < 0 || e.deltaX < 0 ? 1 : -1);
      setTo(Math.max(d.min, Math.min(d.max, now() + step)));
    }, { passive: false });
    val = el('span', { class: 'val', tabindex: 0, title: `click to type (${range})` });
  }
  if (!box.children.length) box.append(lab, input, ...(val ? [val] : []));
  box.title = `${d.label}: ${range}${d.unit ? ` (${d.unit})` : ''} · double-click: default`;
  box.addEventListener('dblclick', (e) => { if (e.target !== val) setTo(d.def); });
  if (val) {
    const typeIn = () => {
      const t = el('input', { type: 'text', class: 'type', value: now(), 'aria-label': `${d.label} value` });
      val.replaceWith(t); t.focus(); t.select();
      let done = false;
      const end = (ok) => {
        if (done) return; done = true;
        if (ok) {
          const s = t.value.trim(), n = Number(s);
          if (s !== '' && Number.isFinite(n)) setTo(n);
          else say(`${d.label}: a number, ${range}`, 'bad');
        }
        t.replaceWith(val); c.show();
      };
      t.addEventListener('keydown', (e) => { if (e.key === 'Enter') end(true); else if (e.key === 'Escape') end(false); });
      t.addEventListener('blur', () => end(true));
    };
    val.addEventListener('click', typeIn);
    val.addEventListener('keydown', (e) => { if (e.key === 'Enter') { e.preventDefault(); typeIn(); } });
  }
  const c = {
    show(force = false) {
      const v = now();
      if (v === undefined) return;
      const held = dragging === h || Date.now() - (touched.get(h) ?? 0) < 500;
      if (force || !held) {
        if (input.type === 'checkbox') input.checked = !!v;
        else if (String(input.value) !== String(v)) {
          if (input.tagName === 'SELECT' && ![...input.options].some((o) => o.value === String(v))) input.append(el('option', { value: v }, `#${v}`));
          input.value = v;
        }
      }
      box.classList.toggle('notdef', v !== d.def);
      c.text(v);
    },
    text(v) {
      if (!val || v !== now()) return;
      const s = ed.text(h, v);
      val.textContent = s ?? String(v);
      val.classList.toggle('wait', s === null);
    },
  };
  (ctls.get(h) ?? ctls.set(h, new Set()).get(h)).add(c);
  box.ctl = c; box.handle = h;
  c.show(true);
  return box;
}
function update(h) { for (const c of ctls.get(h) ?? []) c.show(); if (ed && ed.isEngine(h)) markOff(ed.layerOf(h)); }
window.addEventListener('pointerup', () => {
  if (dragging === null) return;
  const h = dragging;
  dragging = null; touched.set(h, Date.now());
  setTimeout(() => update(h), 600);
});
function forgetCtls(root) {             // controls inside root are gone
  for (const b of root.querySelectorAll('.ctl')) if (b.ctl) ctls.get(b.handle)?.delete(b.ctl);
  if (root.engCtl) ctls.get(root.engCtl.h)?.delete(root.engCtl);
}

// ------------------------------------------------------------------- layout ---
function build() {
  for (const s of ctls.values()) s.clear();
  ctls.clear();
  const layers = $('layers');
  layers.replaceChildren();
  for (let n = 0; n < ed.layout.layers; n++) layers.append(el('section', { class: 'layer', id: `layer-${n}` }));
  for (let n = 0; n < ed.layout.layers; n++) buildLayer(n);
  buildGroups();
  const g = $('globals');
  g.replaceChildren(...ed.globalHandles().map(control));
  showState(ed.state);
  showList(ed.list);
}

function buildLayer(n) {
  const sec = $(`layer-${n}`);
  if (!sec || !ed) return;
  const fa = sec.contains(document.activeElement) ? document.activeElement : null;
  const focus = fa?.classList.contains('engine') ? 'engine' : fa?.closest('.ctl')?.handle;
  forgetCtls(sec);
  const eh = TSX.H.layer(n, TSX.P_ENGINE), common = ed.commonHandles(n).filter((h) => h !== eh);
  const eng = el('select', { class: 'engine', 'aria-label': `layer ${n + 1} engine`, title: 'the engine (ALGORITHM)' });
  for (const [id, name] of ed.d(eh).enum) eng.append(el('option', { value: id }, name));
  eng.value = ed.value(eh);
  eng.addEventListener('change', () => { touched.set(eh, Date.now()); ed.set(eh, Number(eng.value)); });
  const engCtl = { h: eh, show() { if (Date.now() - (touched.get(eh) ?? 0) > 500) eng.value = ed.value(eh); }, text() {} };
  (ctls.get(eh) ?? ctls.set(eh, new Set()).get(eh)).add(engCtl);
  sec.engCtl = engCtl;
  const copyTo = el('select', { 'aria-label': `copy layer ${n + 1} to`, title: 'this layer\'s whole sound into another layer' },
    el('option', { value: '' }, 'copy to…'));
  for (let k = 0; k < ed.layout.layers; k++) if (k !== n) copyTo.append(el('option', { value: k }, `layer ${k + 1}`));
  copyTo.addEventListener('change', () => {
    const v = copyTo.value;
    copyTo.value = '';
    if (v !== '') act(`layer ${n + 1} → layer ${Number(v) + 1}`, () => ed.copyLayer(n, Number(v)));
  });
  const head = el('div', { class: 'head' }, el('h2', {}, `LAYER ${n + 1}`), eng,
    el('button', { title: 'every parameter of this layer back to its default (the engine kept)', onclick: () => act(`layer ${n + 1}: INIT`, () => ed.initLayer(n)) }, 'INIT'),
    copyTo, el('span', { class: 'tag dim', id: `ui-${n}`, title: 'the layer the FM-1 is on (SELECT)', hidden: true }, 'ON FM-1'));
  const body = el('div', { class: 'body' });
  const placed = new Set();
  for (const [title, labels] of LAYER_PAGES) {
    const hs = labels.map((l) => common.find((h) => ed.d(h)?.label === l)).filter((h) => h !== undefined);
    if (!hs.length) continue;
    hs.forEach((h) => placed.add(h));
    body.append(el('h3', {}, title), ...hs.map(control));
  }
  const th = ed.toneHandle(n), tone = th === undefined ? [] : [th, ...ed.toneKnobs(n)];   // (after the engine: its own)
  tone.forEach((h) => placed.add(h));
  const more = common.filter((h) => !placed.has(h));
  if (more.length) body.append(el('h3', {}, 'MORE'), ...more.map(control));
  const name = ed.engineName(ed.value(eh));
  // (EDIT 3 / 4, four knobs each, only when the engine uses them: as the FM-1 passes them by)
  const live = (h) => ed.d(h)?.max > ed.d(h)?.min;
  const knobs = ed.knobHandles(n).filter((h, i, a) => i < 8 || a.slice(i - (i % 4), i - (i % 4) + 4).some(live));
  body.append(el('h3', {}, `${name} KNOBS`), ...knobs.map(control));
  if (tone.length) body.append(el('h3', {}, 'TONE'), ...tone.filter((h) => h === th || (ed.d(h)?.max > ed.d(h)?.min)).map(control));
  sec.replaceChildren(head, body);
  if (focus === 'engine') eng.focus();
  else if (focus !== undefined) [...body.querySelectorAll('.ctl')].find((b) => b.handle === focus)?.querySelector('input, select')?.focus();
  markOff(n);
  if (ed.state) showState(ed.state);
}
// (an engine change: the layer is built again when its knobs are described again, the 'layer' event)
function markOff(n) {
  const sec = $(`layer-${n}`);
  if (sec && ed) sec.classList.toggle('off', ed.value(TSX.H.layer(n, TSX.P_ENGINE)) === TSX.ENG_OFF);
}

function buildGroups() {
  const box = $('groups');
  forgetCtls(box);
  // (a group with no live knob is left out: BREATH LP, retired 2026-10-28, its three kept in S_* as nothing)
  const live = (h) => ed.d(h)?.max > ed.d(h)?.min;
  const groups = [...new Set(ed.soundHandles().filter(live).map((h) => ed.d(h)?.group).filter((g) => g !== null && g !== undefined))];
  const rank = (g) => { const i = GROUP_ORDER.indexOf(TSX.groupName(g)); return i < 0 ? 100 + g : i; };
  groups.sort((a, b) => rank(a) - rank(b));
  box.replaceChildren(...groups.map((g) => el('section', { class: 'card group', id: `group-${g}` })));
  groups.forEach(buildGroup);
}

function buildGroup(g) {
  const card = $(`group-${g}`);
  forgetCtls(card);
  const glob = gcopy.has(g);
  const hs = ed.groupHandles(g).map((h) => (glob ? h | 0x8000 : h));
  const own = el('button', { title: 'the preset has its own copy of this group', onclick: () => act(`${TSX.groupName(g)}: own`, () => ed.group(g, true)) }, 'OWN');
  const gl = el('button', { title: 'the preset uses the global copy (the GLO pages); its own is dropped', onclick: () => act(`${TSX.groupName(g)}: global`, () => ed.group(g, false)) }, 'GLOBAL');
  const cb = el('input', { type: 'checkbox' });
  cb.checked = glob;
  cb.addEventListener('change', () => { if (cb.checked) gcopy.add(g); else gcopy.delete(g); buildGroup(g); });
  card.replaceChildren(
    el('div', { class: 'head' }, el('h2', {}, TSX.groupName(g)), el('span', { class: 'seg' }, own, gl),
      el('label', { class: 'small', title: 'edit the global copy itself (every preset that uses it)' }, cb, 'global copy')),
    el('div', { class: 'ctls' }, ...hs.map(control)));
  card.own = own; card.gl = gl;
  showGroup(g);
}
function showGroup(g) {
  const card = $(`group-${g}`);
  if (!card || !ed?.state) return;
  const own = ed.ownGroup(g);
  card.own.classList.toggle('on', own); card.gl.classList.toggle('on', !own);
  card.classList.toggle('global', !own);
  card.classList.toggle('gcopy', gcopy.has(g));
}

// --------------------------------------------------------------- state, list ---
function showState(s) {
  if (!s || !ed) return;
  const name = ed.list[s.slot]?.name || '—';
  $('now').replaceChildren(`${String(s.slot + 1).padStart(2, '0')} ${name}`, s.edited ? el('span', { class: 'edited', title: 'edited: not saved' }, ' *') : '',
    el('span', { class: 'dim', style: 'font-weight:400;font-size:13px' }, `  ${ed.kind(s.slot)}`));
  document.title = `${String(s.slot + 1).padStart(2, '0')} ${name}${s.edited ? ' *' : ''} — typhoon editor`;
  for (let n = 0; n < ed.layout.layers; n++) {
    $(`layer-${n}`)?.classList.toggle('ui', n === s.layer);
    const t = $(`ui-${n}`); if (t) t.hidden = n !== s.layer;
  }
  for (let g = 0; g < ed.layout.groups; g++) showGroup(g);
  for (const b of $('slots').children) b.classList.toggle('cur', Number(b.dataset.slot) === s.slot);
  $('save').textContent = `Save to ${s.slot + 1}`;
}

function showList(list) {
  if (!ed) return;
  const n = ed.info.slots, cur = ed.state?.slot;
  const box = $('slots');
  if (box.children.length !== n) box.replaceChildren(...Array.from({ length: n }, (_, k) =>
    el('button', { class: 'slot', 'data-slot': k, onclick: () => loadSlot(k) })));
  let users = 0;
  for (const b of box.children) {
    const k = Number(b.dataset.slot), s = list[k], kind = ed.kind(k);
    if (kind === 'user') users++;
    b.className = `slot ${kind}${k === cur ? ' cur' : ''}`;
    b.title = `${k + 1}: ${s?.name || '(nothing)'} — ${kind === 'user' ? 'saved by you' : kind === 'factory' ? 'factory preset' : 'INIT (nothing saved)'} · click: load`;
    b.replaceChildren(el('b', {}, String(k + 1).padStart(2, '0')), el('span', {}, s?.name || '—'));
  }
  $('list-info').textContent = `${users} yours, ${ed.info.factory} factory slots; the outlined ones are yours`;
  for (const id of ['rn-slot', 'cp-from', 'cp-to', 'dl-slot', 'sa-slot']) {
    const sel = $(id), was = sel.value;
    sel.replaceChildren(...Array.from({ length: n }, (_, k) => el('option', { value: k }, `${slotText(k)}${ed.kind(k) === 'user' ? '' : ed.kind(k) === 'factory' ? ' (factory)' : ''}`)));
    sel.value = was !== '' && Number(was) < n ? was : String(cur ?? 0);
  }
  if (ed.state) showState(ed.state);
}

// ------------------------------------------------------------------ actions ---
// the preset and group actions run one after the other, in the order clicked
function act(what, fn) {
  if (!ed) return Promise.resolve();
  const mine = ed;
  actions = actions.then(async () => {
    if (ed !== mine) return;
    say(`${what}…`);
    try {
      const r = await fn();
      say(typeof r === 'string' ? r : `${what}: done`, 'ok');
    } catch (e) {
      say(`${what}: ${e.message}`, 'bad');
    }
  });
  return actions;
}

function loadSlot(k) {
  if (ed.state?.edited && $('ask-edits').checked && !confirm(`The sound is edited and not saved. Load ${slotText(k)} and drop the edits?`)) return;
  act(`load ${slotText(k)}`, () => ed.load(k));
}

$('save').addEventListener('click', () => {
  const k = ed.state.slot;
  if (ed.kind(k) === 'user' || ed.state.edited || confirm(`Save into ${slotText(k)}?`)) act(`save to ${k + 1}`, () => ed.save(k, null));
});
$('revert').addEventListener('click', () => {
  if (!ed.state.edited || confirm('Drop the edits and load the slot again?')) act('revert', () => ed.load(ed.state.slot));
});
$('savefree').addEventListener('click', () => {
  const k = ed.nextFree(ed.state.slot);
  if (k < 0) { say('no free slot: every slot holds a preset of yours', 'bad'); return; }
  const name = ed.list[ed.state.slot]?.name || 'NEW';
  act(`save to ${k + 1} as ${name}`, () => ed.save(k, name));
});

function saveAsCheck() {
  const k = Number($('sa-slot').value), nm = $('sa-name').value.toUpperCase();
  const w = [];
  if (!TSX.NAME_OK.test(nm.replace(/\s+$/, '')) || !nm.trim()) w.push('a name: 1 to 12 of A-Z 0-9 space - . /');
  if (ed.kind(k) === 'user' && k !== ed.state.slot) w.push(`overwrites your ${ed.list[k].name}`);
  $('sa-warn').textContent = w.join(' · ');
  return !w.length || (w.length === 1 && w[0].startsWith('overwrites'));
}
$('saveas').addEventListener('click', () => {
  const dlg = $('saveas-dlg');
  $('sa-slot').value = String(ed.nextFree(ed.state.slot) >= 0 ? ed.nextFree(ed.state.slot) : ed.state.slot);
  $('sa-name').value = ed.list[ed.state.slot]?.name || '';
  saveAsCheck();
  dlg.returnValue = '';
  dlg.showModal();
  $('sa-name').focus(); $('sa-name').select();
});
$('sa-name').addEventListener('input', saveAsCheck);
$('sa-slot').addEventListener('change', saveAsCheck);
$('saveas-dlg').addEventListener('close', () => {
  if ($('saveas-dlg').returnValue !== 'yes' || !saveAsCheck()) return;
  const k = Number($('sa-slot').value), nm = $('sa-name').value;
  act(`save to ${k + 1} as ${nm.toUpperCase()}`, () => ed.save(k, nm));
});

$('rename').addEventListener('click', () => {
  const k = Number($('rn-slot').value);
  act(`rename ${k + 1}`, () => ed.rename(k, $('rn-name').value));
});
$('rn-slot').addEventListener('change', () => { $('rn-name').value = ed.list[Number($('rn-slot').value)]?.name ?? ''; });
$('copy').addEventListener('click', () => {
  const a = Number($('cp-from').value), b = Number($('cp-to').value);
  if (a === b) { say('copy: two different slots', 'bad'); return; }
  const factory = ed.kind(a) !== 'user';
  if (ed.kind(b) === 'user' && !confirm(`Overwrite your ${slotText(b)} with ${slotText(a)}?`)) return;
  if (factory && ed.state.edited && !confirm(`${slotText(a)} is a factory preset: it is copied by loading it and saving it, so the edits of the sound now playing are dropped. Go on?`)) return;
  act(`copy ${a + 1} → ${b + 1}`, () => ed.copy(a, b));
});
$('delete').addEventListener('click', () => {
  const k = Number($('dl-slot').value);
  if (ed.kind(k) !== 'user') { say(`${slotText(k)} holds nothing of yours`, 'bad'); return; }
  if (!confirm(`Delete your ${slotText(k)}? The slot plays its factory preset again (or INIT).`)) return;
  act(`delete ${k + 1}`, () => ed.remove(k));
});

$('backup').addEventListener('click', () => act('backup', async () => {
  const text = await ed.backup(null, (i, n, k) => say(`backup: ${i + 1}/${n} slot ${k + 1}…`));
  const n = JSON.parse(text).presets.length;
  const p2 = (x) => String(x).padStart(2, '0'), d = new Date();
  const file = `typhoon-presets-${d.getFullYear()}-${p2(d.getMonth() + 1)}-${p2(d.getDate())}_${p2(d.getHours())}${p2(d.getMinutes())}${p2(d.getSeconds())}.json`;
  const a = el('a', { href: URL.createObjectURL(new Blob([text], { type: 'application/json' })), download: file });
  document.body.append(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 10000);
  return `backup: ${n} preset(s) → ${file}`;
}));

let restoring = null;
$('restore').addEventListener('click', () => { $('restore-file').value = ''; $('restore-file').click(); });
$('restore-file').addEventListener('change', async () => {
  const f = $('restore-file').files[0];
  if (!f) return;
  try { restoring = TSX.backupParse(await f.text()); } catch (e) { say(`${f.name}: ${e.message}`, 'bad'); return; }
  $('rs-info').textContent = `${f.name}: ${restoring.presets.length} preset(s)${restoring.taken ? `, taken ${restoring.taken}` : ''}. Each goes into the slot on the right (change it to put it elsewhere).`;
  $('rs-rows').replaceChildren(...restoring.presets.map((p, i) => {
    const on = el('input', { type: 'checkbox', checked: !p.bad, disabled: !!p.bad, 'data-i': i });
    const to = el('input', { type: 'number', min: 1, max: ed.info.slots, value: Math.min(Math.max(p.slot || 1, 1), ed.info.slots), 'data-i': i });
    const note = el('td', { class: p.bad ? 'bad' : 'dim' });
    const hint = () => { const k = Number(to.value) - 1; note.textContent = p.bad ?? (ed.kind(k) === 'user' ? `overwrites ${ed.list[k].name}` : ''); };
    to.addEventListener('input', hint); hint();
    return el('tr', {}, el('td', {}, on), el('td', {}, String(p.slot)), el('td', {}, p.name), el('td', {}, to), note);
  }));
  $('restore-dlg').returnValue = '';
  $('restore-dlg').showModal();
});
$('restore-dlg').addEventListener('close', () => {
  if ($('restore-dlg').returnValue !== 'yes' || !restoring) return;
  const rows = [...$('rs-rows').children];
  const to = restoring.presets.map((_, i) => {
    const [on, num] = [rows[i].querySelector('input[type=checkbox]'), rows[i].querySelector('input[type=number]')];
    return on.checked ? Number(num.value) - 1 : null;
  });
  const dup = to.filter((k) => k !== null).find((k, i, a) => a.indexOf(k) !== i);
  if (dup !== undefined) { say(`restore: two presets into slot ${dup + 1}`, 'bad'); return; }
  const parsed = restoring;
  act('restore', async () => {
    const done = await ed.restore(parsed, to, (i, n, k) => say(`restore: ${i + 1}/${n} → slot ${k + 1}…`));
    return `restore: ${done.length} preset(s) written (${done.map((d) => `${d.slot + 1} ${d.name}`).join(', ')})`;
  });
});
$('relist').addEventListener('click', () => act('read the slots', () => ed.listAll().then(() => 'slots read')));

$('connect').addEventListener('click', connect);
$('reconnect').addEventListener('click', connect);
$('disconnect').addEventListener('click', () => disconnect());
ports();
