// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// A MIDI access (as navigator.requestMIDIAccess gives) whose one port is the firmware compiled to wasm (web/sim's
// typhoon.wasm, no sound): the editor page with ?sim, to try it without an FM-1, and the page's own tests.
//   const midi = await simMidi(url or bytes)   midi.inputs / midi.outputs: one port each; midi.ex: the sim's exports
export async function simMidi(wasm) {
  const bytes = typeof wasm === 'string' ? await (await fetch(wasm)).arrayBuffer() : wasm;
  // the sim is a wasm32-wasi reactor; its one WASI call is the clock (plat_host.c times each block for the CPU load)
  let mem = null;
  const wasi = {
    clock_time_get: (id, precision, out) => {
      new DataView(mem.buffer).setBigUint64(out, BigInt(Math.round(performance.now() * 1e6)), true);
      return 0;
    },
  };
  const { instance } = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: wasi });
  const ex = instance.exports;
  mem = ex.memory;
  ex._initialize?.();
  if (ex.sim_init(44100) !== 0) throw new Error('the sim did not boot');
  // ms of the firmware's own clock (sim_ms): the frames may come from a block rendered before, with no time passing
  const render = (ms) => { const t0 = ex.sim_ms(); do ex.sim_render(128); while (ex.sim_ms() < t0 + ms); };
  const name = 'typhoon sim (no FM-1)';
  const input = { id: 'sim-in', name, type: 'input', state: 'connected', onmidimessage: null, open: async () => input };
  const output = {
    id: 'sim-out', name, type: 'output', state: 'connected', open: async () => output,
    send(msg) {
      const m = Uint8Array.from(msg);
      if (m[0] !== 0xF0) { ex.sim_midi(m[0], m[1] ?? 0, m[2] ?? 0); return; }
      new Uint8Array(ex.memory.buffer, ex.sim_sx_in_buf(), m.length - 2).set(m.subarray(1, -1));
      ex.sim_sx_in(m.length - 2);
      render(3);
      const n = ex.sim_sx_out_n();
      if (!n) return;
      const reply = new Uint8Array(ex.memory.buffer, ex.sim_sx_out_buf(), n).slice();
      setTimeout(() => input.onmidimessage?.({ data: reply }), 1);
    },
  };
  const clock = setInterval(() => render(20), 20);   // (time goes on: a note's tail, a write waiting for silence)
  return { inputs: new Map([[input.id, input]]), outputs: new Map([[output.id, output]]), onstatechange: null, sysexEnabled: true,
    ex, stop: () => clearInterval(clock) };
}
