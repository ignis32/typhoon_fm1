// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// typhoon's part of fm1-base's emulator page (base/web/emu/index.html imports it when switched on): typhoon is a
// sound module played by breath, so the page gets a breath slider (on GLOBAL > BREATH CC, CC11 by default) and a Web MIDI input (a breath controller,
// a keyboard), both into the firmware as USB MIDI (app_worklet.js); and the presets, which fm1-base's page does not
// keep (they are in the data area: app/web.c), kept here in localStorage, given back when the firmware asks.
const PRESETS_KEY = "typhoon-emu-presets", MIDI_KEY = "typhoon-emu-midi-in";
const get = (k) => { try { return localStorage.getItem(k); } catch { return null; } };
const put = (k, v) => { try { if (v === null) localStorage.removeItem(k); else localStorage.setItem(k, v); } catch { /* private window */ } };
const toB64 = (u8) => { let s = ""; for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000)); return btoa(s); };
const fromB64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));

let node = null;
const send = (d) => node && node.port.postMessage({ type: "app", cmd: "midi", d });

function panel(status) {
  const box = document.createElement("section");
  box.className = "speak";
  box.setAttribute("aria-labelledby", "typhoon-h");
  box.innerHTML = `<h2 id="typhoon-h" style="font: 600 1.1rem 'Barlow Semi Condensed', 'Barlow', sans-serif; margin: 0 0 .6rem">Breath and MIDI</h2>
<div class="row" style="margin: 0">
  <label style="display: inline-flex; gap: .6rem; align-items: center">Breath
    <input type="range" id="typhoon-breath" min="0" max="127" value="0" style="width: min(60vw, 320px); accent-color: var(--teal)">
    <output id="typhoon-breath-v" for="typhoon-breath" style="font-variant-numeric: tabular-nums; min-width: 2.5em">0</output></label>
  <label style="display: inline-flex; gap: .6rem; align-items: center">MIDI in
    <select id="typhoon-midi" style="font: inherit; color: var(--ink); background: var(--card); border: 1px solid var(--muted); border-radius: 8px; padding: .25em .5em; max-width: 60vw">
      <option value="">None</option></select></label>
</div>
<p class="note">Breath is typhoon's loudness and tone: hold a key (or play one from the MIDI input) and move the
breath (it goes out on GLOBAL > BREATH CC). A breath controller or any MIDI keyboard on this computer plays it
through Web MIDI.</p>`;
  document.getElementById("speak").before(box);

  const breath = box.querySelector("#typhoon-breath"), out = box.querySelector("#typhoon-breath-v");
  breath.addEventListener("input", () => { out.textContent = breath.value; node && node.port.postMessage({ type: "app", cmd: "breath", v: +breath.value }); });

  const sel = box.querySelector("#typhoon-midi");
  if (!navigator.requestMIDIAccess) {
    sel.disabled = true;
    sel.options[0].textContent = "No Web MIDI in this browser";
    return;
  }
  let input = null;
  const use = (id, inputs) => {
    if (input) input.onmidimessage = null;
    input = inputs.get(id) || null;
    if (input) input.onmidimessage = (e) => { if (e.data[0] >= 0x80 && e.data[0] < 0xf0) send(Array.from(e.data)); };
    put(MIDI_KEY, input ? input.name : null);
  };
  navigator.requestMIDIAccess({ sysex: false }).then((acc) => {
    const fill = () => {
      const want = input ? input.id : null, saved = get(MIDI_KEY);
      sel.length = 1;
      for (const i of acc.inputs.values()) sel.add(new Option(i.name, i.id));
      const again = [...acc.inputs.values()].find((i) => i.id === want || (!want && i.name === saved));
      sel.value = again ? again.id : "";
      use(sel.value, acc.inputs);
    };
    fill();
    acc.onstatechange = fill;
    sel.addEventListener("change", () => use(sel.value, acc.inputs));
  }).catch(() => {
    sel.disabled = true;
    sel.options[0].textContent = "MIDI not allowed";
    status("Web MIDI was not allowed: the breath slider and the panel still play.");
  });
}

export function attach(p) {
  node = p.node;
  panel(p.status);
  // fm1-base's Factory reset forgets its store on the second press: typhoon's presets go with it
  let presses = 0;
  const reset = document.getElementById("reset");
  if (reset) reset.addEventListener("click", () => { if (++presses >= 2) put(PRESETS_KEY, null); });
}

export function message(m) {
  if (m.cmd === "presets?") {                        // the firmware is up: the saved presets, if any
    const s = get(PRESETS_KEY);
    if (!s) return;
    const bytes = fromB64(s);
    node.port.postMessage({ type: "app", cmd: "presets", bytes }, [bytes.buffer]);
  } else if (m.cmd === "presets" && m.bytes) {       // a save changed them
    put(PRESETS_KEY, toB64(new Uint8Array(m.bytes)));
  }
}
