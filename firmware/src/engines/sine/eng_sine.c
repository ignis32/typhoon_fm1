/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* SINE: typhoon's smallest engine, and the template for new ones. A sine with a second harmonic (its
 * level is EDIT knob "2ND", plus SHAPE: breath can bring it in) and a little noise for air.
 *
 * An engine is an engine_t (felucca_engine.h, Felucca-Engines' interface, unchanged): a name, up to sixteen
 * parameters (edit[16], P_E0..P_E15 in the layer's p[]), presets, and render(), which ADDS one block of the voice to out[].
 * typhoon's layer (core/layers.c) gives it the voice's pitch (m->inc, m->pitch16), its loudness ramp
 * (m->amp0 -> m->amp1: breath, or the velocity) and two breath-driven offsets: m->cutoff (filter, 0..127 << 8
 * scale) and m->shape (centred on 64 << 8). Use dsp/dsp.c's blocks, not your own copies (one copy each in flash).
 * To add an engine: a folder here, its engine_t, a line in engines.h, its name in a profile (profiles/). */

static const preset_t SINE_PRESETS[] = {
    {"PURE", {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 20}, 0, 1, {0}, 0},
    {"BREATHY", {40, 30, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 20}, 0, 1, {0}, 0},
};

static void sine_note_on(track_t *t, voice_t *v)
{
    (void)t;
    if (!v->s[0])
        v->s[0] = 0x2545F491 + (int32_t)v->age;        /* noise seed */
}

static void sine_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t h2 = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8);   /* 2ND: 0..127 << 8 */
    int32_t air = p[P_E1] << 7;                        /* AIR: noise level (127: half what it was, 2026-10-18: too loud) */
    uint32_t ph = v->ph[0], i;
    for (i = 0; i < n; i++) {
        int32_t s = osc_sine(ph) + ((osc_sine(ph << 1) * h2) >> 15);
        s += (((int32_t)noise32(&v->s[0]) >> 17) * air) >> 15;
        ph += m->inc;
        out[i] += voice_amp((s * 3) >> 2, m, i);      /* (3/4: room for the 2ND) */
    }
    v->ph[0] = ph;
}

static const engine_t ENG_SINE = {
    .name = "SINE",
    .page_title = {"SINE", "SINE +"},
    .edit = {
        {"2ND", F_PCT, 0, 127, 0, 0, 0},
        {"AIR", F_PCT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = SINE_PRESETS,
    .npresets = NELEM(SINE_PRESETS),
    .knob = {P_E0 - P_E0, P_E1 - P_E0, 2, 3},
    .note_on = sine_note_on,
    .render = sine_render,
};
