# typhoon_fm1 — FM-1 as the voice of the Artinoise re.corder

Custom firmware for the M-VAVE FM-1 that makes it the sound of the re.corder, a breath MIDI controller with no sound
of its own: breath is loudness and tone, the re.corder's tilt is pitch bend, and the FM-1 plays a stack of up to
three layered voices through its own effects. On
[fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp)
(`base/`), from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments). Earlier
named tornado: its preset backups restore here too.

**On the web** (https://ignis32.github.io/typhoon_fm1/, Chrome or Edge for USB):
[install](https://ignis32.github.io/typhoon_fm1/install/) it on your FM-1,
[edit presets](https://ignis32.github.io/typhoon_fm1/editor/) live, or try it in the
[simulator](https://ignis32.github.io/typhoon_fm1/sim/) with no FM-1.

## Versions

The version is in `VERSION` (every build carries it: the header at power-on, SysEx INFO, `tools/presets.py list`).

- **0.3** (the first typhoon_fm1 release): on fm1-base, MPL-2.0 throughout;
  engines SINE, ANALOG, NOISE, FM, FOLD, VOWEL, ERS, EVA; reverbs PLATE and HALL; 96 preset slots; the preset editor
  (`web/editor/`); LEAPS; USB audio in and out; Bluetooth BLE-P / BLE-C on their own (below).
- **Since 0.3:** the engines FM, ANALOG, ERS and EVA (SINE, NOISE, FOLD and VOWEL removed: FM at INDEX 0 is the
  sine, EVA has the noises, NS_*); 20 knobs an engine (EDIT 5), older presets read as they were. The mix's
  BREATH LP is ERS's and FM's own (EDIT 5). Every delay on breath is a knob, 0 (none) on a new layer: EVA LAG and
  VIB SWELL, ERS LAG and VIB SWELL; LAG is 0 in every factory preset (the breath at once), the only delay they
  carry is the vibrato's swell. Every delay between breath and sound is listed in `notes/03-lag-watch.md`. EVA has a
  SINE wave (after TRI). QUIRKS (ARP's third page):
  each fingered note a little off in pitch and level, the same way every time, as a real instrument's tone holes;
  QUIRK SET picks another instrument's (EVA's hidden random humanise is gone).
- **0.2** (2026-10-17, as tornado, on its earlier GPL platform; not in this repository) — the first usable one. BLE central, tilt → pitch
  bend, three layers per preset with seven engines, the effects (then also an OLD reverb), 64 slots, SAVE / SAVE AS,
  backup and restore over USB-MIDI (`tools/presets.py`); no editor page yet.

## Playing it

**The link.** GLO → BLUETOOTH, two sides on their own: **BLE-P** (KNOB 1) ON lets a computer or phone find "TYPHOON 1F2D"
(four hex digits of each FM-1's own, so two are told apart) and connect; **BLE-C** (KNOB 2) connects the FM-1 to a Bluetooth MIDI device: OFF, FIRST (the default: the first it finds), CHOOSE
(the one you picked). KNOB 3 opens the list of devices around: REFRESH, the devices (the remembered one
marked `*`, the signal in bars), FORGET; OCT+ takes the entry, OCT- shuts the list. **AUTO_RECONNECT** (KNOB 4) ON:
whenever no device is connected (power-on, one gone) it looks until one is; OFF: one look. Under each side its state
in words, below them the log. Both OFF: the radio off. USB-MIDI and TRS MIDI always play too, on any channel. Notes are mono,
the last one wins, a slur is legato; breath is CC11 by default (GLO → BREATH CC: any CC, or AT for channel aftertouch;
CC2 until 2026-10-24); the breath curve starts at LOW 0 (8 until then); the
re.corder's tilt (CC52) becomes pitch bend (GLO → TILT>BEND: any CC, or OFF).

**USB audio** (GLO → USB AUDIO): **USB IN**, the computer's audio through the FM-1, and **USB OUT**, the FM-1 recorded
on the computer, each OFF (the default: the computer gets no such audio device, so it keeps its own defaults), MUTE
(there, silent) or on at a level of 0 .. 200 %; USB OUT records the MIX (the computer's audio in it) or the SYNTH
alone. MASTER is the speaker's and the headphones' volume only. A change to or from OFF applies once the knob rests:
the FM-1 drops off USB for about a second (MIDI too) and comes back with the other audio devices.

**USB host** (GLO → USB HOST): ON makes the FM-1's USB port a host for a USB MIDI device, such as the re.corder by
cable, plugged in directly or behind one hub. The FM-1 does not power the port, so use a powered hub or a powered OTG
adapter. While it is ON, no computer can use that port: no USB audio, and no editor or installer over USB. Turn it
OFF to get them back, or hold REC at power-on to enter update mode. The setting is saved; the FM-1 starts as a USB
device and switches to host on its first frame. OFF by default. Untested on an FM-1 yet.

**Performance** (GLO → PERFORMANCE): is the sound keeping up. CPU AVERAGE is the audio's load over about 90 ms;
WORST BLOCK, 1 S the slowest single block in the last second, and WORST BLOCK the slowest since the reset, each as %
of the block's time (red from 90 %). XRUNS counts the blocks not done in time since the reset, each a dropout heard as
a click, and LAST XRUN says when the last one came. Turn KNOB 1 to reset. Over USB: `tools/presets.py stats`.

**The sound.** A preset is the whole sound: three layers (engine and its knobs, level, transpose, pan, envelope, how
breath moves them) and a name. The sound after the layers (breath curve, breath LP, drive, detune, delay, reverb,
volume) is in groups: each has a **global** copy (the curve on GLO beside BREATH IN, the effects on REC; an amber
header), and a preset uses it (values dim, a
GLOBAL tag) until a knob is turned on the preset's page — then the group is the preset's **own** (OWN tag). Holding
the page's button gives it back to the global one.

| Control | Does |
|---|---|
| PRESETS encoder | the preset (loads it) |
| SELECT encoder | the layer 1-2-3 |
| ALGORITHM encoder | the layer's engine (the last place: OFF) |
| KNOB 1-4 | the page's four values |
| HOME | BREATH: how the layer follows breath (BR>AMP, BR>TONE, BR>SHP). ERS's and FM's breath LP (cutoff, resonance, breath amount) is their EDIT 5 |
| ENV | breath CURVE · hold: back to global |
| EDIT | the engine's knobs, page by page (EDIT 1 .. 5, the empty ones passed by) → TONE (after the engine: OFF, FILTER LP/BP/HP, SWEEP, TALK, SUB OCT) |
| FX | DRIVE → DETUNE → DELAY → REVERB → REVERB + (TYPE: PLATE / HALL, PRE-DLY), again and again · hold: back to global |
| LFO | ENVELOPE (per layer, drawn): ATTACK, DECAY, SUSTAIN, RELEASE → ENVELOPE + (DELAY, BR>AMP). Restarts on a fresh attack, legato runs on |
| ARP | LAYER (engine, level, transpose, pan) → LAYER + (breath → tone / shape, filter type, fine) |
| SEQ | MIX: the 3 levels, each with its engine, and the preset's VOLUME (KNOB 4: the whole sound, effects too; 100 = as it is) · hold: VOLUME back to global |
| GLO | the global settings: GLOBAL (breath source, bend, transpose) → BLUETOOTH (BLE-P, BLE-C, the list, AUTO_RECONNECT) → TILT>BEND → BREATH IN (BREATH CC, LOUDNESS) → CURVE (the global breath curve) → LEAPS → USB AUDIO → USB HOST → PERFORMANCE (load, xruns) · hold: GLOBAL TO FACTORY ? |
| REC | the global effects: DRIVE → DETUNE → DELAY → REVERB → REVERB + (what a preset without its own group plays) · hold: all notes off |
| OCT− / OCT+ | press: transpose a semitone (when let go) · hold: an octave (or SEL + press) · both: 0. Cuts the sounding note |
| PLAY | mute the layer · hold: solo it (again: back; PLAY blinks meanwhile) |
| SAVE | SAVE TO nn ? (PRESETS picks the slot) · hold: SAVE AS (the keys type the name — a legend on screen — PLAY deletes, SELECT the cursor, ALGORITHM 0-9 - . /, PRESETS the slot; only OCT+ saves, OCT- cancels) |

A question on the screen is answered with **OCT+ = YES**, **OCT− = NO** (the status row says so; SAVE again also
saves in the SAVE prompt; any other button, or 10 s, is NO; while asked, OCT does not transpose).

**Presets on a computer** (USB-MIDI, nothing to install but Python 3): `tools/presets.py list`, `backup [FILE]
[--slot N]`, `restore FILE [--slot N] [--to M]`, `rename N NAME`, `delete N`, `stats` (the audio's CPU load while
you play). Stock V15 wipes the presets (not the globals): back them up before installing it.
**The preset editor** (`web/editor/`, served from any web server; Chrome, the FM-1 on the computer's USB): every
parameter of the sound that plays, live (the FM-1's knobs show there too), each sound group OWN / GLOBAL, and
the slots: save, save as / to the next free slot, copy, rename, delete, backup / restore (the files `presets.py`
writes and reads). `?sim` (the link "try it without an FM-1") runs it on the firmware in the page.

## Building and working on it

- `git submodule update --init --recursive` once: fm1-base in `base/`, felucca-bsp in `base/bsp/`.
- `./build.sh [--engines PROFILE | a,b,c] [--out DIR] [--release X.Y]` — the device build, fm1-base's
  (`base/tools/build.py` on `app/fm1.toml`; the JieLi toolchain and AC79 SDK; its checks: no doubles, no 64-bit
  multiply inlined beside a load, register access only in felucca-bsp, the sizes), with Bluetooth, and its
  `report.json`. A profile is `profiles/<name>.txt`: engines one a line, `include NAME` for another profile.
- Tests: `sh tools/test.sh` (web/sim/build.sh, then tests/sim_test.mjs; zig 0.14.1, fetched if missing, Node 20; after `./build.sh`, which makes the
  generated headers): `tests/sim/*.test.mjs` (core, transpose, presets, fx, reverbs, ers, eva, sysex, the editor
  page's logic, Bluetooth, buttons, USB audio; each on a fresh firmware, numbers by name from `tools/consts.mjs`) on
  the simulator (`web/sim/typhoon_sim.c`: the app on fm1-base's host platform and its browser emulator's clock, as
  WebAssembly).
- `./build.sh --web` — the browser emulator (fm1-base's page with typhoon's part, `app/web/emu/`; emcc) into
  `app/build/emu/`.
- An engine is a folder `firmware/src/engines/<name>/` with a stable id in `engines/engines.h`; nothing outside it
  depends on it. The UI is data: `firmware/src/ui/layout.h`.

- Releases and the site (GitHub Actions): a tag `vX.Y` (`VERSION`) builds, tests and attests the package and
  publishes it as a release (`.github/workflows/release.yml`, The Firmware Hub's shared `zvenson/fm1-build`); then
  `pages.yml` makes the site (`tools/site.sh`: `install/` for that package, `editor/`, `sim/`) on GitHub Pages.

## Layout

- `app/` — typhoon on fm1-base: `fm1.toml` (the build's settings: name, identity, Bluetooth, USB audio), `app.h` /
  `app.c` (the platform's callbacks, and the calls typhoon's code makes, in fm1-base's terms), `ble.c` (the
  BLUETOOTH modes and the remembered device), `web.c` and `web/emu/` (typhoon's part of fm1-base's browser
  emulator, `./build.sh --web` into `app/build/emu/`: Web MIDI in, a breath slider, the presets kept).
- `firmware/src/` — typhoon itself: `core/` (layers, effects, reverbs, parameters, presets, SysEx), `engines/`,
  `dsp/`, `ui/`.
- `base/` — fm1-base (a pinned git submodule), with felucca-bsp in `base/bsp/`: the platform. typhoon's changes to them
  are patches in `patches/` (applied before every build: `patches/README.md`, `tools/patches.sh`).
- `tools/` — `engine_flags.sh` (profiles), `patches.sh` (`patches/` onto `base/`), `presets.py` (the presets over
  USB-MIDI; installs: `base/tools/fm1_install.py`), `consts.mjs` (the firmware's numbers by name, for the tests),
  `gen_engine_tables.py` (`firmware/gen/fe_tables.h`), `test.sh` (the checks), `site.sh` (the site).
- `tests/`, `web/sim/` (the test simulator: the app on fm1-base's host platform, WebAssembly), `web/presets/` (the
  preset protocol in JS), `web/editor/` (the preset editor page: `core.mjs` its logic, `app.js` the page,
  `simport.mjs` the firmware as a MIDI port).
- `profiles/` (which engines a build has); `notes/` — write-ups for the community. The platform's API and rules are
  fm1-base's (`base/DESIGN.md`, `base/include/plat.h`).
- Licences: `LICENSE` (MPL-2.0, typhoon's own code), `LICENSING.md` (what is under which licence), `LICENSES/`.
