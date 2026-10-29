# Lag watch: everything that can stand between breath / a note and the sound

Kept current: any change that adds, removes or moves a delay updates this file in the same commit. Why: hidden
lags (EVA's followers, ERS's LAG, a 20 ms breath smoothing) once made typhoon feel as if Bluetooth or the breath
sensor were slow (2026-10-23, 2026-10-27). The rule: a delay on the breath is a knob, 0 on a new layer and in the
factory presets; the only delay presets carry is the vibrato's swell. Last checked: 2026-10-28.

**How to measure:** a breath step in the simulator, the level of each ms after it, time to 90 % of the new level.
`tests/sim/smooth.test.mjs` (SMOOTH, EVA at LAG 0: 5 ms), `tests/sim/eva.test.mjs` "EVA LAG" (LAG 0: at once;
64: 16 ms; 127: 43 ms). A sine through everything at defaults: ~3 ms, the measure's own window.

## The path, in order

| # | Where | What | Size | Visible | Status |
|---|---|---|---|---|---|
| 1 | the re.corder | how often it sends breath (CC11 / CC2) | unknown, its firmware | — | to measure: CCs a second on the BLE log |
| 2 | Bluetooth, BLE-C (typhoon the central, the re.corder its device) | connection interval: MIDI comes at most once per interval | **30 ms asked** (base/ble/midi/le_midi.c `mc_conn.conn_interval = 24`), the device may ask for less | no | **to measure** on the FM-1: the log's `conn: interval N` (x 1.25 ms). If 30 ms holds: up to 30 ms late, ~15 on average, bunched. Candidate fix: ask 7.5 ms (6), a patch on fm1-base, tried on the device first (two links, the radio busier) |
| 3 | Bluetooth, BLE-P (a computer the central) | the computer's interval | macOS / iOS ~11-15 ms; the FM-1 asks (le_midi.c connection_param_table) | no | the computer decides |
| 4 | Bluetooth in (radio.c) | notes and CCs at once; only clock / start / stop held to the sender's timestamps | 0 for notes | — | fine |
| 5 | USB MIDI | polled 2 kHz | 0.5 ms | — | fine |
| 6 | breath in: SMOOTH (G_BSMOOTH, GLO > BREATH IN) | a one-pole on the breath CC | 0 ms by default (5 until 2026-10-24, 20 until 2026-10-23) | knob | fine at 0 |
| 7 | breath in: the CURVE | shape only | 0 | knobs | fine |
| 8 | LEAPS HOLD (core/leap.c) | an upward slurred jump of OVER semitones or more waits up to WAIT for the next note (the thumb's passing notes) | 30 ms, only those jumps | knobs (LEAPS OFF: none) | by design; watch if leaps feel late |
| 9 | control rate | breath, pitch, levels updated each block of CTL 32 samples, interpolated within | 0.7 ms | — | fine |
| 10 | layer: ENVELOPE ATTACK | the level's rise on a tongued note | 0 by default; no factory preset has one | knob | fine |
| 11 | layer: BR>AMP | linear, at once | 0 | knob | fine |
| 12 | layer: QUIRKS | a fixed offset per fingering | 0 | knobs | no lag |
| 13 | EVA LAG | level, filter, the waves breath plays follow the breath | 0 in presets and new layers; 64: 8 / 25 ms (filter 12 / 40) | knob | **0 everywhere** (the user, 2026-10-28) |
| 14 | EVA VIB SWELL | the vibrato's depth comes in | 64 = 400 ms in presets | knob | the one delay presets carry: vibrato only, not the response |
| 15 | EVA HARMONIC wave | its harmonic count glides | ~3.6 ms (EVA_HARM_GLIDE) | constant in eng_eva.c's block | kept (a de-zipper: no steps as breath moves in 128 steps) |
| 16 | ERS LAG | the pressure follows the breath | 0 in presets and new layers; up to ~90 ms | knob | **0 everywhere** |
| 17 | ERS VIB SWELL | the vibrato's depth comes in | 64 = 400 ms in presets | knob | vibrato only |
| 18 | ERS FREE REED exciter (HARMONICA, BLUES HARP, ACCORDION, REED ORGAN, MELODICA, SHENG ...) | the reed's swing follows the breath | ~8 ms | constant ERS_FREE_SWING (ers.h) | kept (the user, 2026-10-28) |
| 19 | ERS flute windway (FLUE) | the jet smoothed | ~0.3 ms | constant ERS_FLUE_WW (ers.h) | negligible |
| 19b | ERS SAX: a slur | the pressure dips by SAX_DIP, back in ~45 ms (a slurred note's softer start, modelled) | ~45 ms, slurs only | constant SAX_DIP (ers_bores.c's SAX block) | by design; watch if sax slurs feel soft or late |
| 20 | FM index | glides over one block | 0.7 ms | — | a de-zipper |
| 21 | breath LP (ERS, FM EDIT 5) | a filter, no follower | 0 | knobs | no lag (a darker sound is not a later one) |
| 22 | TONE SWEEP | opens by how fast breath rises | by design | knobs | an effect of speed, not a delay |
| 22b | TONE SWEEP: its rise de-stepped (dsp/dspx.c ctl_destep) | the rise it reads is breath through a one-pole: the re.corder's ~50 breath steps a second each fired the accent (a "brbrbr" flutter at soft breath) | 15 ms, the accent's bite only; the level and BR>TONE at once | constant TONE_SWEEP_DESTEP_MS (core/tone.c) | the user, 2026-10-28: a shared de-stepper for what reacts to a change, never on the level |
| 22c | PANFLUTE: the breath's and the bend's line (Denis's, engines/pan/denis/config.h) | each new value is ramped to: a step of up to 2 values over 10.4 ms (his TEControl's period: a swell's 7-bit stairs joined), 8 and more over 4 ms (a tongued attack keeps its edge), between: between | 4 .. 10.4 ms | constants PF_LINE_PERIOD_MS, PF_LINE_FLOOR_MS, PF_LINE_SMALL, PF_LINE_LARGE (his config.h) | his design; the re.corder steps every ~20 ms, so the small-step line may be retuned to that |
| 22d | PANFLUTE: a hop (a new key under held breath) | the jet lifts off the old pipe, then lands on the new one | 20 + 20 ms | constants PF_HOP_LIFT_MS, PF_HOP_ATTACK_MS (his config.h) | by design (a pan flute's hop); watch if slurs feel late |
| 22e | PANFLUTE on the second core | its controls are taken at the audio block's start (as every engine's: the main loop posts between blocks), the block rendered while core 1 renders the other layers | 0 added | — | fine |
| 23 | effects: DETUNE's copies, DELAY, reverb PRE-DLY | late copies beside the dry, which is at once | knobs | knobs | not on the dry path |
| 24 | audio out | the render a block (APP_BLOCK 256 frames, 5.8 ms) ahead of the DAC | ~6 .. 12 ms | — | the platform; fine |

**ctl_destep's users** (dsp/dspx.c: for what reacts to a control's change; never on what sets the level): TONE
SWEEP's rise (22b). A new user: a row here, its time a named constant beside it.

## What to do when something feels late

1. Read the BLE log's interval (row 2): the likeliest one we do not control yet.
2. Check SMOOTH is 0 and the preset's LAGs are 0 (rows 6, 13, 16).
3. Run the breath-step checks above; a new number over a few ms is a regression.
