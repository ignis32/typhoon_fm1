# Notes

Findings worth keeping, written for anyone doing the same on an FM-1 (M-VAVE, JieLi AC79 / WL82).

- [01-bluetooth-start-power.md](01-bluetooth-start-power.md): JieLi's Bluetooth stack on a bare-metal FM-1 firmware
  crashes or hangs on every start after a power cycle; the radio's DC-DC that stock V15 sets up and we did not. How
  it was found (UBOOT RAM reads, `.noinit` evidence, a P33 register diff) and the fix.
- [05-controls.md](05-controls.md): the controls design: breath is just a CC; sources BEND, A .. D (and their
  overblow AO .. DO) set once in GLO with their curves; per layer a grid of cells routes them to the targets each
  engine marks; no engine reads a controller itself.
