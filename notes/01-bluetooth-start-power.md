# Bluetooth on the FM-1: the start crashes after a power cycle, and why (2026-10-15/16)

**Summary.** On a bare-metal FM-1 firmware that runs JieLi's Bluetooth stack (fm1-ctl's shim, ported to typhoon),
Bluetooth worked after stock V15 had run, and crashed or hung on every start after a **power cycle**. Cause: after a
power-on the chip's 1.3 V supply runs from its **LDO** and the radio's **DC-DC is unconfigured**; JieLi's
`power_init` (which stock V15 runs, which a Felucca-family firmware does not) switches it to the DC-DC. The radio's
calibration then draws more than the LDO gives, the supply sags, and reads from the flash and the buses go wrong.
**Fix:** set the four P33 registers below at boot, before the radio starts. Seen working on one unit (an FM-1 with
the V15 factory build): since that build Bluetooth started on 5 power cycles in a row and on
switching it on and off several times (2026-10-16); still to do: many more power cycles, long connections
and playing through it.

Found on typhoon's earlier platform (fm1-ctl's Bluetooth shim). typhoon now runs on fm1-base and felucca-bsp, where
the same fix is `fm1_radio_power` (felucca-bsp's `include/fm1_sys.h`), added by typhoon's
`patches/felucca-bsp/0002-radio-power.patch` and called at boot by `patches/fm1-base/0002-radio-power.patch`.

If you run JieLi's BT libraries on an FM-1 without JieLi's OS and Bluetooth only works "sometimes", or "until the
next power cycle", check these registers first.

## Symptoms

- First start after installing (from stock V15): works, BLE MIDI connects, switching Bluetooth off and on works.
- After a power cycle, every start fails within the radio's calibration window (~0.05 to 0.7 s after the start,
  the log's last lines are the trim's: `wifi_get_xosc ...`, `bank: 37`, `bak: 11 46 x47`, `setbak:36`):
  - a crash in our 10 kHz timer interrupt (running from the flash): `DBG_MSG` bit 21 `c0_wr_bus_inv`;
  - a jump to garbage (`pc 0x470`) out of the trim's own delay;
  - an *illegal instruction* (`EMU_MSG` bit 1) at a perfectly valid `pc = [sp++]` in RAM code;
  - a bus error reading `P33_CON` (`DBG_MSG` bit 4 `axi_rd_inv`);
  - with all of those worked around: a hang (the watchdog reset, no crash record).
- A check that compared the RAM code with its flash copy at the crash found 972 of ~1700 words "different", and
  stored identical values for the first one: the *reads* were unreliable at that moment, not the code.
- Installing stock V15 and then the firmware again "fixed" it, until the next power cycle.

The changing symptoms in one window, and the V15 / power-cycle pattern, were the clues. Every workaround below moved
the symptom without removing it.

## The cause: P33 registers, after V15 and after a power-on

Every P33 register (the power-management unit behind the P33 serial bus; 10-bit addresses, but the space repeats
every 0x100, so 256 registers) was read at boot, before Bluetooth starts, and kept in `.noinit`; then read through
UBOOT after a boot where Bluetooth worked (stock V15, then our firmware: a warm reset) and after a power cycle (it
hangs). 31 registers differ. The ones that matter:

| P33 | name (AC79 SDK `asm/p33.h`) | after V15 (works) | after power-on (fails) | meaning |
|---|---|---|---|---|
| 0x00 | `P3_ANA_CON0` | `0xFB` | `0x77` | bit 7 `RC_250k_EN`, bit 3 `DCDC13_EN`, bit 2 `LDO13_EN`: V15 runs the 1.3 V supply from the DC-DC |
| 0x07 | `P3_ANA_CON7` | `0xA5` | `0x00` | `BTDCDC_BIAS_HD_SEL`, `BTDCDC_DUTY_SEL`, `BTDCDC_OSC_SEL` |
| 0x08 | `P3_ANA_CON8` | `0x96` | `0x00` | `BTDCDC_V2I_RES`, `BTDCDC_ISEN_HD`, `BTDCDC_COMP_HD`, zero, ramp |
| 0x74 | `P3_WLDO12_AUTO` | `0xC0` | `0x00` | the radio's 1.2 V LDO |

The others (not set by the fix, listed for completeness): `P3_ANA_CON1` 20/60, `ANA_CON2` 58/18, `ANA_CON4` 27/00,
`ANA_CON5` 44/47 (VDDIO levels), `P3_VLVD_CON` 3D/05, `P3_RST_SRC` (the reset reason), `P3_LRC_CON0/1` D1 11 / 00 00,
`P3_VLD_KEEP` 00/20, `P3_PMU_CON0..3` 88 04 03 80 / 80 80 00 00, `P3_LP_*` (low-power timers, standby), wake-up
pending and pin selects.

