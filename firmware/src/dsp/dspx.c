/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* typhoon's shared fixed-point DSP, beside Felucca-Engines' dsp/dsp.c (its oscillators, noise, soft clip and the
 * SVF's coefficients): what several parts had a copy of each, in one place (2026-10-28). The float twin is
 * dsp/dspf.c (EVA's). Every one of these was moved as it was, step for step: the sound did not change.
 *
 *   the SVF's step     tsvf_lp (the low-pass alone: Felucca's, moved here), tsvf_step (band-pass and low-pass;
 *                      high-pass = in - k bp - lp), tsvf_step_cl (the same, its input held to +-65536 first)
 *                      (were: dsp.c tsvf_lp, ANALOG's analog_tap, ERS's ers_svf, TONE's tone_svf)
 *   the vowels         DSP_VOWEL_CUT (U O A E I, F1 F2 F3, the filters' cutoff scale), DSP_VOWEL_GAIN, DSP_VOWEL_HZ
 *                      (the same in Hz, for float engines) (were: TONE's TN_VF / TN_VG, ERS's ERS_VF / ERS_VG,
 *                      EVA's EVA_F1 / EVA_F2)
 *   one-poles by time  pole_k (Q15 a control block, the layer's: envelope, SMOOTH), pole_q14 (Q14 a block, rounded:
 *                      ERS's VIB SWELL) (were: core/layers.c pole_k, ERS's ers_pole; two roundings, kept so as they
 *                      were)
 *   conversions        Q15_127 (a 0 .. 127 knob as a Q15 level), CENTS_FINE / CENTS16_FINE (cents as m.fine's units),
 *                      tsvf_k (RES 0 .. 127 as the SVF's damping), and tsvf_coef with it (Felucca's, moved here)
 *   noise              noise_q14: dsp.c's noise32 as +-16384 (ERS's, FM's) */

/* ---- conversions */
#define Q15_127(v) ((v) * 258)                     /* a 0 .. 127 knob as Q15 (127: 32766) */
#define CENTS_FINE(c) ((c) * 2367 / 1000)          /* cents as 1/4096 of a phase increment (m.fine): x 4096 ln2 / 1200 */
#define CENTS16_FINE(c16) ((c16) * 2367 / 16000)   /* the same, from 1/16 cents */
static inline int32_t tsvf_k(int32_t reso) { return 8192 - reso * 7600 / 127; }   /* RES 0 .. 127: damping 2.0 ..
                                                                                    * ~0.15, Q12 */
static inline void tsvf_coef(tsvf_t *c, int32_t cut, int32_t reso)   /* (Felucca's, moved beside tsvf_k) */
{
    tsvf_coef_k(c, cut, tsvf_k(reso));
}

/* ---- noise */
static inline int32_t noise_q14(int32_t *st) { return (int32_t)noise32(st) >> 17; }   /* white, +-16384 */

/* ---- the trapezoidal SVF (A. Simper): coefficients dsp.c tsvf_coef / tsvf_coef_k (Q13), signals within +-150000 */
static inline int32_t tsvf_step(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2, int32_t *bp)   /* -> lp */
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    *bp = v1;
    return v2;
}
static inline int32_t tsvf_lp(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2)   /* the low-pass alone */
{
    int32_t bp;
    return tsvf_step(c, in, ic1, ic2, &bp);
}
/* its input held to +-65536 first (a resonator fed a loud signal: the products stay within 32 bits); s: the state */
static inline void tsvf_step_cl(const tsvf_t *c, int32_t in, int32_t *s, int32_t *bp, int32_t *lp)
{
    *lp = tsvf_step(c, clamp(in, -65536, 65536), &s[0], &s[1], bp);
}

/* ---- the vowels: U O A E I (dark .. bright), a man's voice (Peterson & Barney 1952's averages) */
static const int16_t DSP_VOWEL_CUT[5][3] = {      /* F1 F2 F3 in the filters' cutoff scale << 8 (tools/gen_engine_tables.py
                                                    * cutoff_hz: v = 127 ln(f / 30) / ln(16000 / 30)) */
    {11922, 17435, 22332},                         /* U  300  870 2240 Hz */
    {15246, 17253, 22711},                         /* O  570  840 2410 */
    {16527, 18602, 22775},                         /* A  730 1090 2440 */
    {14869, 21313, 22859},                         /* E  530 1840 2480 */
    {11377, 22446, 23862},                         /* I  270 2290 3010 */
};
static const int16_t DSP_VOWEL_GAIN[3] = {32767, 20000, 11000};   /* F1 loudest, as in speech (Q15) */
static const float DSP_VOWEL_HZ[5][3] = {          /* the same, Hz */
    {300.f, 870.f, 2240.f}, {570.f, 840.f, 2410.f}, {730.f, 1090.f, 2440.f}, {530.f, 1840.f, 2480.f},
    {270.f, 2290.f, 3010.f},
};
enum { DSP_VOWEL_U, DSP_VOWEL_O, DSP_VOWEL_A, DSP_VOWEL_E, DSP_VOWEL_I };

/* ---- a one-pole's step for a time, at the control rate (BLOCKS_PER_S blocks a second) */
#define BLOCKS_PER_S (FS / CTL)                    /* control blocks a second (1378) */
static int32_t pole_k(uint32_t ms)                 /* Q15 a block, for a time constant in ms (the layer's) */
{
    uint32_t den = ms * (BLOCKS_PER_S / 10u) / 100u + 1u;   /* blocks in ms */
    return den <= 1u ? 32768 : (int32_t)(32768u / den);
}
static int32_t pole_q14(int32_t ms)                /* Q14 a block, 16384 / (1 + ms x 1.378), rounded (ERS's swell) */
{
    int32_t d = 1000 + ms * 1378;
    return (16384000 + d / 2) / d;
}
