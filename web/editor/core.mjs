// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The preset editor's logic, without a page: the requests to the FM-1 (Link) and what it knows of the sound (Editor).
// The protocol is core/sysex.c's (web/presets/tsx.mjs); everything about the parameters comes from the device
// (LAYOUT, DESCRIBE, TEXT, ENGINES), so a new parameter or engine shows by itself.
//
// A transport is {send(Uint8Array)}; whatever comes from the device goes to link.receive(bytes). The page's is
// WebMIDI (web/editor/app.js), the tests' the firmware compiled to wasm (tests/sim/webeditor.test.mjs).
//
//   const link = new Link(transport), ed = new Editor(link);
//   await ed.connect();             INFO, LAYOUT, ENGINES, every DESCRIBE, the values, the slots, STATE
//   ed.set(handle, value)           as a knob does: coalesced, at most one SET in flight, ~30 a second
//   ed.on('values' | 'layer' | 'text' | 'state' | 'list' | 'busy' | 'lost' | 'error', fn)
//   ed.start() / ed.stop()          STATE every 250 ms; everything read again when its counter moved
import * as TSX from '../presets/tsx.mjs';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const { CMD, H } = TSX;

export class LinkError extends Error {}

// requests one at a time, each with a fresh seq; no reply in `timeout` ms: the same frame again (the device answers a
// repeat from its cache); BUSY: again after `busyWait` ms (a write waits for silence). Waiting requests go by priority
// (0 first), then in order.
export class Link {
  constructor(transport, { timeout = 300, tries = 8, busyWait = 300, busyFor = 60000 } = {}) {
    Object.assign(this, { transport, timeout, tries, busyWait, busyFor });
    this.seq = (Date.now() & 127);
    this.queue = [];
    this.order = 0;
    this.running = false;
    this.wait = null;            // {cmd, seq, resolve} of the frame in flight
    this.onBusy = null;          // (bool) a write is waiting for silence
    this.sent = 0;               // frames sent (resends too), for the tests
    this.closed = false;
  }

  receive(bytes) {               // anything from the device: our replies are taken, the rest (notes, clock ...) ignored
    if (!bytes || bytes[0] !== 0xF0 || !this.wait) return;
    const r = TSX.parse(bytes);
    if (r && r.cmd === this.wait.cmd && r.seq === this.wait.seq) { const w = this.wait; this.wait = null; w.resolve(r); }
  }

  request(cmd, data = [], prio = 1) {
    if (this.closed) return Promise.reject(new LinkError('not connected'));
    return new Promise((resolve, reject) => {
      this.queue.push({ cmd, data, prio, order: this.order++, resolve, reject });
      this.#run();
    });
  }

  // as request, and the status must be OK: -> data
  async ok(cmd, data = [], prio = 1, what = '') {
    const r = await this.request(cmd, data, prio);
    if (r.status !== 'OK') throw new Error(`${what || Object.keys(CMD).find((k) => CMD[k] === cmd)}: the FM-1 says ${r.status}`);
    return r.data;
  }

  close() {
    this.closed = true;
    for (const j of this.queue.splice(0)) j.reject(new LinkError('not connected'));
  }

  async #run() {
    if (this.running) return;
    this.running = true;
    while (this.queue.length && !this.closed) {
      let k = 0;
      for (let i = 1; i < this.queue.length; i++) {
        const a = this.queue[i], b = this.queue[k];
        if (a.prio < b.prio || (a.prio === b.prio && a.order < b.order)) k = i;
      }
      const job = this.queue.splice(k, 1)[0];
      try { job.resolve(await this.#exchange(job.cmd, job.data)); } catch (e) { job.reject(e); }
    }
    this.running = false;
  }

  async #exchange(cmd, data) {
    this.seq = (this.seq + 1) & 127;
    const seq = this.seq, msg = TSX.frame(cmd, seq, data), t0 = Date.now();
    let busy = false;
    try {
      for (;;) {
        let r = null;
        for (let i = 0; i < this.tries && !r && !this.closed; i++) {
          r = await new Promise((resolve) => {
            const timer = setTimeout(() => { this.wait = null; resolve(null); }, this.timeout);
            this.wait = { cmd, seq, resolve: (x) => { clearTimeout(timer); resolve(x); } };
            this.sent++;
            try { this.transport.send(msg); } catch (e) { clearTimeout(timer); this.wait = null; resolve(null); }
          });
        }
        if (!r) throw new LinkError(this.closed ? 'not connected' : 'no answer from the FM-1');
        if (r.status !== 'BUSY') return r;
        if (!busy) { busy = true; this.onBusy?.(true); }
        if (Date.now() - t0 > this.busyFor) throw new LinkError('the FM-1 stayed busy (a note sounding) too long');
        await sleep(this.busyWait);
      }
    } finally {
      if (busy) this.onBusy?.(false);
    }
  }
}

