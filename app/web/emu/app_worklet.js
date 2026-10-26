// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// typhoon's part of fm1-base's emulator worklet (base/web/emu/worklet.js loads it first): MIDI in from the page (a
// Web MIDI input; the breath slider, on GLOBAL > BREATH CC: app.js), and the presets, which live in the data area
// (app/web.c) rather than in fm1-base's store objects: asked of the page once the firmware is up, written back and
// rescanned; sent to the page whenever they change (checked about once a second of audio).
const PORT_USB = 0;                                   // (base/include/plat.h PORT_*: what a Web MIDI input is)

globalThis.fm1App = {
  load(fm1) {
    this.hash = fm1.ex.web_presets_hash();          // (the empty store: not worth keeping)
    this.frames = 0;
    fm1.port.postMessage({ type: "app", cmd: "presets?" });
  },
  message(fm1, m) {
    const ex = fm1.ex;
    if (m.cmd === "midi") {                         // a channel message: [status, data1, data2]
      const [st, d1 = 0, d2 = 0] = m.d;
      if (st < 0x80 || st >= 0xf0) return;
      ex.web_midi_in(PORT_USB, ((st >> 4) | st << 8 | (d1 & 0x7f) << 16 | (d2 & 0x7f) << 24) >>> 0);
    } else if (m.cmd === "breath") {                // the page's slider: on the BREATH CC the firmware is set to
      const cc = ex.web_breath_cc(), v = m.v & 0x7f;
      this.message(fm1, { cmd: "midi", d: cc >= 128 ? [0xd0, v] : [0xb0, cc, v] });
    } else if (m.cmd === "presets" && m.bytes) {    // the saved presets, back into the data area
      const b = new Uint8Array(m.bytes), n = ex.web_presets_len();
      if (b.length !== n) return;                   // (another layout: left out)
      new Uint8Array(fm1.mem.buffer, ex.web_presets(), n).set(b);
      ex.web_presets_rescan();
      this.hash = ex.web_presets_hash();
    }
  },
  process(fm1) {
    if ((this.frames += 128) < 44100) return;
    this.frames = 0;
    const ex = fm1.ex, h = ex.web_presets_hash();
    if (h === this.hash) return;
    this.hash = h;
    const bytes = new Uint8Array(fm1.mem.buffer, ex.web_presets(), ex.web_presets_len()).slice();
    fm1.port.postMessage({ type: "app", cmd: "presets", bytes }, [bytes.buffer]);
  },
};
