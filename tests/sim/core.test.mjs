// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 ignis32
// The core: the counts the tests read agree with the firmware's; notes from MIDI and the keys, the breath bar on the
// key LEDs, every engine answers breath, legato, mute, an OFF layer, the breath curve's head start, the envelope.
export default function (t) {
  const { ex, C, L1, L2, SG, check, run } = t;
  const counts = [C.P_COUNT, C.S_COUNT, C.G_COUNT, C.NPAGES, C.NGROUP], firmware = [0, 1, 2, 3, 4].map((w) => ex.sim_count(w));
  check(counts.join() === firmware.join(), `tools/consts.mjs reads the enums as the firmware has them (${counts.join()})`);
  check(ex.sim_preset_cur() === 0, 'it starts on the first factory preset');
  ex.sim_preset(255);                                // INIT (one sine): the checks below are about the core
  check(run(500) < 1e-4, 'silent before any note');
  ex.sim_midi(0x90, 60, 100);
  check(run(300) > 0.01, 'a MIDI note sounds');
  ex.sim_midi(0x80, 60, 0);
  run(500);
  ex.sim_key(7, 1);
  check(run(300) > 0.01, 'a key sounds');
  ex.sim_key(7, 0);
  run(300);

  const bits = (v) => { let n = 0; for (; v; v >>>= 1) n += v & 1; return n; };
  for (const [cc, lo, hi] of [[0, 0, 0], [64, 12, 15], [127, 27, 27]]) {   // (the curve: LOW 8, HIGH 120, linear)
    ex.sim_midi(0xB0, 2, cc);
    run(250);                                       // (the smoothing settles)
    const n = bits(ex.sim_leds_keys());
    check(n >= lo && n <= hi && bits(ex.sim_glow_keys()) === 27, `breath ${cc}: ${lo}..${hi} key LEDs lit, 27 glowing (got ${n}, ${bits(ex.sim_glow_keys())})`);
  }
  run(1600);
  check(bits(ex.sim_glow_keys()) === 0, 'the bar goes away 1.5 s after the breath stops');

  // BREATH CC (GLO, BREATH IN): CC11 by default (the tests set CC2: lib.mjs); another CC, or channel aftertouch (128,
  // "AT"), and CC2 is not breath then
  {
    const { G } = t;
    ex.sim_preset(255);
    ex.sim_par_set(SG(C.S_REV_MIX), 0);
    ex.sim_par_set(L1(C.P_BR_AMP), 127);           // (loudness from breath only)
    const level = (st, d1, d2) => { ex.sim_midi(st, d1, d2); run(300); return run(150); };
    ex.sim_midi(0x90, 69, 80);
    const def = t.breathCcDefault, cc2 = level(0xB0, 2, 110);
    level(0xB0, 2, 0);
    ex.sim_par_set(G(C.G_BREATH_CC), 11);
    const cc2b = level(0xB0, 2, 110), cc11 = level(0xB0, 11, 110);
    level(0xB0, 11, 0);
    ex.sim_par_set(G(C.G_BREATH_CC), 128);
    const at = level(0xD0, 110, 0);
    level(0xD0, 0, 0);
    ex.sim_midi(0x80, 69, 0); run(800);
    ex.sim_par_set(G(C.G_BREATH_CC), 2);
    check(def === 11 && cc2 > 0.05 && cc2b < 1e-3 && cc11 > 0.05 && at > 0.05,
      `BREATH CC: 11 by default (${def}), CC2 when set (${cc2.toFixed(3)}); on 11 CC2 is no breath (${cc2b.toFixed(4)}), CC11 is (${cc11.toFixed(3)}); AT: aftertouch (${at.toFixed(3)})`);
  }

  // every engine of the build sounds on layer 1 with breath, and more breath is louder
  for (let e = 0; e < ex.sim_nengines(); e++) {
    ex.sim_par_set(L1(C.P_ENGINE), e);
    ex.sim_par_set(L1(C.P_BR_AMP), 127);            // (the loudness follows breath; the default is 0)
    ex.sim_midi(0xB0, 2, 0);
    run(300);
    ex.sim_midi(0xB0, 2, 30); ex.sim_midi(0x90, 60, 80);
    run(200);                                       // (each level measured once the smoothing has settled)
    const soft = run(300);
    ex.sim_midi(0xB0, 2, 120);
    run(200);
    const loud = run(300);
    t.off(60, 600);
    check(soft > 0.002 && loud > soft * 1.5, `engine ${e}: sounds, and louder with more breath (peak ${soft.toFixed(3)} -> ${loud.toFixed(3)})`);
  }

  // legato (as the re.corder slurs: the new note-on before the old note-off): one note sounds throughout
  ex.sim_par_set(L1(C.P_ENGINE), 0);
  t.note(60, 200);
  ex.sim_midi(0x90, 62, 80); ex.sim_midi(0x80, 60, 0);
  const slur = run(200);
  check(ex.sim_note() === 62 && slur > 0.01, `legato: 60 -> 62 keeps sounding (note ${ex.sim_note()}, peak ${slur.toFixed(3)})`);
  t.off(62, 5000);                                  // (the reverb's tail too)
  check(ex.sim_note() === -1 && run(200) < 1e-3, 'silent after the last note-off');

  // mute: the layer fades out during a held note, and comes straight back unmuted
  t.note(64, 300);
  ex.sim_par_set(L1(C.P_MUTE), 1);
  run(1500);
  const muted = run(200);
  ex.sim_par_set(L1(C.P_MUTE), 0);
  run(150);
  const back = run(200);
  check(muted < 1e-3 && back > 0.01, `mute: silent while muted (${muted.toFixed(4)}), back when unmuted mid-note (${back.toFixed(3)})`);
  t.off(64, 800);

  // an OFF layer (the engine knob's last place): silent, its settings kept; an engine again: it plays
  ex.sim_preset(255);
  t.note(69, 300);
  const on = run(200);
  ex.sim_par_set(L1(C.P_ENGINE), ex.sim_nengines());   // OFF
  run(5000); const off = run(200);                  // (after the reverb's tail: PLATE at DECAY 70 rings ~4 s)
  const level = ex.sim_par_get(L1(C.P_LEVEL));
  ex.sim_par_set(L1(C.P_ENGINE), 0);                // the first engine again: a fresh note brings it in
  ex.sim_midi(0x80, 69, 0); run(300); ex.sim_midi(0x90, 69, 80); run(200);
  const again = run(200);
  t.off(69, 800);
  check(on > 0.01 && off < 1e-3 && level === 100 && again > 0.01,
    `OFF layer: silent (${on.toFixed(3)} -> ${off.toFixed(4)}), level kept (${level}), back with an engine (${again.toFixed(3)})`);

  // the breath curve's LOW below 0: a head start (a held note sounds with no breath at all)
  ex.sim_preset(255);
  ex.sim_par_set(L1(C.P_BR_AMP), 127);              // (loudness from breath, to hear the head start)
  ex.sim_par_set(SG(C.S_REV_MIX), 0);
  ex.sim_midi(0xB0, 2, 0); ex.sim_midi(0x90, 69, 80); run(400);
  const none = run(200);
  ex.sim_par_set(SG(C.S_BC_LO), -40);
  run(400);
  const head = run(200);
  ex.sim_midi(0x80, 69, 0); run(800);
  const after = run(200);
  ex.sim_par_set(SG(C.S_BC_LO), 8);
  check(none < 1e-3 && head > 0.02 && after < 1e-3, `LOW -40: a held note with no breath sounds (${none.toFixed(4)} -> ${head.toFixed(3)}), silent after it (${after.toFixed(4)})`);

  // the volume envelope: DELAY, ATTACK, DECAY to SUSTAIN; it restarts on a fresh attack, not on legato
  ex.sim_preset(255);
  ex.sim_par_set(SG(C.S_REV_MIX), 0);
  ex.sim_par_set(L1(C.P_DEC), 64); ex.sim_par_set(L1(C.P_SUS), 0);        // layer 1: a chiff (100 ms decay, no sustain)
  ex.sim_par_set(L2(C.P_ENGINE), 0); ex.sim_par_set(L2(C.P_LEVEL), 100); ex.sim_par_set(L2(C.P_DELAY), 80);   // layer 2: in after 250 ms
  ex.sim_midi(0xB0, 2, 100); ex.sim_midi(0x90, 60, 80);
  run(30); const chiff0 = ex.sim_layer_amp(0), late0 = ex.sim_layer_amp(1);
  run(570); const chiff1 = ex.sim_layer_amp(0), late1 = ex.sim_layer_amp(1);
  ex.sim_midi(0x90, 62, 80); ex.sim_midi(0x80, 60, 0); run(30);          // a slur: nothing restarts
  const slur0 = ex.sim_layer_amp(0), slur1 = ex.sim_layer_amp(1);
  ex.sim_midi(0x80, 62, 0); run(400);
  ex.sim_midi(0x90, 64, 80); run(30);                                    // a fresh attack: the chiff again, the late one waits
  const again0 = ex.sim_layer_amp(0), again1 = ex.sim_layer_amp(1);
  t.off(64, 600);
  check(chiff0 > 20000 && chiff1 < 500 && late0 < 100 && late1 > 20000 && slur0 < 500 && slur1 > 20000 && again0 > 20000 && again1 < 100,
    `envelope: chiff ${chiff0} -> ${chiff1}, delayed layer ${late0} -> ${late1}, legato keeps them (${slur0}, ${slur1}), a fresh attack restarts (${again0}, ${again1})`);
}
