/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* TONE: a stage after every layer's engine, before its level and pan (core/layers.c layers_block): for an ERS layer
 * the fourth stage after its shaper (E -> R -> S -> T), for any other engine a tone of its own. Breath-driven tone, as
 * wind-controller players use it. P_TONE picks it, P_TONE_A .. C are its knobs (their labels follow the type:
 * tone_desc); BR>TONE (P_BR_FLT, breath -> tone) moves FILTER's cutoff too.
 *   OFF      the engine as it is
 *   FILTER   a resonant filter: CUTOFF (+ breath x BR>TONE), RES, MODE (LP BP HP)
 *   SWEEP    a low-pass that opens by how fast breath rises, not how much (the EWI4000s's sweep): every attack bites
 *            bright and settles to CUTOFF; DEPTH, SPEED (how fast it settles)
 *   TALK     a talkbox: three vowel formants, the vowel moved by breath: VOWEL (where soft breath is, U O A E I),
 *            DEPTH (how far breath moves it), RES
 *   SUB OCT  an octave below (the Steiner EVI's / Akai EWI's sub voice) at the layer's loudness: LEVEL, BREATH (how
 *            much more of it breath brings: 0 always the same), WAVE (sine .. square)
 * A type's knobs start at its defaults when the type is chosen (params.c par_set). State per layer, cleared when the
 * type changes. */

enum { TN_OFF, TN_FILTER, TN_SWEEP, TN_TALK, TN_SUB, TN_COUNT };
static const char *const N_TONE[TN_COUNT] = {"OFF", "FILTER", "SWEEP", "TALK", "SUB OCT"};
static const char *const N_TONE_MODE[3] = {"LP", "BP", "HP"};

/* -- TONE's fixed numbers (the rule: a knob, or here, with why) */
#define TONE_SWEEP_DESTEP_MS 15u                   /* SWEEP's rise is read from breath de-stepped over 15 ms (dsp/dspx.c
                                                    * ctl_destep), 2026-10-28: the re.corder's ~50 breath steps a second
                                                    * each fired the accent (a "brbrbr" flutter at soft breath, where the
                                                    * filter rests under the note). 15 ms is under a step's 20 and inside
                                                    * a tongued attack's own ~40 ms ramp. Only the accent's bite is
                                                    * spread so; the level and BR>TONE answer breath at once */

static struct {
    uint8_t type;                                  /* the type the state is for */
    int32_t f[3][2];                               /* the filters' states */
    int32_t sw, br, bd;                            /* SWEEP: the accent; the last block's breath, de-stepped; its
                                                    * de-stepper */
    uint32_t sub;                                  /* SUB OCT: its phase */
} tone_st[NLAYER];

