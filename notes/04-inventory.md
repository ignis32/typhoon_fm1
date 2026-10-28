# Inventory: every fixed number that shapes the sound (2026-10-28)

The rule (2026-10-28): every number that shapes the sound or how it responds is a **knob**, or a **named constant in
a block at the top of its file**, with a comment saying what and why. Anything that acts *in time while you play*
(an onset, a kick, a glide, a rate, a dip) is the first kind to watch; a delay on the response is a knob, 0 in the
presets (notes/03-lag-watch.md). This file is the map: where each part stands, and what is left inline.

Columns: **what it does while you play**; **value**; **where** (knob, named constant, or inline); **next**:
*knob?* (worth a knob: your call), *name* (move into its file's block, no sound change), *ok*.

## Status by part

| Part | Its numbers | Block |
|---|---|---|
| EVA | all named, each with why | `engines/eva/eng_eva.c` "EVA's fixed numbers" |
| layer: QUIRKS | named | `core/layers.c` QUIRK_* |
| breath LP (ERS, FM) | named / knobs | `dsp/breathlp.c` BLP_KNOBS |
| shared DSP (float) | named | `dsp/dspf.c` |
| ERS | partly: SAX's tuned set (SAX_*), the two lag ones (ERS_FREE_SWING, ERS_FLUE_WW); the rest inline | `ers.h`, `ers_bores.c` |
| FM, ANALOG, TONE, effects, LEAPS, reverbs, the layer's envelope | inline (commented, not in blocks) | to name |

## Acting in time while you play (the ones to see)

| Part | What | Value | Where | Next |
|---|---|---|---|---|
| EVA | vibrato / tremolo only above a breath | 55 % | EVA_VIB_ONSET | *knob?* (VIB ONSET) |
| EVA | vibrato faster blown harder | +1.6 Hz at full | EVA_VIB_HZ_BR | ok |
| EVA | SWEEP's kick on a tongued note decays in | 80 ms | EVA_SWEEP | *knob?* (SWEEP TIME) |
| EVA | CHIFF's burst lasts | 30 ms | EVA_CHIFF | ok (noise layers do more) |
| EVA | HARMONIC's count glides | ~3.6 ms | EVA_HARM_GLIDE | ok (a de-zipper) |
| EVA | LAG, VIB SWELL | knobs | — | done |
| ERS | vibrato only above a pressure | 55 % of a full tone (9011 .. 7373 in eng_ers.c) | inline | name; *knob?* (VIB ONSET, as EVA's) |
| ERS | vibrato rate | 3 .. 8 Hz (RATE) | inline | name |
| ERS | TONGUE's kick decays in | 40 ms (16088 a block) | inline | name; *knob?* |
| ERS | JET: a chiff on each tongued note | 40 ms (1764 samples) | inline | name |
| ERS | DRIFT: the pitch wanders to a new place every | ~0.4 s, up to 8 cents at 127 | inline | name |
| ERS | PR>PITCH: pitch with pressure | +-30 cents at a full tone | inline | name |
| ERS | GROWL's flutter | ~20 .. 40 Hz, down to 1/2 | inline | name |
| ERS | FREE REED's swing builds up | ~8 ms | ERS_FREE_SWING | done (kept) |
| ERS | FLUE windway | ~0.3 ms | ERS_FLUE_WW | done |
| ERS | SAX: on a slur the pressure dips, back in | SAX_DIP, ~45 ms | SAX_* block | ok (named; in the lag watch) |
| ERS | FREE REED blown harder goes flat | ~7 cents | inline | name |
| ERS | reeds / lips: how much breath the reed needs to start | 15 % of the breath | inline (ers_valve.c) | name |
| ERS | JET: blown from just over the threshold | ~0.8 of a full tone | inline (ers_bores.c) | name |
| FM | index glides over a block | 0.7 ms | inline | ok (a de-zipper) |
| FM | KEYTRK: the index halves an octave and a half above C4 | C4, 1.5 oct | inline | name |
| TONE SWEEP | opens by breath's speed, settles in | ~1 s .. 60 ms (SPEED) | inline | name |
| DELAY | a new TIME glides in | 64 samples a block | inline | name |
| DRIVE | comes in past DRV AT | knob | — | ok |
| LEAPS | a big slurred jump waits | up to WAIT ms | knobs | ok |
| layer | SMOOTH, ENVELOPE, QUIRKS | knobs | — | ok |

## Fixed design choices (scales, curves, ranges, levels)

| Part | What | Value | Where | Next |
|---|---|---|---|---|
| EVA | the breath curve to the cutoff, range, S.E.D.'s law, resonance range, levels | (each in the block) | EVA block | ok |
| ERS | the bores, valves, resonators, shapers: the physics, each a measured or STK value | many | inline (commented) | name the ones above; the physics can stay commented inline |
| ERS | BODY's resonances 250 / 700 / 2200 Hz, WAH 400 Hz .. 5 kHz, TILT around 800 Hz, the bell below ~300 Hz | | inline | name |
| FM | INDEX 127 = one cycle of phase; AIR's scale; FEEDBK's; out at 3/4 | | inline | name |
| FM | RATIO's eight choices 0.5 .. 7 | | table | ok |
| ANALOG | DTN in 3/16 semitone steps; DRV x1 .. x3; NOIS x200; the soft knee at 16000; RES's damping 2 .. 0.14; KTR around C4 | | inline (Felucca-Engines' code) | name |
| TONE FILTER | RES's damping 1.76 .. 0.13 | | inline | name |
| TONE TALK | the five vowels' three formants (Peterson & Barney), their gains 1 / 0.61 / 0.34 | | tables | ok |
| TONE SUB OCT | LEVEL x (1 - BREATH + BREATH x breath) | | inline | ok |
| DRIVE | x1 .. x7 at full breath, the clipper at 3/4 | | inline | name |
| DELAY | FEEDBK up to ~0.97, its TONE a one-pole 2000 .. 30000 (Q15) | | inline | name |
| DETUNE | its window 23 ms (DT_W) | DT_W | named | ok |
| reverbs | PLATE (Dattorro's delays and coefficients), HALL's lines and reflections | tables | named in plate.c / hall.c | ok |
| layer | ENVELOPE's time steps 0, 5, 15, 40, 100, 250, 600, 1500, 4000 ms (every 16) | | env_ms's table | ok |
| layer | BR>AMP linear | | inline | ok |
| layer | QUIRKS: +-12 cents, +-1.5 dB at 100 % | QUIRK_* | named | done |
| platform | the audio block 256 frames (5.8 ms), the control block 32 samples | APP_BLOCK, CTL | named | ok |

## What's left to do

1. **Name** the inline ones marked *name*: blocks at the top of `eng_ers.c` (or `ers.h`), `eng_fm.c`, `eng_analog.c`,
   `tone.c`, `fx.c`, no sound change (each checked with a render fingerprint, as EVA's was).
2. **Your call on the *knob?* ones:**
   - **VIB ONSET**, EVA's and ERS's: where in the breath the vibrato starts (55 % now).
   - **SWEEP TIME**, EVA's: how long the bright kick lasts (80 ms now).
   - **TONGUE TIME**, ERS's: how long its kick lasts (40 ms now).
3. Keep this file current with notes/03-lag-watch.md.
