/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* ERS: the shapers (ers.h): b->x through the chosen one, DC out, into the voice's output. And the growl's flutter,
 * which is in the pressure: worked out before the exciter (ers_growl). */

/* MUTE: straight, cup, harmon: cutoff (the cutoff scale), damping (Q12), the LP BP HP gains (Q14) */
static const int16_t ERS_MUTE[3][5] = {
    {19900, 2900, 0, 9830, 13107}, {17611, 4096, 16384, 4915, 1638}, {21201, 614, 0, 16384, 4915},
};

/* GROWL: the pressure's gain this block (Q14): it flutters ~20 .. 40 Hz, unevenly, down to ~1/2 */
static int32_t ers_growl(ers_t *st, int32_t amt)
{
    uint32_t r = noise32(&st->seed);
    int32_t d = amt >> 1, lfo;
    st->gph += 54000000u + (r & 0x3FFFFFFu);
    lfo = sine_i(st->gph);
    return ERS_ONE - ((d * ((32767 + lfo) >> 1)) >> 16);
}

/* the shapers' filters and gains for this block */
typedef struct {
    tsvf_t c[3];
    int32_t g, a14, sk, gl, gb, gh;
} ers_shp_t;

static void ers_shape_setup(const ers_blk_t *b, ers_shp_t *s)
{
    static const int16_t BODY_CUT[3] = {10979, 16310, 22238};   /* 250 700 2200 Hz */
    int32_t amt = b->amt;
    uint32_t j;
    s->g = 256 + (b->shp == SH_SAT ? amt >> 3 : amt >> 4);
    s->a14 = amt >> 1;
    s->sk = s->gl = s->gb = s->gh = 0;
    switch (b->shp) {
    case SH_FILTER: tsvf_coef(&s->c[0], amt + ((b->m->pitch16 - 60 * 16) * 9), 30); break;
    case SH_BELL: tsvf_coef_k(&s->c[0], 22238, s->sk = 2048); break;   /* a presence peak ~2.2 kHz */
    case SH_MUTE: {                                /* straight .. cup .. harmon */
        int32_t i = amt < (64 << 8) ? 0 : 1, f = i ? (amt - (64 << 8)) * 4 / 63 : amt * 4 / 64;   /* Q10 */
        const int16_t *a = ERS_MUTE[i], *z = ERS_MUTE[i + 1];
#define MIX(q) (a[q] + (((z[q] - a[q]) * f) >> 10))
        s->sk = MIX(1);
        tsvf_coef_k(&s->c[0], MIX(0), s->sk);
        s->gl = MIX(2); s->gb = MIX(3); s->gh = MIX(4);
#undef MIX
        break;
    }
    case SH_WAH: tsvf_coef_k(&s->c[0], 13412 + ((amt * 103) >> 8), s->sk = 1400); break;   /* 400 Hz .. 5 kHz */
    case SH_BODY:
        for (j = 0; j < 3u; j++)
            tsvf_coef_k(&s->c[j], BODY_CUT[j], 1000);
        break;
    case SH_AIR: tsvf_coef_k(&s->c[0], ers_cut(b->m->pitch16) + 3589, s->sk = 5000); break;   /* around 2 f, wide */
    }
}

static inline int32_t ers_shape_sample(ers_blk_t *b, const ers_shp_t *s, int32_t x, uint32_t i)
{
    static const int16_t BODY_G[3] = {9830, 8192, 4915};
    ers_t *st = b->st;
    int32_t y, bp, lp, a14 = s->a14, sk = s->sk;
    uint32_t j;
    switch (b->shp) {
    case SH_SAT: return softclip((x * s->g) >> 7) >> 1;
    case SH_FOLD: return osc_sine((uint32_t)(((clamp(x, -4 * ERS_ONE, 4 * ERS_ONE) << 1) * s->g) >> 8) << 15) >> 1;
    case SH_FILTER: return tsvf_lp(&s->c[0], x, &st->sh[0][0], &st->sh[0][1]);
    case SH_BELL:                                  /* less low end (a one-pole's low part out), a presence peak */
        st->sh[1][0] += ((x - st->sh[1][0]) * 700) >> 14;
        ers_svf(&s->c[0], x, st->sh[0], &bp, &lp);
        return x - ((st->sh[1][0] * a14) >> 15) + ((((bp * sk) >> 12) * a14) >> 15);
    case SH_MUTE:
        ers_svf(&s->c[0], x, st->sh[0], &bp, &lp);
        return (lp * s->gl + ((bp * sk) >> 12) * s->gb + (x - ((sk * bp) >> 12) - lp) * s->gh) >> 14;
    case SH_WAH:
        ers_svf(&s->c[0], x, st->sh[0], &bp, &lp);
        return lp + ((bp * sk) >> 12);
    case SH_BODY:
        y = x;
        for (j = 0; j < 3u; j++) {
            ers_svf(&s->c[j], x, st->sh[j], &bp, &lp);
            y += (((((bp * 1000) >> 12) * BODY_G[j]) >> 14) * a14) >> 14;
        }
        return y;
    case SH_AIR: {                                 /* breath noise around twice the note (half, 2026-10-18) */
        int32_t nz = (((ers_air(st, 18) * ers_p(b, i)) >> 14) * a14) >> 14;
        ers_svf(&s->c[0], nz, st->sh[0], &bp, &lp);
        return x + (((bp * sk) >> 12) >> 1);
    }
    case SH_GROWL:                                 /* (the flutter is in the pressure) + a half-pitch roughness */
        st->sub += b->m->inc >> 1;
        return x + ((((st->sub & 0x80000000u) ? -x : x) * (a14 >> 1)) >> 14);
    }
    return x;                                      /* BYPASS */
}

/* the shaper, DC out, into the voice's output */
static void ers_shape(ers_blk_t *b, int32_t *out)
{
    ers_t *st = b->st;
    ers_shp_t s;
    uint32_t i;
    ers_shape_setup(b, &s);
    for (i = 0; i < b->n; i++) {
        int32_t x = ers_shape_sample(b, &s, b->x[i], i), y;
        if (b->tilt) {                             /* TILT: around ~800 Hz, -1 (the low-pass) .. +1 (+6 dB above) */
            st->tl += ((x - st->tl) * 1753) >> 14;
            x += ((x - st->tl) * b->tilt) >> 14;
        }
        y = x - st->ox + ((st->oy * 16300) >> 14);   /* DC out */
        st->ox = x;
        st->oy = y;
        out[i] += voice_amp(soft_knee(clamp(y << 1, -(1 << 20), 1 << 20), 28000), b->m, i);
    }
}