// priorities: what the user does first, then descriptions, the polling, the value texts
const P_USER = 0, P_DESC = 1, P_POLL = 2, P_TEXT = 3;

export class Editor {
  constructor(link, { poll = 250, setGap = 33, quiet = 400 } = {}) {
    Object.assign(this, { link, pollMs: poll, setGap, quietMs: quiet });
    this.fns = {};
    this.desc = new Map();        // handle -> {fmt, min, max, def, names, group, label, unit, enum: Map value -> name | null}
    this.val = new Map();         // handle -> value as the device has it (or as we set it)
    this.texts = new Map();       // `${handle}:${value}` -> the screen's text
    this.wantText = new Map();    // handle -> value whose text is wanted (the latest only)
    this.pending = new Map();     // handle -> value to SET (the latest only)
    this.redescribe = new Set();  // layers whose engine knobs to describe again
    this.lastSet = 0;
    this.settled = [];
    this.lastEngine = [];
    this.state = null;
    this.list = [];
    this.timer = null;
    link.onBusy = (b) => this.emit('busy', b);
  }

  on(ev, fn) { (this.fns[ev] ??= []).push(fn); return this; }
  emit(ev, ...a) { for (const f of this.fns[ev] ?? []) { try { f(...a); } catch (e) { console.error(e); } } }

  // ---------------------------------------------------------------- handles ---
  layerHandles(n) { return Array.from({ length: this.layout.p }, (_, i) => H.layer(n, i)); }
  knobHandles(n) { return this.layerHandles(n).slice(this.layout.e0); }
  // the TONE stage (core/tone.c): its type, then the three knobs whose meaning follows it (described again with it)
  toneHandle(n) { return this.commonHandles(n).find((h) => this.d(h)?.label === 'TONE'); }
  toneKnobs(n) { const h = this.toneHandle(n); return h === undefined ? [] : [h + 1, h + 2, h + 3]; }
  #toneSide(h) { const n = this.layerOf(h), t = n >= 0 ? this.toneHandle(n) : undefined; return t !== undefined && h >= t && h <= t + 3; }
  commonHandles(n) { return this.layerHandles(n).slice(0, this.layout.e0); }
  soundHandles() { return Array.from({ length: this.layout.s }, (_, i) => H.snd(i)); }
  globalSoundHandles() { return Array.from({ length: this.layout.s }, (_, i) => H.sndGlobal(i)); }
  globalHandles() { return Array.from({ length: this.layout.g }, (_, i) => H.global(i)); }
  allHandles() {
    const hs = [];
    for (let n = 0; n < this.layout.layers; n++) hs.push(...this.layerHandles(n));
    return hs.concat(this.soundHandles(), this.globalSoundHandles(), this.globalHandles());
  }
  groupHandles(g) { return this.soundHandles().filter((h) => this.desc.get(h)?.group === g); }
  isEngine(h) { return h < 0x4000 && (h & 0xFF) === TSX.P_ENGINE; }
  layerOf(h) { return h < 0x4000 ? h >> 8 : -1; }
  // the descriptor of a handle (a sound's global copy is the same parameter as the preset's)
  d(h) { return this.desc.get(h & 0x4000 ? (h & ~0x8000) : h); }
  engineName(id) { return id === TSX.ENG_OFF ? 'OFF' : this.engines.find((e) => e.id === id)?.name ?? `#${id}`; }

