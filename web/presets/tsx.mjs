// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// typhoon's preset SysEx protocol (firmware/src/core/sysex.c has the whole description): frames and their
// parts, for the browser (WebMIDI: web/editor/) and node (tests/sim_test.mjs). tools/presets.py is the same in Python.
//   F0 7D 'T' 'R' 01 cmd seq <data: 8-bit bytes as 7-bit, LSB first> sum F7   (sum: 7D .. sum = 0 mod 128)
export const VER = 1;
export const CMD = { INFO: 0, LIST: 1, READ: 2, WBEGIN: 3, WDATA: 4, WCOMMIT: 5, DELETE: 6, RENAME: 7, STATS: 8,
  LAYOUT: 9, DESCRIBE: 10, GET: 11, SET: 12, TEXT: 13, ENGINES: 14, GROUP: 15, LOAD: 16, SAVE: 17, STATE: 18 };
// the editor's parameter handles (core/params.c) and 16-bit values
export const H = { layer: (n, i) => (n << 8) | i, snd: (i) => 0x4000 | i, sndGlobal: (i) => 0xC000 | i, global: (i) => 0x8000 | i };
export const s16 = (d, at) => { const v = d[at] | (d[at + 1] << 8); return v & 0x8000 ? v - 0x10000 : v; };
export const i16 = (v) => [v & 255, (v >> 8) & 255];
// a DESCRIBE reply's data -> {fmt, min, max, def, names, group, label, unit}
export const describe = (d) => ({ fmt: d[0], min: s16(d, 1), max: s16(d, 3), def: s16(d, 5), names: d[7],
  group: d[8] === 255 ? null : d[8], label: nameOf(d.slice(9, 21)), unit: nameOf(d.slice(21, 25)) });
export const STATUS = ['OK', 'EMPTY', 'BUSY', 'BAD', 'FAIL', 'CRC'];
export const SLOT_BYTES = 512, NAME_BYTES = 12, CHUNK = 64;

export function pack(bytes) {
  const out = [];
  let value = 0, bits = 0;
  for (const b of bytes) {
    value |= b << bits; bits += 8;
    while (bits >= 7) { out.push(value & 127); value >>= 7; bits -= 7; }
  }
  if (bits) out.push(value & 127);
  return out;
}

export function unpack(seven) {
  const out = [];
  let value = 0, bits = 0;
  for (const b of seven) {
    value |= b << bits; bits += 7;
    if (bits >= 8) { out.push(value & 255); value >>= 8; bits -= 8; }
  }
  return value || bits >= 7 ? null : out;
}

// a request: the whole message, F0 .. F7
export function frame(cmd, seq, data = []) {
  const body = [0x7D, 0x54, 0x52, VER, cmd, seq & 127, ...pack(data)];
  const sum = body.reduce((a, b) => a + b, 0);
  return Uint8Array.from([0xF0, ...body, (128 - (sum & 127)) & 127, 0xF7]);
}

// a reply (F0 .. F7) -> {cmd, seq, status, data} (status a name), or null when it is not one of ours
export function parse(msg) {
  const m = Array.from(msg);
  if (m.length < 10 || m[0] !== 0xF0 || m[m.length - 1] !== 0xF7 || m[1] !== 0x7D || m[2] !== 0x54 || m[3] !== 0x52 ||
      m[4] !== VER || !(m[5] & 0x40)) return null;
  if (m.slice(1, -1).reduce((a, b) => a + b, 0) & 127) return null;
  const d = unpack(m.slice(7, -2));
  if (!d || !d.length) return null;
  return { cmd: m[5] & 0x3F, seq: m[6], status: STATUS[d[0]] ?? `?${d[0]}`, data: d.slice(1) };
}

export function crc32(bytes) {
  let c = 0xFFFFFFFF;
  for (const b of bytes) { c ^= b; for (let k = 0; k < 8; k++) c = c & 1 ? (c >>> 1) ^ 0xEDB88320 : c >>> 1; }
  return (~c) >>> 0;
}

export const u16 = (v) => [v & 255, (v >> 8) & 255];
export const u32 = (v) => [v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255];
export const nameBytes = (s) => Array.from({ length: NAME_BYTES }, (_, i) => (i < s.length ? s.charCodeAt(i) & 127 : 0));
export const nameOf = (bytes) => String.fromCharCode(...bytes.slice(0, bytes.indexOf(0) < 0 ? bytes.length : bytes.indexOf(0)));

