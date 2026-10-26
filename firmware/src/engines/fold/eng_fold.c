/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* FOLD: a wavefolder (after the Buchla 259's timbre circuit, done smoothly): the wave is driven harder into a sine
 * shaper, sin(pi/2 * (gain * x + bias)), which folds it back each time it passes the top. Soft breath: a round
 * tone; blown hard: more folds, brighter, buzzing, metallic. Smooth folding (a sine, not corners) keeps the
 * aliasing down without oversampling.
 *
 * Page 1 (the tone):  FOLD   the drive into the folder (what breath moves: + vmod.shape)
 *                     SYMM   a bias: uneven folds, even harmonics (a hollow, reedy colour)
 *                     WAVE   the source, sine .. triangle (triangle folds brighter)
 *                     SUB    a sine an octave below, under the folds
 * Page 2 (colour):    TONE   a gentle low-pass after the folder (127 = open)
 *                     AIR    breath noise into the folder (it folds too: a breathy edge) */

static const preset_t FOLD_PRESETS[] = {
    {"ROUND", {20, 0, 0, 30, 110, 5, 0, 0}, {0, 0, 127, 20}, 0, 1, {0}, 0},
    {"REEDY", {45, 30, 40, 0, 90, 15, 0, 0}, {0, 0, 127, 20}, 0, 1, {0}, 0},
    {"METAL", {70, -20, 127, 0, 127, 0, 0, 0}, {0, 0, 127, 30}, 0, 1, {0}, 0},
};

static void fold_note_on(track_t *t, voice_t *v)
{
    (void)t;
    if (!v->s[1])
        v->s[1] = 0x27182818 + (int32_t)v->age;        /* noise seed */
}

static void fold_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t fold = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8);
    int32_t g = 256 + (fold >> 4);                     /* drive x1 .. ~x9 (Q8) */
    int32_t bias = p[P_E1] << 8, tri = p[P_E2] << 8, sub = p[P_E3] << 8, air = (p[P_E5] * 19) >> 1;   /* AIR 127 = the first 7.5 % (too loud: 2026-10-16 to 15 %, 2026-10-18 half that) */
    int32_t lp = 1200 + p[P_E4] * 250;                 /* TONE: a one-pole's step, Q15 (~127: almost open) */
    uint32_t ph = v->ph[0], ps = v->ph[1], i;
    for (i = 0; i < n; i++) {
        int32_t x = osc_sine(ph), y, s;
        x += ((osc_tri(ph) - x) * tri) >> 15;          /* WAVE: sine .. triangle */
        x += (((int32_t)noise32(&v->s[1]) >> 17) * air) >> 15;
        /* the folder: u in quarter cycles of the shaper's sine (Q15: 1.0 = a quarter), any size: it wraps = folds */
        y = osc_sine((uint32_t)(((x * g) >> 8) + bias) << 15);
        v->s[0] += ((y - v->s[0]) * lp) >> 15;         /* TONE */
        s = (v->s[0] * 3) >> 2;
        s += (osc_sine(ps) * sub) >> 16;               /* SUB, up to half */
        ph += m->inc;
        ps += m->inc >> 1;
        out[i] += voice_amp(s, m, i);
    }
    v->ph[0] = ph;
    v->ph[1] = ps;
}

static const engine_t ENG_FOLD = {
    .name = "FOLD",
    .page_title = {"FOLD", "FOLD +"},
    .edit = {
        {"FOLD", F_PCT, 0, 127, 30, 0, 0},
        {"SYMM", F_BIPCT, -64, 63, 0, 0, 0},
        {"WAVE", F_PCT, 0, 127, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"TONE", F_PCT, 0, 127, 110, 0, 0},
        {"AIR", F_PCT, 0, 127, 8, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = FOLD_PRESETS,
    .npresets = NELEM(FOLD_PRESETS),
    .knob = {0, 1, 2, 4},
    .keep = 0x01u,                                     /* s[0], the TONE filter: no click on a retrigger */
    .note_on = fold_note_on,
    .render = fold_render,
};