  // ---------------------------------------------------------------- connect ---
  async connect() {
    const r = await this.link.request(CMD.INFO, [], P_USER);
    if (r.status !== 'OK') throw new Error(`INFO: the FM-1 says ${r.status}`);
    this.info = TSX.parseInfo(r.data);
    if (this.info.name !== 'TYPHOON' || this.info.ver !== TSX.VER)
      throw new Error(`not typhoon with preset protocol ${TSX.VER} (it says ${this.info.name} ${this.info.ver})`);
    const l = await this.link.request(CMD.LAYOUT, [], P_USER);
    if (l.status !== 'OK') throw new Error(`typhoon ${this.info.version} has no editor (LAYOUT: ${l.status}): install a build from f65fb78 on`);
    this.layout = TSX.parseLayout(l.data);
    this.engines = [];
    while (this.engines.length < this.layout.engines) {
      const e = TSX.parseEngines(await this.link.ok(CMD.ENGINES, [this.engines.length], P_USER));
      if (!e.list.length) break;
      this.engines.push(...e.list);
    }
    // the layers' common parameters, the sound's, the globals; the engine knobs follow from the first refresh (a
    // layer's knobs are described whenever its engine or knobs move)
    const hs = [];
    for (let n = 0; n < this.layout.layers; n++) hs.push(...this.commonHandles(n));
    hs.push(...this.soundHandles(), ...this.globalHandles());
    for (const h of hs) await this.#describe(h);
    this.lastEngine = [];
    await this.refresh(P_USER);
    await this.listAll();
    await this.readState();
    return this;
  }

