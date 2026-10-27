# Plans: noise waves, breath split, bipolar breath amounts (2026-10-27)

Agreed in discussion, not built yet except where marked. Order: 1, 2, 3, then 4 and 5 together.

## 1. EVA noise waves (next)

Four entries added to EVA's WAVE list, after SUPERSAW, so existing presets keep their waves:

| Wave | What | Use |
| --- | --- | --- |
| WHITE | flat noise | bright "tss", spit |
| PINK | -3 dB/oct | a natural breath "hhh", the all-round puff |
| BROWN | -6 dB/oct | a soft "huh", a "p" puff |
| TUNED | noise through a narrow resonator on the note's pitch; breath narrows it (soft: breathy, hard: almost a tone) | flute or pipe chiff; the harmonic via the layer's TRANS (+12, +19) |

Each wave should be about as loud as a saw. A puff is then a layer, needing no new knobs:
- **Shape:** the layer ENVELOPE (ATTACK 0, DECAY is the length, SUSTAIN 0). It restarts only on a tongued note; legato runs on.
- **Spectrum:** SWEEP for a bright start, or a dark start when negative. BP plus VOWEL/FORMANT for the consonant ("puh" ~1 kHz, "tuh"/"kuh" 2-3 kHz, "tss" > 4 kHz). KEYTRK makes high notes brighter.
- **Tone under it:** a tone layer with ATTACK ~15 ms, so the puff comes first.

CHIFF stays for now, because 7 of EVA's 18 engine presets use it. Retire it once the noise layers have proven themselves.

## 2. ALTO FLUTE + noise

Five variants of the factory ALTO FLUTE, each with a noise layer (puff, chiff, air), for listening on the FM-1.

## 3. Transpose / octave splash

A big splash screen on transpose and octave changes, showing the new value, readable from a distance (the header's
-1 / +4 is not).

## 4. Bipolar breath amounts

One knob gives both the amount and the direction of breath's effect:
- **0:** none.
- **Right:** follows breath.
- **Left:** against it.

BR>FLT and BR>SHP already work this way. To do:
- **BR>AMP bipolar:** negative is louder as the breath gets softer (an airy pianissimo layer).
- **EVA's own breath links:** today EVA's level, filter and vibrato always rise with breath, so they need a bipolar form too. Care is needed, since a negative breath -> level alone means sound with no breath.
- **The other engines:** go through every breath-driven knob; bipolar only where inverting means something.
- **Saved presets:** each changed knob needs a mapping from the old 0..127 values that keeps the sound as it is.

## 5. Breath split: the layers stacked along the breath (like velocity layers)

This replaces the per-layer XFADE idea. A bipolar knob only has two directions, so it can't give a middle zone.

One page for the whole sound, saved in the preset:

| Knob | What |
| --- | --- |
| SPLIT | OFF (every layer always, as today), 1>2 (layer 3 always on, as a body), 1>2>3 |
| POINT A | breath 0..127 where layer 1 hands over to 2 |
| POINT B | where 2 hands over to 3 |
| WIDTH | each handover's width in breath units (0: a hard switch) |

- The layer number is its place in the breath (1 the softest), so overlaps can't contradict each other.
- The split reads breath after the global CURVE, the same value the layers get.
- **No holes:** at most two neighbouring layers fade at once, with equal-power curves (sin / cos: 71 % each at
  the handover's middle). Outside a handover one layer plays at 100 %, so with equally loud layers the total is even.
  The dynamics come on top, from BR>AMP or EVA's own breath.
- **Check by ear:** two very alike layers (same wave, same pitch) add by amplitude, and equal-power then bumps
  slightly in the middle.

Example: 1>2>3, A 20, B 100, WIDTH 10 gives layer 1 at 0-15, a handover at 20, layer 2 at 25-95, a handover at 100,
and layer 3 at 105-127.

**Four layers** (if NLAYER becomes 4) scale the same way: N layers, N-1 points, modes 1>2 .. 1>2>3>4. The page's
four knobs then take SPLIT, A, B, C, with WIDTH on a "+" page. A bonus comes with it: under a split, a layer at 0 %
share needs no rendering, only its envelope and state kept. At most two layers render at once, which makes a fourth
layer cheap. That is worth measuring alongside the second core.
