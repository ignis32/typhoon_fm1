# patches: typhoon's changes to its platform

typhoon builds on [fm1-base](https://github.com/charlesvestal/fm1-base) (`base/`, a pinned git submodule) with
[felucca-bsp](https://github.com/hugelton/felucca-bsp) inside it (`base/bsp/`). What typhoon needs changed there is
kept here as patches, in plain sight, and applied to the pinned trees before every build. Nothing is committed
inside `base/`: these files are the record.

| Folder | Applied in | Order |
| --- | --- | --- |
| `felucca-bsp/NNNN-*.patch` | `base/bsp/` | first, by name |
| `fm1-base/NNNN-*.patch` | `base/` | after, by name |

Each patch starts with a header: what it changes, why typhoon needs it, the upstream commit it applies to, and
whether it has been offered upstream. Patched files are MPL-2.0 as the originals, so the patches are too.

## Working with them (`tools/patches.sh`)

- `./build.sh` (and `web/sim/build.sh`) run `tools/patches.sh apply`: each patch not applied yet goes in; one already
  applied is left; one that fits neither way stops the build with its name.
- `tools/patches.sh status`: each patch, applied / open / conflict.
- Changing a patch: edit the files in `base/` or `base/bsp/`, then `tools/patches.sh refresh patches/…/NNNN-x.patch`
  (its header kept; the other patches on that tree are taken out meanwhile and put back). A new file needs
  `git -C base add -N FILE` first.
- `.gitmodules` has `ignore = dirty` for `base`: the patched submodule doesn't show as changed in typhoon's status.

## Updating fm1-base

1. `tools/patches.sh revert` (the trees as upstream left them).
2. Bump the submodule (`git -C base fetch && git -C base checkout <commit> && git -C base submodule update`), commit
   the new pointer.
3. `tools/patches.sh apply`. A patch upstream has taken in: delete it. One that no longer fits: apply what fits by
   hand, `refresh` it, and say in its header which upstream commit it now applies to.
4. Build and run the checks (`./build.sh`, then `sh web/sim/build.sh && node tests/sim_test.mjs`).

## The patches

| Patch | What |
| --- | --- |
| `felucca-bsp/0001-usb-audio-runtime-layout.patch` | the USB audio streams presented chosen at run time: both, either, or none (MIDI only) |
| `fm1-base/0001-usb-audio-levels.patch` | `plat_usb_audio_set`: each USB audio direction OFF / MUTE / ON at 0 .. 200 %, the recording the mix or the app alone (GLOBAL > USB AUDIO) |
| `fm1-base/0003-ble-advertise-name.patch` | `plat_ble_advertise`: advertising on / off apart from the central (BLE-P, BLE-C); fm1.toml `ble_name`: the name advertised (TYPHOON, "TYPHOON 1F2D" in full) |

Taken upstream (deleted here): the radio power fix (`felucca-bsp/0002-radio-power.patch`, `fm1-base/0002-radio-power.patch`)
is felucca-bsp's `fm1_power_stock`, which fm1-base calls at the start since 5e1e79f (typhoon on fm1-base 934e40d).