  // DESCRIBE h; an enum's names by TEXT (an engine's from ENGINES). -> true when it is not as it was
  async #describe(h, prio = P_USER, fresh = false) {
    const d = TSX.describe(await this.link.ok(CMD.DESCRIBE, TSX.u16(h), prio, `DESCRIBE ${h.toString(16)}`));
    const old = this.desc.get(h);
    const changed = !old || old.label !== d.label || old.fmt !== d.fmt || old.min !== d.min || old.max !== d.max ||
      old.names !== d.names || old.unit !== d.unit;
    if (changed || fresh) this.#forget(h);
    d.enum = null;
    if (this.isEngine(h)) {
      d.enum = new Map([[TSX.ENG_OFF, await this.#text(h, TSX.ENG_OFF, prio)], ...this.engines.map((e) => [e.id, e.name])]);
    } else if (d.fmt === TSX.F.ENUM && d.names > 0) {
      d.enum = new Map();
      for (let v = d.min; v <= d.max; v++) d.enum.set(v, await this.#text(h, v, prio));
    }
    this.desc.set(h, d);
    return changed;
  }

  #forget(h) {                                     // h's cached texts (its meaning changed)
    for (const k of [...this.texts.keys()]) if (k.startsWith(`${h}:`)) this.texts.delete(k);
  }

  async getMany(hs, prio = P_USER) {               // GET, 16 a request -> values
    const out = [];
    for (let i = 0; i < hs.length; i += 16) {
      const part = hs.slice(i, i + 16), d = await this.link.ok(CMD.GET, part.flatMap(TSX.u16), prio, 'GET');
      part.forEach((_, k) => out.push(TSX.s16(d, k * 2)));
    }
    return out;
  }

  async #text(h, v, prio) {
    const k = `${h}:${v}`;
    if (!this.texts.has(k)) this.texts.set(k, TSX.nameOf(await this.link.ok(CMD.TEXT, [...TSX.u16(h), ...TSX.i16(v)], prio, 'TEXT')));
    return this.texts.get(k);
  }

  // ----------------------------------------------------------------- values ---
  // GET every handle (16 a request); layers whose engine or knobs moved are described again. -> handles that changed
  async refresh(prio = P_POLL) {
    const hs = this.allHandles(), vs = await this.getMany(hs, prio), changed = [];
    hs.forEach((h, k) => {
      if (this.pending.has(h)) return;              // (the user is turning it: not fought)
      if (this.val.get(h) !== vs[k]) { this.val.set(h, vs[k]); changed.push(h); }
    });
    for (const h of changed) if (this.#engineSide(h)) this.redescribe.add(this.layerOf(h));
    if (changed.length) this.emit('values', changed);
    await this.#redescribe(prio);
    return changed;
  }

  #engineSide(h) { return this.layerOf(h) >= 0 && (this.isEngine(h) || (h & 0xFF) >= this.layout.e0 || this.#toneSide(h)); }

  value(h) { return this.pending.has(h) ? this.pending.get(h) : this.val.get(h); }

  // as a knob: the value at once here, on the device as soon as the link has room (only the latest of each)
  set(h, v) {
    const d = this.d(h);
    if (!d) return;
    if (!this.isEngine(h)) v = Math.max(d.min, Math.min(d.max, Math.round(v)));
    this.pending.set(h, v);
    this.val.set(h, v);
    this.emit('values', [h]);
    this.#pump();
  }

  // one SET in flight at a time, at most one per setGap ms, with the latest value of each handle that moved
  async #pump() {
    if (this.pumping) return;
    this.pumping = true;
    try {
      while (this.pending.size) {
        const gap = this.setGap - (Date.now() - this.lastSet);
        if (gap > 0) await sleep(gap);
        const batch = [...this.pending].slice(0, 12);
        this.lastSet = Date.now();
        let d;
        try {
          d = await this.link.ok(CMD.SET, batch.flatMap(([h, v]) => [...TSX.u16(h), ...TSX.i16(v)]), P_USER, 'SET');
        } catch (e) {
          for (const [h, v] of batch) if (this.pending.get(h) === v) this.pending.delete(h);
          throw e;
        }
        this.lastSet = Date.now();
        const back = [];
        batch.forEach(([h, v], k) => {
          const now = TSX.s16(d, k * 2);
          if (this.pending.get(h) === v) {          // (not moved again meanwhile)
            this.pending.delete(h);
            if (this.val.get(h) !== now) { this.val.set(h, now); back.push(h); }
          }
          if (this.#engineSide(h)) this.redescribe.add(this.layerOf(h));
        });
        if (back.length) this.emit('values', back);
      }
    } catch (e) {
      this.#fail(e);
    } finally {
      this.pumping = false;
      for (const r of this.settled.splice(0)) r();
    }
    if (this.redescribe.size) {                    // once the knob rests a moment
      clearTimeout(this.redTimer);
      this.redTimer = setTimeout(() => {
        if (!this.pumping && !this.pending.size) this.#redescribe(P_DESC).catch((e) => this.#fail(e));
      }, 120);
    }
  }

  // every SET sent and answered, and the layers they touched described again
  async flush() {
    while (this.pending.size || this.pumping) await new Promise((r) => { this.settled.push(r); this.#pump(); });
    clearTimeout(this.redTimer);
    await this.#redescribe(P_USER);
  }

  // a layer's engine knobs described again (their labels and ranges follow the engine, ERS's follow its other knobs);
  // after an engine change, or a label that changed, the layer's values read again (an engine sets its knobs)
  async #redescribe(prio) {
    while (this.redescribe.size) {
      const n = [...this.redescribe][0];
      this.redescribe.delete(n);
      const eng = this.val.get(H.layer(n, TSX.P_ENGINE)), engChanged = this.lastEngine[n] !== eng;
      this.lastEngine[n] = eng;
      let any = engChanged;
      for (const h of [...this.knobHandles(n), ...this.toneKnobs(n)]) if (await this.#describe(h, prio, engChanged)) any = true;
      if (any) {
        const hs = this.layerHandles(n), vs = await this.getMany(hs, prio);
        hs.forEach((h, k) => { if (!this.pending.has(h)) this.val.set(h, vs[k]); });
        this.emit('layer', n);
      }
    }
  }

  // the text the screen shows for h at v: cached, or null and asked for (a 'text' event when it comes)
  text(h, v) {
    const k = `${h}:${v}`;
    if (this.texts.has(k)) return this.texts.get(k);
    const e = this.d(h)?.enum;
    if (e?.has(v)) return e.get(v);
    this.wantText.set(h, v);
    this.#textPump();
    return null;
  }

  async #textPump() {
    if (this.textPumping) return;
    this.textPumping = true;
    try {
      while (this.wantText.size) {
        const [h, v] = [...this.wantText][0];
        this.wantText.delete(h);
        const s = await this.#text(h, v, P_TEXT);
        this.emit('text', h, v, s);
      }
    } catch (e) {
      this.#fail(e);
    } finally {
      this.textPumping = false;
    }
  }

  // --------------------------------------------------------------- polling ---
  start() {
    this.stop();
    const tick = async () => {
      try {
        const s = TSX.parseState(await this.link.ok(CMD.STATE, [], P_POLL, 'STATE'));
        const old = this.state;
        this.state = s;
        if (!old || old.slot !== s.slot || old.edited !== s.edited || old.own !== s.own || old.layer !== s.layer || old.page !== s.page)
          this.emit('state', s);
        if (old && old.slot !== s.slot) await this.listSlots(s.slot, 1, P_POLL);
        const quiet = !this.pending.size && !this.pumping && Date.now() - this.lastSet > this.quietMs;
        if (quiet && (this.seen === undefined || this.seen !== s.counter)) {
          this.seen = s.counter;
          await this.refresh(P_POLL);
        }
      } catch (e) {
        if (!this.#fail(e)) return;
      }
      if (this.timer) this.timer = setTimeout(tick, this.pollMs);
    };
    this.seen = this.state?.counter;
    this.timer = setTimeout(tick, this.pollMs);
  }
  stop() { clearTimeout(this.timer); this.timer = null; }

  #fail(e) {                                       // -> true: carry on
    if (e instanceof LinkError) { this.stop(); this.emit('lost', e); return false; }
    this.emit('error', e);
    return true;
  }

  async readState(prio = P_USER) {
    this.state = TSX.parseState(await this.link.ok(CMD.STATE, [], prio, 'STATE'));
    this.emit('state', this.state);
    return this.state;
  }

  // --------------------------------------------------------- groups, layers ---
  ownGroup(g) { return !!(this.state && (this.state.own >> g & 1)); }
  async group(g, own) {
    await this.flush();
    await this.link.ok(CMD.GROUP, [g, own ? 1 : 0], P_USER, 'GROUP');
    await this.readState();
    await this.refresh(P_USER);
  }

  // a layer's parameters back to their defaults (DESCRIBE's), the engine kept; its knobs to the engine's defaults
  async initLayer(n) {
    for (const h of this.layerHandles(n)) if (!this.isEngine(h) && this.desc.get(h)) this.set(h, this.desc.get(h).def);
    await this.flush();
    await this.refresh(P_USER);
  }

  // layer a's sound into layer b: the engine first (its knobs follow it), then the rest
  async copyLayer(a, b) {
    if (a === b) return;
    await this.flush();
    this.set(H.layer(b, TSX.P_ENGINE), this.val.get(H.layer(a, TSX.P_ENGINE)));
    await this.flush();
    for (let i = 0; i < this.layout.p; i++) if (i !== TSX.P_ENGINE) this.set(H.layer(b, i), this.val.get(H.layer(a, i)));
    await this.flush();
    await this.refresh(P_USER);
  }

  // ---------------------------------------------------------------- presets ---
  async listSlots(first, count, prio = P_USER) {
    for (let k = first; k < first + count; k += 4) {
      const n = Math.min(4, first + count - k);
      for (const s of TSX.parseList(await this.link.ok(CMD.LIST, [k, n], prio, 'LIST'), k)) this.list[s.slot] = s;
    }
    this.emit('list', this.list);
    return this.list;
  }
  listAll() { return this.listSlots(0, this.info.slots); }
  kind(slot) { const s = this.list[slot]; return s?.user ? 'user' : slot < this.info.factory ? 'factory' : 'empty'; }
  // the first slot holding nothing (no user preset, no factory one), else the first without a user preset; -1: none
  nextFree(from = 0) {
    const n = this.info.slots, free = (pred) => { for (let i = 0; i < n; i++) { const k = (from + i) % n; if (pred(k)) return k; } return -1; };
    const e = free((k) => this.kind(k) === 'empty');
    return e >= 0 ? e : free((k) => this.kind(k) !== 'user');
  }

  async load(slot) {
    await this.flush();
    await this.link.ok(CMD.LOAD, [slot], P_USER, 'LOAD');
    await this.readState();
    this.seen = this.state.counter;
    await this.refresh(P_USER);
  }

  static name(s) {                                 // a name as the device takes it, or throws
    const n = String(s ?? '').toUpperCase().replace(/\s+$/, '');
    if (!TSX.NAME_OK.test(n)) throw new Error('a name: up to 12 of A-Z 0-9 space - . /');
    return n;
  }

  // what plays into the slot; name: null keeps the preset's name
  async save(slot, name = null) {
    const nm = name === null ? '' : Editor.name(name);
    if (name !== null && !nm) throw new Error('the name is empty');
    await this.flush();
    await this.link.ok(CMD.SAVE, [slot, ...TSX.nameBytes(nm)], P_USER, 'SAVE');
    await this.listSlots(slot, 1);
    await this.readState();
  }

  async rename(slot, name) {
    const nm = Editor.name(name);
    if (!nm) throw new Error('the name is empty');
    const r = await this.link.request(CMD.RENAME, [slot, ...TSX.nameBytes(nm)], P_USER);
    if (r.status === 'EMPTY') throw new Error(`slot ${slot + 1} holds nothing saved: save it first`);
    if (r.status !== 'OK') throw new Error(`RENAME: the FM-1 says ${r.status}`);
    await this.listSlots(slot, 1);
  }

  async remove(slot) {
    await this.link.ok(CMD.DELETE, [slot], P_USER, 'DELETE');
    await this.listSlots(slot, 1);
    await this.readState();
  }

  // a slot's record (512 B), or null: nothing saved there (a factory preset or INIT)
  async read(slot) {
    const rec = [];
    for (let at = 0; at < TSX.SLOT_BYTES; at += TSX.CHUNK) {
      const r = await this.link.request(CMD.READ, [slot, ...TSX.u16(at), TSX.CHUNK], P_USER);
      if (r.status === 'EMPTY') return null;
      if (r.status !== 'OK') throw new Error(`READ slot ${slot + 1}: the FM-1 says ${r.status}`);
      rec.push(...r.data);
    }
    if (!TSX.recordGood(rec)) throw new Error(`slot ${slot + 1}: the record read has a bad CRC (try again)`);
    return rec;
  }

  async write(slot, rec) {
    if (!TSX.recordGood(rec)) throw new Error('the record is damaged');
    await this.link.ok(CMD.WBEGIN, [slot], P_USER, 'WRITE BEGIN');
    for (let at = 0; at < TSX.SLOT_BYTES; at += TSX.CHUNK)
      await this.link.ok(CMD.WDATA, [...TSX.u16(at), ...rec.slice(at, at + TSX.CHUNK)], P_USER, 'WRITE DATA');
    await this.link.ok(CMD.WCOMMIT, TSX.u32(TSX.crc32(rec)), P_USER, `WRITE slot ${slot + 1}`);
    await this.listSlots(slot, 1);
    await this.readState();
  }

  // slot a into slot b. A user preset goes as its record; a factory one (nothing saved: no record) by LOAD and SAVE,
  // so b plays afterwards and what played before (edits too) is left
  async copy(a, b) {
    if (a === b) return;
    const rec = await this.read(a);
    if (rec) return this.write(b, rec);
    await this.load(a);
    await this.save(b, null);
  }

  // the user's presets (or those slots) as a tools/presets.py backup (JSON text)
  async backup(slots = null, progress = () => {}) {
    const want = slots ?? this.list.filter((s) => s?.user).map((s) => s.slot), out = [];
    for (const [i, k] of want.entries()) {
      progress(i, want.length, k);
      const rec = await this.read(k);
      if (rec) out.push({ slot: k + 1, name: TSX.recordName(rec), record: rec });
    }
    return TSX.backupJSON(out);
  }

  // a backup's presets into their slots (or `to` [slot index per preset]); -> what was written
  async restore(parsed, to = null, progress = () => {}) {
    const done = [];
    for (const [i, p] of parsed.presets.entries()) {
      const k = to ? to[i] : p.slot - 1;
      if (k === null || k === undefined || p.bad) continue;
      if (k < 0 || k >= this.info.slots) throw new Error(`slot ${k + 1}: 1..${this.info.slots}`);
      progress(i, parsed.presets.length, k);
      await this.write(k, p.record);
      done.push({ slot: k, name: p.name });
    }
    return done;
  }
}
