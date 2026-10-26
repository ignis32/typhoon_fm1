/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* ERS: the feed-forward exciters (ers.h): a sound at the note, loud as the pressure, into b->x for a resonator after
 * it (a bore's comb, MODAL, FORMANT, or none). Nothing comes back to them: in a bore a jet or a valve plays the
 * coupled models instead (ers_bores.c). */

static inline int32_t ers_exc_sample(ers_blk_t *b, uint32_t ph, int32_t pi, const tsvf_t *ce)
{
    ers_t *st = b->st;
    const vmod_t *m = b->m;
    int32_t col = b->col, e, bp, lp;
    switch (b->exc) {
    case EX_NOISE:
        st->nlp += ((((int32_t)noise32(&st->seed) >> 17) - st->nlp) * (1500 + col * 245)) >> 15;
        return st->nlp;
    case EX_REED:                                  /* a saw squeezed by the reed: buzzier as it stiffens */
        return softclip((osc_saw(ph, m->inc) * (256 + col * 16)) >> 8) >> 1;
    case EX_SAX:                                   /* ... and a little even (a cone's) */
        e = softclip((osc_saw(ph, m->inc) * (256 + col * 16)) >> 8) >> 1;
        return e + ((e * e) >> 16);
    case EX_DREED:                                 /* squeezed harder, uneven */
        e = softclip((osc_saw(ph, m->inc) * (512 + col * 24)) >> 8) >> 1;
        return e + ((e * e) >> 15);
    case EX_LIP:                                   /* a buzz: the pulse narrows as it is blown harder (brighter) */
        return osc_pulse(ph, m->inc, 0x80000000u - (uint32_t)clamp(pi, 0, ERS_ONE) * 98304u) >> 1;
    case EX_JET:                                   /* an edge tone: noise at the note, and a soft whistle */
        ers_svf(ce, (int32_t)noise32(&st->seed) >> 17, st->ex, &bp, &lp);
        return (((((bp * 400) >> 12) * 3) * col) >> 7) + (((osc_sine(ph) >> 2) * (127 - col)) >> 7);
    default:                                       /* OSC: saw .. pulse */
        e = osc_saw(ph, m->inc);
        return (e + (((osc_pulse(ph, m->inc, 0x80000000u) - e) * (col << 8)) >> 15)) >> 1;
    }
}

/* FREE REED: the reed swings at the note through its slot; the air passes in pulses while it is out of it (either
 * side, unevenly). The swing follows the breath (~8 ms: a free reed speaks softly), the slot's overlap is RASP */
static void ers_free(ers_blk_t *b)
{
    ers_t *st = b->st;
    uint32_t ph = b->v->ph[0], inc = b->m->inc, i;
    for (i = 0; i < b->n; i++) {
        int32_t pc = clamp(ers_p(b, i), 0, ERS_ONE), sw, ov, a, a2;
        st->fa += ((pc - st->fa) * 48) >> 14;
        sw = (osc_sine(ph) * st->fa) >> 15;        /* where the reed is, Q14 */
        ov = ((1638 + b->col * 90) * (8192 + (st->fa >> 1))) >> 14;   /* the overlap (0.1 .. 0.8), less when soft */
        a = sw > ov ? sw - ov : 0;
        a2 = -sw > ov ? ((-sw - ov) * 11) >> 4 : 0;
        b->x[i] = (a + a2) * 2 + (sw >> 3);        /* the air's pulses, and a little of the swing */
        ph += inc - (((inc >> 14) * (uint32_t)pc) >> 8);   /* blown harder: ~7 cents flat */
    }
    b->v->ph[0] = ph;
}

/* FM: the modulator (RATIO x the note) bends the carrier's phase by the breath's index (0.5 cycle at a full tone).
 * fb8: what comes back from a bore into the carrier's phase (ers_bore_fm), 0 here */
static const uint16_t ERS_FM_RATIO[8] = {128, 256, 384, 512, 768, 1024, 1280, 1792};   /* Q8: 0.5 1 1.5 2 3 4 5 7 */
static inline uint32_t ers_fm_inc(const ers_blk_t *b)
{
    return (uint32_t)(umul64(b->m->inc, ERS_FM_RATIO[clamp(b->col, 0, 7)]) >> 8);
}
static inline int32_t ers_fm_sample(uint32_t pc, uint32_t pm, int32_t pi, uint32_t fb)
{
    int32_t k = clamp(pi >> 3, 0, 4064);           /* the index, 1/4096 cycle */
    return osc_sine(pc + ((uint32_t)(osc_sine(pm) * k) << 5) + fb);
}
static void ers_fm(ers_blk_t *b)
{
    uint32_t pc = b->v->ph[0], pm = b->v->ph[1], im = ers_fm_inc(b), i;
    for (i = 0; i < b->n; i++) {
        int32_t pi = ers_p(b, i);
        b->x[i] = (ers_fm_sample(pc, pm, pi, 0) * pi) >> 14;
        pc += b->m->inc;
        pm += im;
    }
    b->v->ph[0] = pc;
    b->v->ph[1] = pm;
}

static void ers_excite(ers_blk_t *b)
{
    uint32_t ph = b->v->ph[0], i;
    tsvf_t ce;
    if (b->exc == EX_FREE) {
        ers_free(b);
        return;
    }
    if (b->exc == EX_FM) {
        ers_fm(b);
        return;
    }
    if (b->exc == EX_JET)
        tsvf_coef_k(&ce, ers_cut(b->m->pitch16), 400);
    for (i = 0; i < b->n; i++) {
        int32_t pi = ers_p(b, i);
        b->x[i] = (ers_exc_sample(b, ph, pi, &ce) * pi) >> 14;
        ph += b->m->inc;
    }
    b->v->ph[0] = ph;
}
