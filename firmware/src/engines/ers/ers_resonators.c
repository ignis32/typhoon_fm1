/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* ERS: the feed-forward resonators (ers.h): b->x through them, in place. A bore that a jet or a valve does not play
 * (ers_bores.c) is a comb a period long; MODAL and FORMANT are band-passes; BYPASS leaves it. */

/* the modes above the fundamental in the cutoff scale (<< 8): 32512 ln(r) / ln(16000 / 30) */
static const int16_t ERS_MODE[ERS_NM] = {0, 5250, 8735, 11337, 13421};
/* (the vowels, U O A E I, F1 F2 F3, and their gains: dsp/dspx.c DSP_VOWEL_CUT, DSP_VOWEL_GAIN, shared) */

/* a comb a period long (HOLE: half, inverted: odd harmonics); BRASS with its bell */
static void ers_comb(ers_blk_t *b)
{
    ers_t *st = b->st;
    int32_t lpb = (128 - b->bri) * (b->res == RS_PIPE ? 32 : 64), fb = b->dec * 128 > 16220 ? 16220 : b->dec * 128;
    int32_t gin = ERS_ONE - ((fb * 15) >> 4), half = b->res == RS_HOLE;
    uint32_t db = ers_len((int32_t)(half ? b->per8 >> 1 : b->per8) - (lpb >> 6), ERS_BORE), i, w = st->w;
    if (half)
        fb = -fb;
    for (i = 0; i < b->n; i++) {
        int32_t bo = ers_tap(st->bore, ERS_BORE - 1u, w, db), f, y;
        f = bo + (((st->lp - bo) * lpb) >> 14);
        st->lp = bo;
        y = ((b->x[i] * gin) >> 14) + ((f * fb) >> 14);
        st->bore[w & (ERS_BORE - 1u)] = ers_s16(y);
        if (b->res == RS_BRASS) {                  /* (the bell) */
            st->bl += ((y - st->bl) * 700) >> 14;
            y -= (st->bl * 3) >> 2;
        }
        b->x[i] = y;
        w++;
    }
    st->w = w;
}

/* five band-passes at a free bar's modes, following the note; DECAY their ring, BRIGHT how the upper ones fall */
static void ers_modal(ers_blk_t *b)
{
    int32_t c0 = ers_cut(b->m->pitch16), d = 127 - b->dec;
    int32_t k = 16 + d * d * 2400 / 16129, mk = (int32_t)ers_isqrt((uint32_t)k << 12);
    int32_t roll = 4096 + b->bri * 96, g[ERS_NM];
    uint32_t i, j;
    tsvf_t c[ERS_NM];
    for (j = 0; j < ERS_NM; j++) {
        int32_t cut = c0 + ERS_MODE[j];
        g[j] = j ? (g[j - 1] * roll) >> 14 : ERS_ONE;
        if (cut > (126 << 8) || cut < 0)
            g[j] = 0;                              /* (above ~15 kHz, or below the scale) */
        tsvf_coef_k(&c[j], cut, k);
        g[j] = (g[j] * mk) >> 12;
    }
    for (i = 0; i < b->n; i++) {
        int32_t in = b->x[i] >> 1, y = 0, bp, lp;
        for (j = 0; j < ERS_NM; j++)
            if (g[j]) {
                tsvf_step_cl(&c[j], in, b->st->md[j], &bp, &lp);
                y += (bp * g[j]) >> 14;
            }
        b->x[i] = y << 1;
    }
}

/* three vowel formants, not following the note: BRIGHT the vowel (U O A E I), DECAY their sharpness */
static void ers_formant(ers_blk_t *b)
{
    int32_t pos = b->bri << 8, seg = pos * 4 / (127 << 8), frac, k = 7200 - b->dec * 6600 / 127;
    uint32_t i, j;
    tsvf_t c[3];
    if (seg > 3) seg = 3;
    frac = (pos - seg * (127 << 8) / 4) * 4;
    for (j = 0; j < 3u; j++)
        tsvf_coef_k(&c[j], DSP_VOWEL_CUT[seg][j] + (((DSP_VOWEL_CUT[seg + 1][j] - DSP_VOWEL_CUT[seg][j]) * (frac >> 8)) / 127), k);
    for (i = 0; i < b->n; i++) {
        int32_t in = b->x[i] >> 1, y = 0, bp, lp;
        for (j = 0; j < 3u; j++) {
            tsvf_step_cl(&c[j], in, b->st->fm[j], &bp, &lp);
            y += (bp * DSP_VOWEL_GAIN[j]) >> 15;
        }
        b->x[i] = ((y * k) >> 12) << 1;
    }
}

/* one resonance that does not follow the note (a reed chamber, a mouth), over the dry sound: BRIGHT its FREQ
 * (150 Hz .. 6 kHz in the cutoff scale), DECAY its RES (wide .. narrow, the same height at any RES) */
static void ers_cavity(ers_blk_t *b)
{
    int32_t k = clamp(7200 - b->dec * 54, 300, 7200), bp, lp;
    uint32_t i;
    tsvf_t c;
    tsvf_coef_k(&c, (33 << 8) + b->bri * 150, k);
    for (i = 0; i < b->n; i++) {
        tsvf_step_cl(&c, b->x[i], b->st->md[0], &bp, &lp);
        b->x[i] = (b->x[i] >> 1) + ((bp * k) >> 12);
    }
}

static void ers_resonate(ers_blk_t *b)
{
    if (ers_is_bore(b->res) && b->exc != EX_BOW)
        ers_comb(b);
    else if (b->res == RS_MODAL)
        ers_modal(b);
    else if (b->res == RS_FORMANT)
        ers_formant(b);
    else if (b->res == RS_CAVITY)
        ers_cavity(b);
}
