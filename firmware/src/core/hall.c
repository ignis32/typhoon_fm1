/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* HALL, a room: early reflections (six taps a side, the walls), then a tail of eight delays mixed by a Hadamard
 * matrix (a feedback delay network), damped, two of them slowly modulated. core/reverb.c includes it (hl_layout,
 * hl_block); its delay lines are core/rvline.h's. */
/* ---- HALL: the walls (early reflections), then the room's tail (an 8-line feedback delay network) */
#define HL_SMAX 6144u                              /* SIZE's largest scale (Q12): 1.5 */
#define HL_GAIN 2
static const uint16_t HL_LEN[8] = {1559, 1877, 2203, 2593, 2939, 3301, 3637, 4001};   /* 35 .. 91 ms at SIZE 1 */
static const uint16_t HL_DIF[2] = {225, 341};
/* the reflections, samples after PRE-DLY at SIZE 1 (4 .. 44 ms), their gains (Q15) and signs */
static const uint16_t HL_ERL[6] = {190, 428, 666, 953, 1292, 1715};
static const uint16_t HL_ERR[6] = {260, 498, 785, 1076, 1460, 1927};
static const int16_t HL_ERG[6] = {27525, -23593, 19988, -16384, 13107, -10158};
#define HL_ER_MAX 2900u                            /* the last reflection at SIZE 1.5 */
#define HL_TAIL 600u                               /* the tail starts ~14 ms after the first reflections (SIZE 1) */

static void hl_layout(void)
{
    uint32_t k;
    rvl_take(&rv.pre, RV_PRE_MAX + HL_ER_MAX + 2u);
    for (k = 0; k < 2u; k++)
        rvl_take(&rv.dif[k], HL_DIF[k] + 2u);
    for (k = 0; k < 8u; k++) {
        rv.base[k] = HL_LEN[k];
        rvl_take(&rv.tank[k], HL_LEN[k] * HL_SMAX / 4096u + RV_EXC + 4u);
    }
}

static void hl_block(int32_t *l, int32_t *r, int32_t mixq, uint32_t pd)
{
    uint32_t i, k, d[8], erl[6], err[6], tail, e = rv_e_decay();
    int32_t s, kd = 32767 - sv(S_REV_DAMP) * 200;  /* the lines' LP: open .. dark */
    rv.scale += ((2048 + sv(S_REV_SIZE) * 32) - rv.scale) >> 6;      /* SIZE 0.5 .. 1.5, gliding */
    s = rv.scale;
    for (k = 0; k < 8u; k++) {
        d[k] = rv.base[k] * (uint32_t)s >> 12;
        if (d[k] + RV_EXC + 2u >= rv.tank[k].len)
            d[k] = rv.tank[k].len - RV_EXC - 3u;
        /* a pass through line k (d samples) loses 60 dB x d / FS / RT60: 2^(-9.966 d / 44100 / RT60), RT60 = 0.4 s / e:
         * x16 = 9.966 x 65536 / 44100 / 0.4 x d x e = 37.0 x d x e */
        rv.dec[k] = exp2_neg_q15((uint32_t)(umul64(umul64(d[k], 37u), e) >> 15));
    }
    for (k = 0; k < 6u; k++) {
        erl[k] = pd + (HL_ERL[k] * (uint32_t)s >> 12);
        err[k] = pd + (HL_ERR[k] * (uint32_t)s >> 12);
    }
    tail = pd + (HL_TAIL * (uint32_t)s >> 12);
    for (i = 0; i < CTL; i++) {
        int32_t x, y[8], v[8], a, b, yl = 0, yr = 0;
        uint32_t e1, e2;
        rv.lfo += 37000u;                          /* ~0.38 Hz and ~0.52 Hz: two lines sway, out of step */
        rv.lfo2 += 50600u;
        e1 = (uint32_t)((osc_sine(rv.lfo) + 32768) * (int32_t)RV_EXC);
        e2 = (uint32_t)((osc_sine(rv.lfo2 + 0x40000000u) + 32768) * (int32_t)RV_EXC);
        rvl_put(&rv.pre, ((l[i] + r[i]) >> 1) >> 1);
        for (k = 0; k < 6u; k++) {                 /* the walls: left and right apart */
            yl += mulz(rvl_at(&rv.pre, erl[k]), HL_ERG[k]);
            yr += mulz(rvl_at(&rv.pre, err[k]), HL_ERG[k]);
        }
        x = rvl_at(&rv.pre, tail);                /* into the tail: a little bandwidth, two diffusers */
        rv.bw += mulz(x - rv.bw, 26000);
        x = rvl_ap(&rv.dif[0], rv.bw, 19661, HL_DIF[0]);   /* 0.6 */
        x = rvl_ap(&rv.dif[1], x, 19661, HL_DIF[1]);
        y[0] = rvl_atf(&rv.tank[0], d[0] - RV_EXC + (e1 >> 16), (int32_t)((e1 >> 8) & 255u));
        y[1] = rvl_atf(&rv.tank[1], d[1] - RV_EXC + (e2 >> 16), (int32_t)((e2 >> 8) & 255u));
        for (k = 2; k < 8u; k++)
            y[k] = rvl_at(&rv.tank[k], d[k]);
        for (k = 0; k < 8u; k++) {                 /* damped, then the loss of a pass */
            rv.damp[k] += mulz(y[k] - rv.damp[k], kd);
            v[k] = mulz(rv.damp[k], rv.dec[k]);
        }
        for (k = 0; k < 8u; k += 2u) {             /* the Hadamard matrix (fast: three rounds of sums and differences) */
            a = v[k]; b = v[k + 1u]; v[k] = a + b; v[k + 1u] = a - b;
        }
        for (k = 0; k < 8u; k += (k & 1u) ? 3u : 1u) {
            a = v[k]; b = v[k + 2u]; v[k] = a + b; v[k + 2u] = a - b;
        }
        for (k = 0; k < 4u; k++) {
            a = v[k]; b = v[k + 4u]; v[k] = a + b; v[k + 4u] = a - b;
        }
        x = mulz(x, 11585);                        /* the input into every line, / sqrt 8 (Hadamard's first row) */
        for (k = 0; k < 8u; k++)                   /* / sqrt 8 keeps the matrix lossless */
            rvl_put(&rv.tank[k], mulz(v[k], 11585) + x);
        /* the sides from different lines (four each), so they come out apart but not against each other */
        yl += y[0] - y[2] + y[5] - y[7];
        yr += y[1] - y[3] + y[4] - y[6];
        l[i] += mulz64(yl, mixq) * HL_GAIN;
        r[i] += mulz64(yr, mixq) * HL_GAIN;
    }
}