// --- the editor's view of the firmware (web/editor/core.mjs); tests/sim/webeditor.test.mjs checks each against the C
// the formats (engines/felucca_engine.h F_*), in their order
export const FMT = ['INT', 'PCT', 'BIPCT', 'TIME', 'LFOHZ', 'CUTOFF', 'DB', 'SEMI', 'ENUM', 'BPM', 'NOTE', 'ONOFF', 'OCT', 'STEPS'];
export const F = Object.fromEntries(FMT.map((n, i) => [n, i]));
export const P_ENGINE = 0;           // a layer's first parameter is its engine (core/layers.h), by stable id both ways
export const ENG_OFF = 0;            // engines.h ENG_ID_OFF: not in ENGINES' list
// the sound's groups (core/layers.h GR_*), named as the device's pages are; a new one shows as GROUP n
export const GROUPS = ['CURVE', 'BREATH LP', 'DRIVE', 'DELAY', 'REVERB', 'DETUNE', 'VOLUME'];
export const groupName = (g) => GROUPS[g] ?? `GROUP ${g + 1}`;
// a slot's name: up to 12 of these (the device's SAVE AS)
export const NAME_OK = /^[A-Z0-9 .\/-]{0,12}$/;

// replies' data (after the status) -> objects
export const parseInfo = (d) => ({ ver: d[0], slots: d[1], record: d[2] | (d[3] << 8), cur: d[4], edited: d[5],
  factory: d[6], name: nameOf(d.slice(7, 15)), version: d.length >= 23 ? nameOf(d.slice(15, 23)) : '?' });
export const parseLayout = (d) => ({ layers: d[0], p: d[1], s: d[2], g: d[3], groups: d[4], engines: d[5], e0: d[6], layer: d[7] });
export const parseState = (d) => ({ counter: (d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24)) >>> 0, slot: d[4],
  edited: d[5], own: d[6], layer: d[7], page: d[8] });
export const parseList = (d, first) => Array.from({ length: d.length / 13 }, (_, k) =>
  ({ slot: first + k, user: !!d[k * 13], name: nameOf(d.slice(k * 13 + 1, k * 13 + 13)) }));
export const parseEngines = (d) => ({ count: d[0], list: Array.from({ length: (d.length - 1) / 9 }, (_, k) =>
  ({ id: d[1 + k * 9], name: nameOf(d.slice(2 + k * 9, 10 + k * 9)) })) });

// a slot's record (core/presets.c ps_rec_t): CRC-32 of bytes 8.. at 4..7, the name at 16..27
export const recordGood = (rec) => rec.length === SLOT_BYTES &&
  crc32(rec.slice(8)) === ((rec[4] | (rec[5] << 8) | (rec[6] << 16) | (rec[7] << 24)) >>> 0);
export const recordName = (rec) => nameOf(rec.slice(16, 28));
export const hex = (bytes) => Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join('');
export const unhex = (s) => (/^([0-9a-f]{2})*$/i.test(s) ? s.match(/../g)?.map((h) => parseInt(h, 16)) ?? [] : null);

// a backup: tools/presets.py's JSON, both ways. presets: [{slot (1..96), name, record (bytes)}]
export function backupJSON(presets, taken = new Date()) {
  const p2 = (n) => String(n).padStart(2, '0');
  const t = `${taken.getFullYear()}-${p2(taken.getMonth() + 1)}-${p2(taken.getDate())} ${p2(taken.getHours())}:${p2(taken.getMinutes())}:${p2(taken.getSeconds())}`;
  return JSON.stringify({ format: 'typhoon-presets', version: 1, taken: t,
    presets: presets.map((p) => ({ slot: p.slot, name: p.name, record: hex(p.record) })) }, null, 1) + '\n';
}
// -> {taken, presets: [{slot, name, record, bad}]} (bad: why the record cannot be written), or throws
export function backupParse(text) {
  const j = JSON.parse(text);
  if (!j || !['typhoon-presets', 'tornado-presets'].includes(j.format) || !Array.isArray(j.presets))   // (tornado: typhoon's earlier name)
    throw new Error('not a typhoon presets backup');
  return { taken: j.taken ?? '', presets: j.presets.map((p) => {
    const record = typeof p.record === 'string' ? unhex(p.record) : null;
    const bad = !record ? 'no record' : !recordGood(record) ? 'damaged record' :
      !(Number.isInteger(p.slot) && p.slot >= 1) ? 'no slot' : null;
    return { slot: p.slot, name: p.name ?? (record ? recordName(record) : ''), record, bad };
  }) };
}
