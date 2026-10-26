/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* PLATE, a plate in the style of Griesinger, as J. Dattorro draws it in "Effect Design, Part 1: Reverberator and
 * Other Filters" (J. Audio Eng. Soc. 45(9), 1997): its block diagram, its delays (at 29761 Hz, scaled here to 44.1 kHz
 * and by SIZE), its coefficients and its output taps; the swaying lines read as Part 2 (45(10), 1997) reads a
 * modulated line. Also: J. O. Smith's Physical Audio Signal Processing, Schroeder 1962, Valimaki et al. 2012.
 * core/reverb.c includes it (pl_layout, pl_block); its delay lines are core/rvline.h's.
 *   the input   the mono mix, PRE-DLY, a one-pole LP (the paper's "bandwidth"), four allpasses (the input diffusers)
 *   the tank    two halves in one loop, each: an allpass that sways (decay diffusion 1), a delay, the damping LP, the
 *               decay, an allpass (decay diffusion 2), a delay, the decay; the end of each feeds the other's start,
 *               and the diffused input goes into both
 *   the output  seven taps a side off the tank's lines (the paper's table), the sides from different places */
/* ---- PLATE: the paper's numbers (its delays in samples at its rate, 29761 Hz) */
#define PL_FS 29761u                               /* the paper's sample rate: its delays x 44100 / PL_FS */
#define PL_SMAX 5096u                              /* SIZE's largest scale (Q12): 2048 + 127 x 24, x1.244 */
#define PL_BW 32751                                /* bandwidth 0.9995 (Q15): the input LP, all but open */
#define PL_ID1 24576                               /* input diffusion 1: 0.75 */
#define PL_ID2 20480                               /* input diffusion 2: 0.625 */
#define PL_DD1 22938                               /* decay diffusion 1: 0.70 (drawn with the opposite sign) */
#define PL_GAIN 2                                  /* the wet level: HALL's, on noise (tests/sim/plate.test.mjs) */
static const uint16_t PL_DIF[4] = {142, 107, 379, 277};   /* the input diffusers (fixed: SIZE scales the tank) */
/* the tank's lines, in order around the loop: the left half (0..3), then the right (4..7): the swaying allpass, the
 * delay, the second allpass, the delay */
static const uint16_t PL_TANK[8] = {672, 4453, 1800, 3720, 908, 4217, 2656, 3163};
/* the output taps: which tank line, how far back (at PL_FS, SIZE 1), the sign; summed x 0.6 (the paper's) */
static const struct { uint8_t ln; int8_t sg; uint16_t d; } PL_TAPL[7] = {
    {5, 1, 266}, {5, 1, 2974}, {6, -1, 1913}, {7, 1, 1996}, {1, -1, 1990}, {2, -1, 187}, {3, -1, 1066},
}, PL_TAPR[7] = {
    {1, 1, 353}, {1, 1, 3627}, {2, -1, 1228}, {3, 1, 2673}, {5, -1, 2111}, {6, -1, 335}, {7, -1, 121},
};

static inline uint32_t pl_at(uint32_t d, uint32_t sc)   /* a paper delay at the scale sc (Q12, 44.1 kHz in it) */
{
    return d * sc >> 12;
}

static void pl_layout(void)
{
    uint32_t k, sc = PL_SMAX * 44100u / PL_FS;
    rvl_take(&rv.pre, RV_PRE_MAX + 2u);
    for (k = 0; k < 4u; k++)
        rvl_take(&rv.dif[k], PL_DIF[k] * 44100u / PL_FS + 2u);
    for (k = 0; k < 8u; k++)                       /* (the swaying ones: room for the sweep) */
        rvl_take(&rv.tank[k], pl_at(PL_TANK[k], sc) + ((k & 3u) == 0 ? RV_EXC + 4u : 2u));
}

