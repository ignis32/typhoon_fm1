# Denis's pan flute in typhoon: every change from the original

**Upstream:** [denisvibecodes/toy-pan-flute](https://github.com/denisvibecodes/toy-pan-flute), commit `d288f0e`:
`src/panflute.c`, `panflute.h`, `config.h`, `coefficients.h`, by Denis Rosset, MIT (`LICENSES/MIT-toy-pan-flute.txt`).
His Freeverb (`src/reverb.c`) is not taken: the pan flute goes through typhoon's own effects as every layer does.

**The files as they came** are typhoon commit `12f2e24`. Everything since, in one command:

    git diff 12f2e24 -- firmware/src/engines/pan/denis/

Every change below is marked in the code with `typhoon (CHANGES.md N)`. A change is one row: what, why, and what it may
do to the sound or the feel (what to listen for when something sounds wrong). When upstream moves, take his new
files at a new commit, then redo the rows still needed.

## Changes in his files

| # | File | What | Why | Risk to the sound / the feel |
|---|---|---|---|---|
| 1 | panflute.c | The turbulence's hash (SplitMix64) out of line: `splitmix_at(key, n)`, `noinline`, with its multiply by GOLDEN | The FM-1's AC79 now and then corrupts a register when a 64-bit multiply is inlined among loads (fm1-base `bsp/docs/CPU.md`); the build checks for it | None: the same numbers, a call per sample more |
| 2 | panflute.c | The pipes' delay lines (bore 64 + jet 256 floats a pipe) in a pool of `PF_POOL` 4 slots, not one each: a pipe takes a slot as it wakes, gives it back as it sleeps (`pool_take`, `pool_give`); the wake list is built after the wakes | RAM: 25 pipes x 1.5 KB = 37.7 KB, typhoon had ~20 KB free; now ~10 KB | None while at most 4 pipes ring at once. A fifth (hops faster than the tails die) cuts the quietest tail the jet has left: a click is possible there, on very fast runs |
| 3 | panflute.c, pf_math.h (new) | `<math.h>` replaced by `pf_math.h`: his libm calls on fm1-base's `fastmath.h` (`fm_sqrtf`, `fm_tanhf`, `fm_expf`, `fm_powf`, `fm_log2f`, `fm_sinf`, `fm_cosf`, `fm_tanf` ...), and `acosf` (fastmath has none) as the Abramowitz & Stegun 4.4.46 polynomial, `roundf`, `isfinite` | The FM-1's firmware links no libm; the simulator takes the same functions, so it computes the device's samples | Tiny: fastmath is within 1e-7 .. 1e-5 of libm (acos 2e-8 rad); the tuning and the tone move by far less than a cent or a dB. If a pipe sounds out of tune against his page, look here first |

## Changes around his files (typhoon's wrapper, `engines/pan/eng_pan.c`)

Not in his code, but they decide what it hears. Listen here too.

| # | What | Why | Risk |
|---|---|---|---|
| W1 | Rendered on the FM-1's second core, a whole audio block (256 frames, 5.8 ms) at once; the controls are taken at the block's start | CPU: it costs more than three of typhoon's layers | His page applies them every 128 frames (2.9 ms); here every 256. typhoon takes every engine's controls once a block anyway (the main loop posts between blocks) |
| W2 | One layer at a time may play it (his state is one instrument's) | His code keeps its state in statics | — |
| W3 | Breath: typhoon's breath after its CURVE (GLO / the preset's), 0..127, not the raw CC; before SMOOTH | So the CURVE pages work for it as for every engine; his own line ramps join the 7-bit steps | With a linear curve, the same as his. SMOOTH does not reach it |
| W4 | The breath's pp / mf / ff markers are knobs, typhoon's defaults 20 / 45 / 90 (his: 4 / 12 / 33, a TEControl's) | The re.corder's breath values are not a TEControl's | Set by ear; too low ff over-blows, too high never reaches ff |
| W5 | Bend and vibrato come from a controller number each (knobs BEND CC, VIB CC; 0 = none, the default): the raw 7-bit value | Keep his three controls; the re.corder has breath and its rotation (CC 52) only | With none chosen: no bend, no vibrato (his defaults otherwise) |
| W6 | Keys outside his 25 pipes (G4..G6, 67..91) are folded by octaves into them | The re.corder's notes go beyond | A folded note is an octave off what was fingered |
| W8 | typhoon's FINE, pitch bend (and the tilt's TILT>BEND), QUIRKS' pitch, BR>TONE, BR>SHP and the ENVELOPE's shape beyond a gate do not reach him | His pipes are cut to their notes; his bend is the lip (BEND CC) | Those layer knobs do nothing on a PANFLUTE layer |
| W7 | Output: his signal (1.0 = full scale) x `PAN_OUT_LVL` into the layer, then typhoon's layer chain (level, pan, TONE), effects, MASTER | typhoon's mix | Level only |
