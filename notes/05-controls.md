# Controls: sources, curves and the matrix (design, 2026-10-30)

Status: stages 1 .. 3 built 2026-10-30 (the sources; the routes, HOME's grid; the engines' hidden links, PANFLUTE).
Stage 4 (the effects' grid, new curves, CENTRE, an LFO source) not started. Keep this file the reference; what was
built differently from the plan below, and each engine change with what to listen for: "As built", at the end.

## Why

Today breath is special: engines read it directly, and many knobs only exist to say "how much breath moves this"
(BR>AMP, BR>TONE, BR>SHP, BR>LP, DRV AT, EVA BREATH). Other controllers are an afterthought: TILT>BEND in GLO,
PANFLUTE's own BEND CC / VIB CC knobs inside one preset. Some engine parameters also follow breath where nobody can
see it, such as EVA's vibrato only starting above 55 % breath. The result is many knobs, some of which do nothing in some
modes, and no way to give a parameter to another controller.

The new model:

- **Breath is just a CC.** No engine reads breath, or any CC, itself.
- The **controller** is described once, in GLO: which CC is which source and its curve (**sources**).
- The **sound** says what each source does, per layer, in a grid (**cells**), one source to many targets.
- An engine only sees its **targets**: knob values plus what the cells add. Nothing hidden.

## Sources (GLO → CONTROLS)

| Source | Assigned to | Convention (not enforced) |
|---|---|---|
| BEND | a CC (with centre: DEAD, RANGE, INVERT, as TILT>BEND now) or MIDI pitch bend | pitch bend |
| A | a CC, AT (channel aftertouch) or OFF | breath (CC11 by default) |
| B, C, D | a CC, AT or OFF | free (e.g. the re.corder's rotation, inclination, a TEControl's bite / nod) |
| AO, BO, CO, DO | derived: the top of A .. D | overblow (growl, drive, a brighter top) |

- **The CONTROLS page:** K1 .. K4 assign A .. D. BEND keeps its page (today's TILT>BEND, renamed).
- **The curve page:** EDIT opens the curve of the source last touched, so there are no extra GLO pages.
- **Each of A .. D has, on its curve page:**
  - **LOW, HIGH, SHAPE:** today's breath CURVE. A's curve is today's breath curve.
  - **SMOOTH (ms):** default 0. Today's G_BSMOOTH becomes A's. Every source with SMOOTH above 0 is a row in
    `03-lag-watch.md`.
  - **OVER:** OFF, or a threshold 1 .. 126 (after the curve).
    - When set, the source keeps running to 127, and its XO runs 0 → 127 from OVER up to the top. This is the DIY
      flute's CC11 101..127 → CC99 1..27, generalised.
    - The threshold is the player's, like the curve, so it is global: one change moves every preset.
    - A source with OVER OFF has no XO column in the grid.
- **Curves are global only** (not a preset's own). Today's GR_CURVE preset group goes. The curve is one
  self-contained piece per source (its type and values in one struct, one function from raw 0..127 to 0..1), so new
  curve types (reversed, steps, ...) are added inside it later without touching anything that reads sources.
- **CENTRE for A .. D** (a gyro's middle, signed values) is an idea for later. BEND has it already (DEAD).

## Targets

Two kinds, both declared by the engine (or the layer, for the common ones):

- **Knob targets:** the value is the knob plus the sum of the cells, clamped to the knob's range. The knob is the
  resting value.
- **Input targets:** there is no knob; the value is the sum of the cells, resting at 0. Each engine's EXCITE is one, and
  so are PANFLUTE's LIP and WOBBLE. EXCITE replaces what BR>SHP / vmod.shape and the breath carry into an engine today.
  With nothing routed to EXCITE the layer is silent. With EXCITE's knob (where an engine has one, e.g. ERS BLOW) up and
  nothing routed, the layer plays from notes alone (an organ).

**Choosing what is offered.** One place per engine, a true / false:

- **Knob targets:** a `MOD` flag in the engine's knob table, e.g.
  `{"CUTOFF", F_PCT, 0, 127, 64, 0, 0, MOD}` (param_desc_t gains a flags field).
- **Input targets:** a short list beside the knob table: name, range, what it drives.

**The rules for MOD:** continuous (no F_ENUM or switch), heard while it moves, safe to move every control block
(0.7 ms: no clicks, nothing re-initialised), and something a player would move within a phrase. Calibration and timing
knobs (markers, LAG, VIB SWELL, KEYTRK, the envelope) are not offered.

### The targets agreed

| | Offered | Not offered |
|---|---|---|
| **Every layer** | LEVEL, PITCH, PAN, TONE A, TONE B, TONE C | ENGINE, MUTE, TRANSP, FINE, the envelope, TONE (type), QUIRKS, QUIRK SET |
| **FM** | EXCITE (INDEX), FEEDBK, DETUNE, AIR, LP CUT | RATIO, KEYTRK, LP RES |
| **ANALOG** | EXCITE, CUT, RES, DRV, NOIS, MIX, DTN | WAVE, KTR |
| **ERS** | EXCITE (BLOW), COLOR, BRIGHT, AMOUNT, AIR, TILT, VIB, LP CUT | EXCITER, RESONATOR, SHAPER, DECAY, RATE, TONGUE, PR>PITCH, LAG, DRIFT, VIB SWELL, LP RES |
| **EVA** | EXCITE (BREATH), CUTOFF, RES, SHAPE, VOWEL, FORMANT, VIB, TREM, RATE, WAVE MOD (new) | WAVE, FILTER, S.E.D., SWEEP, KEYTRK, CHIFF, LAG, VIB SWELL |
| **PANFLUTE** | EXCITE (his breath), LIP (his bend), WOBBLE (his tilt input), LFO DEPTH, RATE | PP, MF, FF, REST, SEMI LO, SEMI HI, FULL, CENTRE |
| **Effects** (later, a grid of their own on the FX pages) | DRIVE, DELAY MIX, REVERB MIX | the rest |

**Amount scales:**

- A cell's AMOUNT is −100 .. +100 %.
- On a knob target, 100 % with the source at full moves the knob across its whole range.
- On PITCH, 100 % is G_BEND's range. BEND's DEAD, RANGE and INVERT apply first.
- Sources are 0 .. 1. BEND is −1 .. +1.

## Hidden links removed from the engines

The rule: **an engine reads no source.** Where an engine follows breath today in a way that is a synth's shortcut,
not its physics, the parameter becomes a plain target. The factory presets get the cell that keeps their sound. Each
item is a change to an engine's code, done and listed one by one (as `engines/pan/denis/CHANGES.md` is), so a
changed sound can be traced.

| Engine | Hidden now | Becomes | Factory presets get |
|---|---|---|---|
| EVA | VIB and TREM only above 55 % breath (EVA_VIB_ONSET), deeper blown harder | VIB, TREM plain depths | A→VIB, A→TREM |
| EVA | the vibrato faster blown harder | RATE plain | A→RATE |
| EVA | SHAPE's drive more with breath | SHAPE plain | A→SHAPE |
| EVA | each wave's breath-driven detail: BR PWM's width, HARMONIC's count, SYNC's ratio, SUPERSAW's spread, NS_TUNED's width (s->zw) | a new knob target WAVE MOD | A→WAVE MOD |
| ERS | VIB only above 55 % of a full tone's pressure | VIB plain | A→VIB |
| ERS, FM | the breath low-pass: BR>LP | LP CUT a target; the BR>LP knob goes | A→LP CUT |
| layer | BR>AMP, BR>TONE, BR>SHP | the knobs go | A→LEVEL, A→TONE A, A→EXCITE |
| FX (later) | DRIVE past DRV AT | DRV AT goes | AO→DRIVE (A's OVER = the old DRV AT) |
| PANFLUTE | BEND CC, VIB CC, VIB MODE, VIB DEPTH | the knobs go; LIP, WOBBLE, LFO DEPTH targets | B→LIP etc. by the preset |

**What the factory presets lose:** a linear A→VIB cannot reproduce the 55 % onset, so soft notes get a little
vibrato. Listen for it. If it is missed, the fix is a per-cell START (below), or AO→VIB where A's OVER sits near
there.

**What stays inside EXCITE** (the model's physics, documented as what EXCITE means in each engine):

- ERS: the pressure overblowing the tone, the air noise growing with the pressure, PR>PITCH, the growl's flutter, the
  free reed's swing.
- PANFLUTE: his whole pressure model, including the PP / MF / FF markers.
- EVA: S.E.D.'s split of the excitation into level and filter, and CHIFF as loud as the excitation.

Any source can still be routed to other targets on top of these.

## The grid (per layer: HOME)

HOME's breath page (BR>AMP, BR>TONE, BR>SHP) becomes the layer's grid:

```
 LAYER 1  EVA         BND A  AO  B  C
 LEVEL                 .  ●  .   .  .
 PITCH                 ●  .  .   .  .
 EXCITE                .  ●  .   .  .
 CUTOFF                .  .  .   ◐  .
 VIB                   .  .  .   ●  .
 SHAPE                 .  .  ●   .  .
 ────────────────────────────────────
 B → VIB   +80 %
 K1 row   K2 column   K3 amount   K4 —
```

- **Rows:** the layer's targets (the common ones, then the engine's).
- **Columns:** the sources assigned in GLO (an unassigned source, or an XO with OVER OFF, has no column).
- **Cells:** a dot sized and coloured by its amount (another colour for negative); the cell under the cursor is
  written out at the bottom.
- **One to many:** a column with several dots. **Many to one:** a row with several; the cells add.
- **The knobs:**
  - K1: the row;
  - K2: the column;
  - K3: the amount (0 removes the cell);
  - K4: free, kept for a per-cell START if it is wanted.
- **Scrolling:** 10 to 16 rows fit at ~12 px; more scroll.
- **A new layer starts with A→EXCITE 100 % and BEND→PITCH 100 %**, so it plays at once. Both are visible and removable
  (PANFLUTE moves BEND to LIP).

## Storage

- **Cells are per preset, pooled across its layers:** up to **32** a preset, 3 bytes each:
  - layer (2 bits) and target (6 bits);
  - source (4 bits: BEND, A .. D, AO .. DO);
  - amount (int8, −100 .. +100).
- **They fit in the record:** the 512-byte preset record already has a fourth layer row (4 x 48 values stored, 3 used):
  96 bytes = 32 cells, so the record's size and the flash layout do not change. PS_VER 5.
- **Target numbers are stable per engine** (like ENG_ID_*): new targets are appended, never renumbered.
- **GLO's sources** go in the global state (STATE_MAGIC bumped).

## Running it

- **Once per control block (CTL, 32 samples),** per layer:
  1. read the sources (after curve, SMOOTH, OVER);
  2. copy the layer's p[] to a scratch copy on the stack (~84 bytes);
  3. add each cell's source x amount to its target;
  4. pass the copy to the engine.

  The engine reads p[] as now, and its inputs (EXCITE, LIP, WOBBLE) from the vmod.
- **RAM:** the cells of the current preset (96 bytes) and the sources' state (a few dozen bytes), in typhoon's ~0.9 KB
  free. Nothing per sample.
- **PANFLUTE (core 2)** takes its inputs at the audio block's start, as now.

## No conversion

There are no saved presets to keep (the user, 2026-10-30), so nothing is converted:

- **Old preset records are not read.** A record older than PS_VER 5 fails `ps_ok`, so its slot plays its factory
  preset (or INIT), as a slot never saved does. The read paths for older records (versions 1 .. 4, PS_NP_V3) are
  deleted.
- **The global state starts from its defaults once** (a new STATE_MAGIC; the older ones' read paths deleted). The
  defaults are the re.corder's: A = CC11 (breath), BEND = CC52 (rotation). Bluetooth and the other GLO settings are
  set again by hand once.
- **The factory presets (core/factory.h) are written in the new form directly.**

## Tests

- **A MOD flag on a switch** (F_ENUM, F_ONOFF) fails the build's check.
- **No engine source file reads breath, src_breath or cc_raw** (a grep in the sim tests).
- **Each factory preset sounds the same as before:** the level, the pitch and the spectrum over a breath swell, against
  renders from the build before the change, within a tolerance. The EVA vibrato onset is the known exception, tested on its own.
- **B→VIB:** vibrato with breath held soft, none with B at 0.
- **AO:** zero below OVER, 127 at the top, A unchanged.
- **The web editor** (web/editor, tsx.mjs tables) shows and edits the grid and the CONTROLS page.

## The order of work

1. **Sources:** GLO CONTROLS, curves per source, SMOOTH, OVER; breath moved into A, tilt into BEND. The sound
   unchanged.
2. **The grid:** cells, storage (PS_VER 5), the HOME page, applying cells per block, the MOD flags and input lists.
3. **The engines:**
   - the hidden links out, one change at a time, each listed;
   - BR>AMP, BR>TONE, BR>SHP, BR>LP and PANFLUTE's routing knobs removed;
   - the old record and state read paths deleted;
   - the factory presets rewritten;
   - the web editor.
4. **Later:**
   - the effects' grid (DRIVE, DELAY MIX, REVERB MIX; DRV AT into A's OVER);
   - new curve types;
   - CENTRE for A .. D;
   - a per-cell START;
   - an LFO as a source, which would retire every engine's own vibrato knobs (EVA VIB, RATE, TREM, VIB SWELL; ERS VIB,
     RATE, VIB SWELL; PANFLUTE RATE).

## As built (2026-10-30)

**Differences from the plan above:**

- **LEVEL is a share, not an addition:** a route to LEVEL at +100 % makes the whole level the source's (as BR>AMP 127
  did), at -100 % the level falls as the source rises; several multiply. Adding to the layer's LEVEL knob would have
  made 0 (the layer off) the resting value.
- **Cells:** 32 a preset, pooled across its layers, each in a fixed slot (the audio never sees one half written).
- **PITCH** is not offered on a PANFLUTE layer (his pipes take no pitch: CHANGES.md W8).
- **TONE's own breath** went too: TALK's DEPTH and SUB OCT's BREATH are routes (A -> VOWEL 63 %, A -> LEVEL 17 % with
  LEVEL 48); SWEEP's accent rises with what its routes move its CUTOFF with (A by default: as before).
- **Engine presets** (the tables in each engine, which the factory presets use) keep their knobs as they were made;
  a layer taking one gets the depths that were with breath as A routes (core/routes.c routes_preset). The factory
  presets' own routes (core/factory.h CEL) go on top.
- **EVA's BREATH** (how much of EXCITE reaches its slots) stays a knob: it is EVA's own scale, like PANFLUTE's markers.

**Every engine change, what to listen for:**

| Engine | Before | Now | Listen for |
|---|---|---|---|
| layer | BR>AMP, BR>TONE (vmod.cutoff), BR>SHP | routes: LEVEL, the filter knob the engine had on vmod.cutoff (ANALOG CUT, ERS AMOUNT, TONE CUTOFF: 31 %), EXCITE | a filter swept by breath in coarser steps (a knob's 128, not 256 a knob step): zipper on slow swells |
| FM, ERS | BR>LP +40 | A -> LP CUT 63 %; the BR>LP knob gone | the same |
| EVA | VIB, TREM only above 55 % breath, deeper blown harder | VIB, TREM plain; A -> VIB (the preset's depth), the swell from each tongued note | **soft notes now have a little vibrato** (a linear A -> VIB: no onset) |
| EVA | the vibrato 1.6 Hz faster at full breath | A -> RATE 32 % | the same |
| EVA | SHAPE's drive half with the level | SHAPE half at rest, A -> SHAPE the other half (from A, not EVA's level slot) | the drive's breath response a little different at the bottom |
| EVA | the waves' breath (BR PWM, HARMONIC, SYNC, SUPERSAW, TUNED) | WAVE MOD (EDIT 5), A -> WAVE MOD 100 % | the same |
| ERS | VIB only above 55 % of a full tone's pressure | VIB plain, A -> VIB (the preset's) | **soft notes now have a little vibrato** (BANSURI VIB) |
| PANFLUTE | breath from the CURVE before SMOOTH; BEND CC, VIB MODE, VIB CC, VIB DEPTH | EXCITE, LIP, WOBBLE, LFO DEPTH routes; TILT DEPTH (c/g), RATE, CENTRE | the same (the LFO measured equal, 2026-10-30) |

**Still reading a source themselves** (stage 4): the effects' DRIVE (past DRV AT on A) and the status row's breath
bar. CHIFF (EVA, ERS's JET) and ERS's physics follow EXCITE, as planned.
