# Licensing

typhoon's own code is **MPL-2.0** (full text in `LICENSE`), file by file: each file's `SPDX-License-Identifier` line
says which licence it is under. If you change one of these files and give the result to others, publish your change
to that file under MPL-2.0. typhoon's predecessor, tornado, was GPL-3.0-only before its
move onto fm1-base (2026-10-23); those versions are not in this repository.

## Where the code comes from

| Code | Origin | Licence |
| --- | --- | --- |
| `base/`: the platform (screen and drawing, panel, MIDI, USB, storage, Bluetooth, the build, the simulator) | [fm1-base](https://github.com/charlesvestal/fm1-base) (Charles Vestal) on [felucca-bsp](https://github.com/hugelton/felucca-bsp) (`base/bsp/`), a pinned git submodule; from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments), with his permission ([felucca-bsp#4](https://github.com/hugelton/felucca-bsp/issues/4)) | MPL-2.0, file by file (some of `base/ble/` MPL-2.0 AND Apache-2.0: parts of the JieLi AC79 SDK); see `base/LICENSING.md` |
| `patches/`: typhoon's changes to fm1-base and felucca-bsp, applied to the pinned trees before every build (`patches/README.md`) | written for typhoon, on their files | MPL-2.0 (as the files they change) |
| typhoon's own code: `app/`, `firmware/src/core/`, `firmware/src/ui/`, `firmware/src/engines/engines.h` and the engines `fm`, `ers`, `eva`; `web/editor/`, `web/presets/`, `web/sim/typhoon_sim.c` and `build.sh`; `tools/` (but `gen_engine_tables.py`, below); `tests/`, `profiles/`, `build.sh` | written for typhoon | MPL-2.0 |
| ERS's models (`firmware/src/engines/ers/`) | written for typhoon after the designs of [STK](https://github.com/thestk/stk) (Perry R. Cook, Gary P. Scavone): Flute, Clarinet, Saxofony, Bowed, BowTable (STK's licence is MIT-like) | MPL-2.0 |
| [Felucca-Engines](https://github.com/hugelton/Felucca-Engines) (c46e008): `firmware/src/engines/felucca_engine.h` (the engine interface, which also shapes `core/layers.h`'s `track_t`), `engines/analog/eng_analog.c`, `firmware/src/dsp/dsp.c`, `tools/gen_engine_tables.py` and its output `firmware/gen/fe_tables.h` (unchanged but for the `FS` guard) | Leo Kuroshita, Hügelton Instruments | MIT (`LICENSES/MIT-Felucca-Engines.txt`) |
| The browser emulator (`./build.sh --web`): fm1-base's page and worklet (`base/web/emu/`), with typhoon's part in `app/web/emu/` (`app.js`, `app_worklet.js`) and `app/web.c` | fm1-base (Charles Vestal); typhoon's part written for typhoon | MPL-2.0 |
| The Terminus faces on the screen (fm1-base's `tools/gen_font.py`, `X0X_FONTSET=terminus`) | Terminus Font, Dimitar Zhekov | SIL OFL 1.1 (`base/assets/fonts/Terminus-LICENSE.txt`) |
| JieLi AC79 SDK: the Bluetooth libraries linked in, `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` placed in the package, all read from your SDK checkout at build time; no SDK files are in this tree | Zhuhai Jieli Technology | Apache-2.0 (`LICENSES/Apache-2.0.txt`) |

How this was checked: fm1-base's `base/tools/provenance.py` against Felucca (at `63f88ec1`,
its credited pull requests included) and against Dinghy, TONITOR, OberVave, Melodee, fm1-x0x, Jangada, Hortator,
Optimist, FM-106, ORBIT and Modular-1; apart from the rows above, it finds only `build.sh`'s three lines of shell
boilerplate and the `import` lines every Node script starts with.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE" and "FM-1" belong to their owners;
"re.corder" is Artinoise's. typhoon is not made or endorsed by them.
