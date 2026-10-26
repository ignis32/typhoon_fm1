# TYPHOON

TYPHOON is a firmware for the M-VAVE FM-1 that makes it the voice of a breath controller, the Artinoise re.corder:
breath is loudness and tone, the re.corder's tilt is pitch bend, and the FM-1 plays up to three layered voices
through its own effects. It also plays from USB or TRS MIDI.

Version {{VERSION}}. Installing is at your own risk; the installer can put M-VAVE's own firmware back.

- [Install](install/): put TYPHOON on your FM-1, from Chrome or Edge, the FM-1 connected by USB.
- [Preset editor](editor/): every parameter of the sound that plays and the preset slots, live over USB; "try it
  without an FM-1" runs it on the firmware in the page.
- [Simulator](sim/): the same code the FM-1 runs, in the browser, with sound. No FM-1 needed.
- [Download {{PKG}}](firmware/{{PKG}}): for the command-line installer, `python3 base/tools/fm1_install.py {{PKG}}`.
- [Source](https://github.com/ignis32/typhoon_fm1): MPL-2.0. Built on fm1-base (Charles Vestal) and felucca-bsp, from
  Felucca by Leo Kuroshita, Hügelton Instruments.

M-VAVE and FM-1 are trademarks of their owners, re.corder is Artinoise's. TYPHOON is not affiliated with them.
