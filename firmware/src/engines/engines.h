/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* Every engine typhoon knows, and which ones this build has.
 *
 * A build switches engines on with TYPHOON_ENG_<NAME>=1 (the profile in profiles/<name>.txt, one engine per line,
 * turned into -D flags by tools/engine_flags.sh for the device, host and browser builds alike). An engine left
 * out is not compiled at all: it costs no flash and no RAM.
 *
 * Each engine has a stable id (ENG_ID_*): saved settings keep the id, not the position in ENGINES[], so a
 * profile can change without breaking them. An id this build lacks plays ENGINES[0] instead (layers.c
 * eng_index); the screen then says which engine is missing (ui.c).
 *
 * The engines' UI, the same for all (ui/layout.h): two pages of four knobs, page 1 the tone, page 2 colour and
 * extras; page 1's first knob (P_E0) is what breath moves (each engine adds vmod.shape to it), and br_shp below
 * is how much breath moves it by default (the layer's BR>SHP when the engine is picked).
 *
 * To add an engine: engines/<name>/eng_<name>.c with its engine_t, a new id below (never reuse a number),
 * a block in each list, its name in eng_id_name and in profiles/. */

enum { ENG_ID_OFF = 0, ENG_ID_SINE = 1, ENG_ID_ANALOG = 2, ENG_ID_NOISE = 3, ENG_ID_FM = 4, ENG_ID_FOLD = 5, ENG_ID_VOWEL = 6, ENG_ID_ERS = 7,
       ENG_ID_SAMPLE = 8, ENG_ID_EVA = 9 };   /* (8: SAMPLE, recorded loops, 2026-10-20, removed 2026-10-21 for flash: in git history,
                               * 3523c50 .. 9e90cdf; its id stays taken) */
/* ENG_ID_OFF: a layer with no engine: silent, nothing rendered, its other settings kept. The engine knob has it after
 * the last engine (position NENGINES) */

#if TYPHOON_ENG_SINE
#include "sine/eng_sine.c"                 /* typhoon, GPL-3.0: the template */
#endif
#if TYPHOON_ENG_ANALOG
#include "analog/eng_analog.c"             /* Felucca-Engines, MIT: VA, 2 osc + SVF */
#endif
#if TYPHOON_ENG_NOISE
#include "noise/eng_noise.c"               /* Felucca-Engines, MIT: noise through a resonant filter (air layer) */
#endif
#if TYPHOON_ENG_FM
#include "fm/eng_fm.c"                     /* typhoon: 2-op FM, breath -> index */
#endif
#if TYPHOON_ENG_FOLD
#include "fold/eng_fold.c"                 /* typhoon: sine wavefolder, breath -> folds */
#endif
#if TYPHOON_ENG_VOWEL
#include "vowel/eng_vowel.c"               /* typhoon: 3 formants, breath -> vowel */
#endif
#if TYPHOON_ENG_ERS
#include "ers/eng_ers.c"                   /* typhoon: exciter -> resonator -> shaper, breath -> the pressure */
#endif
#if TYPHOON_ENG_EVA
#include "eva/eng_eva.c"                   /* typhoon: a breath synth, breath shaped per destination */
#endif

static const struct { uint8_t id; int8_t br_shp; const engine_t *e; } ENGINES[] = {
#if TYPHOON_ENG_SINE
    {ENG_ID_SINE, 30, &ENG_SINE},
#endif
#if TYPHOON_ENG_ANALOG
    {ENG_ID_ANALOG, 0, &ENG_ANALOG},       /* (its SHAPE is PWM; breath works on its filter: BR>FLT) */
#endif
#if TYPHOON_ENG_NOISE
    {ENG_ID_NOISE, 0, &ENG_NOISE},
#endif
#if TYPHOON_ENG_FM
    {ENG_ID_FM, 40, &ENG_FM},
#endif
#if TYPHOON_ENG_FOLD
    {ENG_ID_FOLD, 40, &ENG_FOLD},
#endif
#if TYPHOON_ENG_VOWEL
    {ENG_ID_VOWEL, 40, &ENG_VOWEL},
#endif
#if TYPHOON_ENG_ERS
    {ENG_ID_ERS, 63, &ENG_ERS},           /* (breath is the pressure: all of it) */
#endif
#if TYPHOON_ENG_EVA
    {ENG_ID_EVA, 63, &ENG_EVA},           /* (breath: the slots, all of it) */
#endif
};
#define NENGINES ((uint32_t)NELEM(ENGINES))
#if !(TYPHOON_ENG_SINE || TYPHOON_ENG_ANALOG || TYPHOON_ENG_NOISE || TYPHOON_ENG_FM || TYPHOON_ENG_FOLD || TYPHOON_ENG_VOWEL || TYPHOON_ENG_ERS \
      || TYPHOON_ENG_EVA)
#error "no engine: build with a profile (profiles/*.txt) or -DTYPHOON_ENG_<NAME>=1"
#endif

#if defined(TYPHOON_SIM)
static const char *host_engine_name(uint32_t i)    /* the simulator (sim_engine_name): engine i of this build, 0 past the last */
{
    return i < NENGINES ? ENGINES[i].e->name : 0;
}
#endif

/* the names of every known id, also the ones this build lacks (for "ANALOG missing") */
static const char *eng_id_name(uint32_t id)
{
    static const char *const N[] = {"OFF", "SINE", "ANALOG", "NOISE", "FM", "FOLD", "VOWEL", "ERS", "SAMPLE", "EVA"};
    return id < NELEM(N) ? N[id] : "?";
}
