/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* FM: two sine operators, the modulator's index breath-driven (the classic breath-controller sound: soft is pure,
 * blown hard it turns brassy), with modulator feedback and a little air.
 *
 * Page 1 (the tone):  INDEX  how much the modulator bends the carrier (what breath moves: + vmod.shape)
 *                     RATIO  the modulator's frequency against the carrier's
 *                     FEEDBK the modulator fed back into itself (saw-like, then noisy)
 *                     DETUNE the modulator a few cents off (slow beating)
 * Page 2 (colour):    AIR    breath noise mixed in
 *                     KEYTRK the index falls with pitch (high notes stay clean), 0 = off
 * Phase modulation in 32-bit phase: an index of 1.0 cycle (2 pi rad) at INDEX 127. */

static const char *const N_FM_RATIO[] = {"0.5", "1", "1.5", "2", "3", "4", "5", "7"};
static const uint16_t FM_RATIO_Q8[8] = {128, 256, 384, 512, 768, 1024, 1280, 1792};

/* (EDIT 5, the breath LP, at the mix's old one: LP CUT 60, LP RES 20, BR>LP +40) */
static const preset_t FM_PRESETS[] = {
    {"BRASS", {40, 1, 10, 0, 10, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 20, 40}, {0, 0, 127, 20}, 0, 1, {0}, 0},
    {"REED", {30, 3, 30, 0, 20, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 20, 40}, {0, 0, 127, 20}, 0, 1, {0}, 0},
    {"BELL", {50, 6, 0, 6, 0, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 20, 40}, {0, 0, 127, 40}, 0, 1, {0}, 0},
};

static void fm_note_on(track_t *t, voice_t *v)
{
    (void)t;
    if (!v->s[1])
        v->s[1] = 0x31415927 + (int32_t)v->age;        /* noise seed */
}

static void fm_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t idx = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8);   /* 0..32512 */
    int32_t fb = p[P_E2] << 5, air = p[P_E4] << 7, i0 = v->s[2], di;   /* (AIR: half what it was, 2026-10-18) */
    uint32_t inc_m = (uint32_t)(umul64(m->inc, FM_RATIO_Q8[p[P_E1] & 7]) >> 8), pc = v->ph[0], pm = v->ph[1], i;
    inc_m += (uint32_t)((int32_t)(inc_m >> 12) * CENTS_FINE(p[P_E3]));   /* DETUNE: cents */
    if (p[P_E5] > 0) {                                 /* KEYTRK: the index halves an octave and a half above C4 */
        int32_t over = clamp(m->pitch16 - 60 * 16, 0, 36 * 16), f = over * p[P_E5] / 146;   /* 0..~501 of 1024 */
        idx -= (idx * f) >> 10;                        /* (32-bit: no 64-bit division on this chip) */
    }
    di = (idx - i0) / (int32_t)n;                      /* the index glides over the block: no zipper */
    for (i = 0; i < n; i++) {
        int32_t mo = osc_sine(pm + (uint32_t)(v->s[0] * fb)), s;   /* the modulator, its own last output fed back */
        int32_t k = (i0 + di * (int32_t)i) >> 3;       /* 0..4064 */
        v->s[0] = mo >> 6;
        s = osc_sine(pc + ((uint32_t)(mo * k) << 5));  /* carrier, phase-modulated: wraps are the modulo of a phase */
        s += (noise_q14(&v->s[1]) * air) >> 15;
        pc += m->inc;
        pm += inc_m;
        out[i] += voice_amp((s * 3) >> 2, m, i);
    }
    v->ph[0] = pc;
    v->ph[1] = pm;
    v->s[2] = idx;
}

static const engine_t ENG_FM = {
    .name = "FM",
    .page_title = {"FM", "FM +", 0, 0, "BREATH LP"},
    .edit = {
        {"INDEX", F_PCT, 0, 127, 40, 0, 0},
        {"RATIO", F_ENUM, 0, 7, 1, N_FM_RATIO, 0},
        {"FEEDBK", F_PCT, 0, 127, 0, 0, 0},
        {"DETUNE", F_INT, -50, 50, 0, 0, "ct"},
        {"AIR", F_PCT, 0, 127, 10, 0, 0},
        {"KEYTRK", F_PCT, 0, 127, 40, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
        [16] = BLP_KNOBS,                              /* EDIT 5: the breath LP (dsp/breathlp.c; the mix's until 2026-10-28) */
    },
    .presets = FM_PRESETS,
    .npresets = NELEM(FM_PRESETS),
    .knob = {0, 1, 2, 4},
    .keep = 0x04u,                                     /* s[2], the index now: no jump on a retrigger */
    .blp = 17,                                         /* (EDIT 16 .. 18) */
    .note_on = fm_note_on,
    .render = fm_render,
};
