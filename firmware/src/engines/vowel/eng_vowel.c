/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* VOWEL: a singing voice. A saw / pulse source and aspiration noise through three formant band-passes (Simper
 * SVFs, dsp/dsp.c's coefficients), moved through the vowels U O A E I (dark .. bright). Breath moves the vowel,
 * so a swell opens "oo" into "ah"; the noise is the breath you hear in a voice.
 *
 * Page 1 (the tone):  VOWEL  where between U O A E I (what breath moves: + vmod.shape)
 *                     RES    the formants' sharpness: soft and wide .. ringing and narrow
 *                     AIR    aspiration noise through the formants
 *                     SOURCE saw .. pulse (pulse: hollower, more nasal)
 * Page 2 (colour):    SHIFT  all formants up / down (a bigger or smaller throat), about +-10 semitones
 *                     F3     the third formant's level (presence, the "ring" of a voice)
 * The formant frequencies are the classic male averages (Peterson & Barney 1952), in the filter's cutoff scale
 * (tools/gen_engine_tables.py cutoff_hz: v = 127 ln(f / 30) / ln(16000 / 30), << 8). */

#define VW_N 5
static const int16_t VW_F[VW_N][3] = {             /* F1 F2 F3 per vowel, cutoff scale << 8 */
    {11922, 17435, 22332},                         /* U  300  870 2240 Hz */
    {15246, 17253, 22711},                         /* O  570  840 2410 */
    {16527, 18602, 22775},                         /* A  730 1090 2440 */
    {14869, 21313, 22859},                         /* E  530 1840 2480 */
    {11377, 22446, 23862},                         /* I  270 2290 3010 */
};
static const int16_t VW_GAIN[3] = {32767, 20000, 11000};   /* F1 loudest, as in speech */

static const preset_t VOWEL_PRESETS[] = {
    {"OOH-AAH", {30, 80, 20, 0, 0, 60, 0, 0}, {0, 0, 127, 25}, 0, 1, {0}, 0},
    {"WHISPER", {60, 60, 110, 0, 0, 40, 0, 0}, {0, 0, 127, 25}, 0, 1, {0}, 0},
    {"NASAL", {80, 100, 15, 100, 10, 80, 0, 0}, {0, 0, 127, 25}, 0, 1, {0}, 0},
};

static void vowel_note_on(track_t *t, voice_t *v)
{
    (void)t;
    if (!v->s[6])
        v->s[6] = 0x16180339 + (int32_t)v->age;        /* noise seed */
}

/* one band-pass step of the trapezoidal SVF (as eng_analog.c analog_tap, BP = v1) */
static inline int32_t vowel_bp(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    return v1;
}

static void vowel_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t pos = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8);
    int32_t seg = pos * (VW_N - 1) / (127 << 8), frac, k, shift = p[P_E4] * 48;
    int32_t air = p[P_E2] << 7, src = p[P_E3] << 8,   /* (AIR: half, 2026-10-18) */ f3 = (VW_GAIN[2] * p[P_E5]) / 127;
    tsvf_t c[3];
    uint32_t ph = v->ph[0], i, j;
    if (seg > VW_N - 2) seg = VW_N - 2;
    frac = (pos - seg * (127 << 8) / (VW_N - 1)) * (VW_N - 1);   /* 0 .. 127 << 8 within the segment */
    k = 7200 - p[P_E1] * 6600 / 127;                   /* damping Q12: RES 0 wide (~1.75) .. 127 narrow (~0.15) */
    for (j = 0; j < 3u; j++) {
        int32_t f = VW_F[seg][j] + (((VW_F[seg + 1][j] - VW_F[seg][j]) * (frac >> 8)) / 127) + shift;
        tsvf_coef_k(&c[j], clamp(f, 0, 127 << 8), k);
    }
    for (i = 0; i < n; i++) {
        int32_t x = osc_saw(ph, m->inc), y = 0;
        x += ((osc_pulse(ph, m->inc, 0x80000000u) - x) * src) >> 15;
        x += (((int32_t)noise32(&v->s[6]) >> 16) * air) >> 15;
        x >>= 1;
        y += (vowel_bp(&c[0], x, &v->s[0], &v->s[1]) * VW_GAIN[0]) >> 15;
        y += (vowel_bp(&c[1], x, &v->s[2], &v->s[3]) * VW_GAIN[1]) >> 15;
        y += (vowel_bp(&c[2], x, &v->s[4], &v->s[5]) * f3) >> 15;
        y = (y * k) >> 12;                             /* a band-pass peaks at 1 / k: level the same at any RES */
        ph += m->inc;
        out[i] += voice_amp(soft_knee(y, 24000), m, i);
    }
    v->ph[0] = ph;
}

static const engine_t ENG_VOWEL = {
    .name = "VOWEL",
    .page_title = {"VOWEL", "VOWEL +"},
    .edit = {
        {"VOWEL", F_INT, 0, 127, 30, 0, 0},
        {"RES", F_PCT, 0, 127, 80, 0, 0},
        {"AIR", F_PCT, 0, 127, 20, 0, 0},
        {"SOURCE", F_PCT, 0, 127, 0, 0, 0},
        {"SHIFT", F_BIPCT, -64, 63, 0, 0, 0},
        {"F3", F_PCT, 0, 127, 60, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = VOWEL_PRESETS,
    .npresets = NELEM(VOWEL_PRESETS),
    .knob = {0, 1, 2, 4},
    .keep = 0x3Fu,                                     /* s[0..5], the formant filters: no click on a retrigger */
    .note_on = vowel_note_on,
    .render = vowel_render,
};