These survive a warm reset (an update's restart, a watchdog reset) and are lost at power-off, which is the whole
pattern.

## The fix

At boot, before anything else, the radio still off (now `fm1_radio_power`, above):

```c
fm1_p33_write(P3_ANA_CON7, 0xA5);            /* the radio's DC-DC: bias, duty, oscillator */
fm1_p33_write(P3_ANA_CON8, 0x96);            /* ... V2I, current sense, comparator, zero, ramp */
fm1_p33_or(P3_ANA_CON0, 0x80);               /* RC_250k_EN */
delay 200 us;
fm1_p33_or(P3_ANA_CON0, 0x08);               /* DCDC13_EN: the DC-DC first ... */
delay 2 ms;
fm1_p33_and(P3_ANA_CON0, ~0x04);             /* ... then LDO13 off, so the rail never drops */
delay 1 ms;
fm1_p33_write(P3_WLDO12_AUTO, 0xC0);
```

The values are V15's on this unit, read back from the chip, not derived from the SDK's `power_init` (binary only).
Other units, or other stock versions, may set other levels; the same snapshot-and-diff finds them.

## How it was found (the method, reusable)

- **Read RAM and flash without changing anything**: the firmware enters UBOOT on the SysEx soft key
  `F0 22 24 35 7D F7` (written raw to the FM-1's MIDI port, e.g. `/dev/snd/midiC1D0`); JieLi's UBOOT
  (`4c4a:8057`, a SCSI device) then answers `jl-uboot-tool` (kagaimiq): `memread ADDR LEN FILE` and `read ADDR LEN
  FILE` (flash), `reset` to run the app again. Nothing is written. `.noinit` survives the trip.
- **Keep evidence in `.noinit`** (not zeroed at boot): the fault record (vector, pc, rets, `EMU_MSG`, `DBG_MSG`, the
  last four branches from the ETM), a Bluetooth log ring, counters in the radio's delay function (how many delays,
  the last length, TIMER4 at the last one), the P33 snapshots. **Not `static`**: the compiler may then assume a zero
  start and fold a magic check away; the build should refuse static `.noinit` objects. **A power-on fills `.noinit` with random bytes**: recognise it (`P3_RST_SRC` bit 0) or a garbage crash note
  appears.
- **The SDK names the bits**: `cpu/wl82/debug.c` has the `DBG_MSG` and `EMU_MSG` bit names, `include_lib/driver/
  cpu/wl82/asm/p33.h` the P33 registers and their fields.
- The diagnostic pieces (the P33 snapshots, the RAM-code check, the delay counters) were removed once the cause was
  found.
- The delay counters showed the calibration ran ~1340 delays of up to 1 ms for 0.69 s and then waited on something
  without a delay: a flag the radio never raised, on a sagging supply.

## Workarounds that did not fix it (on the earlier platform; harmless, may help elsewhere)

- Mask our flash-resident interrupts (timer, audio) during the radio start (fm1-ctl's `BLE_MASK_AT_START`).
- No P33 access during the start: the shim's `shim_delay_us` fed the watchdog over P33 every millisecond; now a plain
  TIMER4 wait, the watchdog armed for 32 s before the start and not fed until it is over.
- No flash write (a settings save: ~50 ms with interrupts off and the flash unreadable) during the start and for
  5 s after: switching Bluetooth on is itself a settings change, and its save landed in the start.
- Nothing else runs while it starts (no keys, screen, USB, MIDI).
- A crash guard: a start that crashed keeps Bluetooth off at the next boot.

## Other things learned on the way

- fm1-ctl's `le_midi.c` declares `bt_ble_adv_enable` static where the SDK's `bt_common.h` declares it extern; the
  JieLi toolchain 20260730.1 refuses that (fixed in the copy typhoon used).
- JieLi's BT libraries are LTO bitcode: link with the toolchain's `LLVMgold.so` plugin. Their radio code puts calls
  in `.volatile_ram_code`, so a "no calls in `.ram_text`" rule has to exempt theirs.
- The stack's saved data (syscfg: the BT address, `VM_BLE_LOCAL_INFO` 109, `VM_BLE_REMOTE_DB_INFO` 113) is small
  (< 100 B here); the radio trim (`VM_WIFI_RF_INIT_INFO` 187) is not saved by this shim, so every start calibrates.
  Erasing the saved data did not change the hang: it was not the cause.
- Size: the stack adds ~126 KB of flash, ~21 KB RAM, ~49 KB pool (three 8 KB task stacks, a 24 KB heap).

## Open

- Whether the other differing P33 registers matter (VDDIO levels, `PMU_CON`, the LRC oscillator); the four above
  were enough on this unit.
- Whether fm1-ctl sees the same after a power cycle (its shim is the same; it does not set these registers).
- Current draw and audio noise with the DC-DC on (fm1-base sets it at every boot of a build with a radio).