static void pl_block(int32_t *l, int32_t *r, int32_t mixq, uint32_t pd)
{
    uint32_t i, k, sc, d[8], dif[4], tl[7], tr[7], e = rv_e_decay();
    int32_t kd = 32767 - sv(S_REV_DAMP) * 200, dd1, dd2;   /* damping: open .. dark, as HALL's */
    rv.scale += ((2048 + sv(S_REV_SIZE) * 24) - rv.scale) >> 6;      /* SIZE 0.5 .. 1.244, gliding */
    sc = (uint32_t)rv.scale * 44100u / PL_FS;
    for (k = 0; k < 4u; k++)
        dif[k] = PL_DIF[k] * 44100u / PL_FS - 1u;  /* (an allpass of delay D reads D - 1 back: rvline.h) */
    for (k = 0; k < 8u; k++) {
        d[k] = pl_at(PL_TANK[k], sc);
        if (d[k] + ((k & 3u) == 0 ? RV_EXC + 3u : 1u) >= rv.tank[k].len)
            d[k] = rv.tank[k].len - ((k & 3u) == 0 ? RV_EXC + 4u : 2u);
    }
    /* the decay, from DECAY's RT60 (so the knob means what it means in HALL): the two multiplies in each half each
     * take the loss of the stretch of loop before them (an allpass and a delay: an allpass delays D on average),
     * 60 dB x D / FS / RT60, as HALL's 2^(-37.0 x D x e) */
    for (k = 0; k < 4u; k++)
        rv.dec[k] = exp2_neg_q15((uint32_t)(umul64(umul64(d[2u * k] + d[2u * k + 1u], 37u), e) >> 15));
    dd2 = clamp(rv.dec[0] + 4915, 8192, 16384);    /* decay diffusion 2: the decay + 0.15, kept to 0.25 .. 0.5 */
    /* decay diffusion 1 the paper's 0.7, but less for the shortest DECAYs: the swaying allpass rings on by itself
     * (0.7 a pass of ~25 ms: ~0.4 s to fall 60 dB), and DECAY 0's 0.4 s would come out longer */
    dd1 = -clamp(rv.dec[0] + 9830, 19661, PL_DD1); /* the decay + 0.3, kept to 0.6 .. 0.7; the sign: the paper's */
    for (k = 0; k < 7u; k++) {
        tl[k] = pl_at(PL_TAPL[k].d, sc);
        tr[k] = pl_at(PL_TAPR[k].d, sc);
    }
    for (i = 0; i < CTL; i++) {
        int32_t x, fl, fr, y, z, yl = 0, yr = 0;
        uint32_t e1, e2;
        rv.lfo += 97391u;                          /* ~1 Hz (the paper's): the two swaying allpasses a quarter apart */
        e1 = (uint32_t)((osc_sine(rv.lfo) + 32768) * (int32_t)RV_EXC);
        e2 = (uint32_t)((osc_sine(rv.lfo + 0x40000000u) + 32768) * (int32_t)RV_EXC);
        rvl_put(&rv.pre, ((l[i] + r[i]) >> 1) >> 1);
        x = rvl_at(&rv.pre, pd);
        rv.bw += mulz(x - rv.bw, PL_BW);
        x = rvl_ap(&rv.dif[0], rv.bw, PL_ID1, dif[0]);
        x = rvl_ap(&rv.dif[1], x, PL_ID1, dif[1]);
        x = rvl_ap(&rv.dif[2], x, PL_ID2, dif[2]);
        x = rvl_ap(&rv.dif[3], x, PL_ID2, dif[3]);
        /* the ends of the halves, read before either is written (the loop's own sample), each with its decay */
        fl = mulz(rvl_at(&rv.tank[3], d[3] - 1u), rv.dec[1]);
        fr = mulz(rvl_at(&rv.tank[7], d[7] - 1u), rv.dec[3]);
        /* the left half: the input and the right's end; its first allpass sways RV_EXC samples about its delay */
        y = rvl_apf(&rv.tank[0], x + fr, dd1, d[0] - 1u - RV_EXC / 2u + (e1 >> 16), (int32_t)((e1 >> 8) & 255u));
        z = rvl_at(&rv.tank[1], d[1] - 1u);
        rvl_put(&rv.tank[1], y);
        rv.damp[0] += mulz(z - rv.damp[0], kd);
        rvl_put(&rv.tank[3], rvl_ap(&rv.tank[2], mulz(rv.damp[0], rv.dec[0]), dd2, d[2] - 1u));
        /* the right half: the same, from the left's end */
        y = rvl_apf(&rv.tank[4], x + fl, dd1, d[4] - 1u - RV_EXC / 2u + (e2 >> 16), (int32_t)((e2 >> 8) & 255u));
        z = rvl_at(&rv.tank[5], d[5] - 1u);
        rvl_put(&rv.tank[5], y);
        rv.damp[1] += mulz(z - rv.damp[1], kd);
        rvl_put(&rv.tank[7], rvl_ap(&rv.tank[6], mulz(rv.damp[1], rv.dec[2]), dd2, d[6] - 1u));
        for (k = 0; k < 7u; k++) {                 /* the sides, each from both halves */
            yl += PL_TAPL[k].sg * rvl_at(&rv.tank[PL_TAPL[k].ln], tl[k]);
            yr += PL_TAPR[k].sg * rvl_at(&rv.tank[PL_TAPR[k].ln], tr[k]);
        }
        l[i] += mulz64(mulz64(yl, 19661), mixq) * PL_GAIN;   /* x 0.6 (seven taps: past 16 bits) */
        r[i] += mulz64(mulz64(yr, 19661), mixq) * PL_GAIN;
    }
}