/* the knobs' meaning (A, B, C) for a type; their defaults are a new type's (tone_defaults) */
static const param_desc_t *tone_desc(int32_t type, uint32_t k)
{
    static const param_desc_t D[TN_COUNT][3] = {
        [TN_OFF] = {{"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}},
        [TN_FILTER] = {{"CUTOFF", F_CUTOFF, 0, 127, 80, 0, 0}, {"RES", F_PCT, 0, 127, 40, 0, 0},
                       {"MODE", F_ENUM, 0, 2, 0, N_TONE_MODE, 0}},
        [TN_SWEEP] = {{"CUTOFF", F_CUTOFF, 0, 127, 50, 0, 0}, {"DEPTH", F_PCT, 0, 127, 90, 0, 0},
                      {"SPEED", F_PCT, 0, 127, 60, 0, 0}},
        [TN_TALK] = {{"VOWEL", F_PCT, 0, 127, 20, 0, 0}, {"DEPTH", F_BIPCT, -64, 63, 40, 0, 0},
                     {"RES", F_PCT, 0, 127, 70, 0, 0}},
        [TN_SUB] = {{"LEVEL", F_PCT, 0, 127, 70, 0, 0}, {"BREATH", F_PCT, 0, 127, 40, 0, 0},
                    {"WAVE", F_PCT, 0, 127, 20, 0, 0}},
    };
    return &D[(uint32_t)clamp(type, 0, TN_COUNT - 1)][k % 3u];
}
static void tone_defaults(int16_t *p)              /* the knobs at their type's defaults */
{
    uint32_t k;
    for (k = 0; k < 3u; k++)
        p[P_TONE_A + k] = tone_desc(p[P_TONE], k)->def;
}

/* (the vowels and the SVF's step: dsp/dspx.c DSP_VOWEL_CUT, DSP_VOWEL_GAIN, tsvf_step_cl, shared; TONE's own copies
 * until 2026-10-28) */
static inline int32_t tone_k(int32_t res) { return 7200 - clamp(res, 0, 127) * 52; }   /* RES: damping 1.76 .. 0.13
                                                                                         * (Q12; FILTER's, TALK's) */

/* layer l's block (buf, n samples, after its engine): through its TONE. breath Q15; m its modulation (inc, amp) */
static void tone_block(uint32_t l, int32_t *buf, uint32_t n, const int16_t *p, const vmod_t *m, int32_t breath)
{
    int32_t type = clamp(p[P_TONE], 0, TN_COUNT - 1), a = p[P_TONE_A], b = p[P_TONE_B], c = p[P_TONE_C], bp, lp;
    uint32_t i, j;
    tsvf_t f[3];
    if (type != tone_st[l].type) {                 /* another type: a clean state */
        memset(&tone_st[l], 0, sizeof tone_st[l]);
        tone_st[l].type = (uint8_t)type;
        tone_st[l].br = tone_st[l].bd = breath;
    }
    switch (type) {
    case TN_FILTER: {
        int32_t k = tone_k(b), mode = clamp(c, 0, 2);
        tsvf_coef_k(&f[0], clamp((a << 8) + m->cutoff, 0, 127 << 8), k);
        for (i = 0; i < n; i++) {
            tsvf_step_cl(&f[0], buf[i], tone_st[l].f[0], &bp, &lp);
            buf[i] = mode == 0 ? lp : mode == 1 ? (bp * k) >> 12 : buf[i] - ((bp * k) >> 12) - lp;
        }
        break;
    }
    case TN_SWEEP: {                               /* the accent: breath's rise (de-stepped), settling at SPEED */
        int32_t bd = ctl_destep(&tone_st[l].bd, breath, TONE_SWEEP_DESTEP_MS), rise = bd - tone_st[l].br;
        tone_st[l].br = bd;
        if (rise > 0)
            tone_st[l].sw = clamp(tone_st[l].sw + rise * 4, 0, 32767);
        tone_st[l].sw -= (tone_st[l].sw * (8 + c * 3)) >> 12;   /* SPEED 0: ~1 s .. 127: ~60 ms */
        tsvf_coef_k(&f[0], clamp((a << 8) + ((tone_st[l].sw * b) >> 8) + m->cutoff, 0, 127 << 8), 2600);
        for (i = 0; i < n; i++) {
            tsvf_step_cl(&f[0], buf[i], tone_st[l].f[0], &bp, &lp);
            buf[i] = lp;
        }
        break;
    }
    case TN_TALK: {                                /* the vowel: VOWEL + breath x DEPTH, U .. I */
        int32_t pos = clamp((a << 8) + ((breath * b) >> 6), 0, 127 << 8), seg = pos * 4 / (127 << 8), frac;
        int32_t k = tone_k(c);
        if (seg > 3) seg = 3;
        frac = (pos - seg * (127 << 8) / 4) * 4;
        for (j = 0; j < 3u; j++)
            tsvf_coef_k(&f[j], DSP_VOWEL_CUT[seg][j] + (((DSP_VOWEL_CUT[seg + 1][j] - DSP_VOWEL_CUT[seg][j]) * (frac >> 8)) / 127), k);
        for (i = 0; i < n; i++) {
            int32_t y = 0;
            for (j = 0; j < 3u; j++) {
                tsvf_step_cl(&f[j], buf[i] >> 1, tone_st[l].f[j], &bp, &lp);
                y += (bp * DSP_VOWEL_GAIN[j]) >> 15;
            }
            buf[i] = ((y * k) >> 12) << 1;
        }
        break;
    }
    case TN_SUB: {                                 /* an octave below at the layer's loudness (m->amp1, Q15) */
        int32_t lev = (a * ((127 - b) * 256 + ((b * breath) >> 7))) >> 7;   /* Q15-ish: LEVEL x (1 - B + B x breath) */
        int32_t g = (int32_t)(mul64(lev, m->amp1) >> 15), w = c << 8;
        uint32_t inc = m->inc >> 1, ph = tone_st[l].sub;
        for (i = 0; i < n; i++) {
            int32_t s = osc_sine(ph);
            s += ((osc_pulse(ph, inc, 0x80000000u) - s) * w) >> 15;   /* WAVE: sine .. square */
            buf[i] += (s * (g >> 1)) >> 15;
            ph += inc;
        }
        tone_st[l].sub = ph;
        break;
    }
    }
}
