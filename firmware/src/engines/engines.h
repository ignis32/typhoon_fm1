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
 * eng_index); the screen then says which engine is missing (ui.c). ENGINES[0] is FM: the INIT sound (one sine:
 * params.c layers_defaults) and what a missing engine plays.
 *
 * The engines' UI, the same for all (ui/layout.h): two pages of four knobs, page 1 the tone, page 2 colour and
 * extras; page 1's first knob (P_E0) is what breath moves (each engine adds vmod.shape to it), and br_shp below
 * is how much breath moves it by default (the layer's BR>SHP when the engine is picked).
 *
 * To add an engine: engines/<name>/eng_<name>.c with its engine_t, a new id below (never reuse a number),
 * a block in each list, its name in eng_id_name and in profiles/. */

enum { ENG_ID_OFF = 0, ENG_ID_SINE = 1, ENG_ID_ANALOG = 2, ENG_ID_NOISE = 3, ENG_ID_FM = 4, ENG_ID_FOLD = 5, ENG_ID_VOWEL = 6, ENG_ID_ERS = 7,
       ENG_ID_SAMPLE = 8, ENG_ID_EVA = 9, ENG_ID_PAN = 10 };   /* (8: SAMPLE, recorded loops, 2026-10-20, removed 2026-10-21 for flash: in git history,
                               * 3523c50 .. 9e90cdf; its id stays taken. 1 SINE, 3 NOISE, 5 FOLD, 6 VOWEL: removed
                               * 2026-10-28, in git history; their ids stay taken, a layer saved with one plays FM) */
/* ENG_ID_OFF: a layer with no engine: silent, nothing rendered, its other settings kept. The engine knob has it after
 * the last engine (position NENGINES) */

#if TYPHOON_ENG_FM
#include "fm/eng_fm.c"                     /* typhoon: 2-op FM, breath -> index */
#endif
#if TYPHOON_ENG_ANALOG
#include "analog/eng_analog.c"             /* Felucca-Engines, MIT: VA, 2 osc + SVF */
#endif
#if TYPHOON_ENG_ERS
#include "ers/eng_ers.c"                   /* typhoon: exciter -> resonator -> shaper, breath -> the pressure */
#endif
#if TYPHOON_ENG_EVA
#include "eva/eng_eva.c"                   /* typhoon: a breath synth, breath shaped per destination */
#endif
#if TYPHOON_ENG_PAN
#include "pan/eng_pan.c"                   /* Denis Rosset's pan flute (MIT, pan/denis/), on the second core */
#endif

static const struct { uint8_t id; int8_t br_shp; const engine_t *e; } ENGINES[] = {
#if TYPHOON_ENG_FM
    {ENG_ID_FM, 40, &ENG_FM},              /* (first: the INIT sound, what a missing engine plays) */
#endif
#if TYPHOON_ENG_ANALOG
    {ENG_ID_ANALOG, 0, &ENG_ANALOG},       /* (its SHAPE is PWM; breath works on its filter: BR>FLT) */
#endif
#if TYPHOON_ENG_ERS
    {ENG_ID_ERS, 63, &ENG_ERS},           /* (breath is the pressure: all of it) */
#endif
#if TYPHOON_ENG_EVA
    {ENG_ID_EVA, 63, &ENG_EVA},           /* (breath: the slots, all of it) */
#endif
#if TYPHOON_ENG_PAN
    {ENG_ID_PAN, 0, &ENG_PAN},            /* (breath is his pressure, his own way: no shape) */
#endif
};
#define NENGINES ((uint32_t)NELEM(ENGINES))
#if !TYPHOON_ENG_FM
#error "FM is in every build: the INIT sound (one sine) and what a missing engine plays (profiles/*.txt)"
#endif

#if defined(TYPHOON_SIM)
static const char *host_engine_name(uint32_t i)    /* the simulator (sim_engine_name): engine i of this build, 0 past the last */
{
    return i < NENGINES ? ENGINES[i].e->name : 0;
}
#endif

/* the names of every known id, also the ones this build lacks and the removed ones (for "ANALOG missing") */
static const char *eng_id_name(uint32_t id)
{
    static const char *const N[] = {"OFF", "SINE", "ANALOG", "NOISE", "FM", "FOLD", "VOWEL", "ERS", "SAMPLE", "EVA",
                                    "PANFLUTE"};
    return id < NELEM(N) ? N[id] : "?";
}
