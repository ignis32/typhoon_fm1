/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* ERS: a reed's (or the lips') valve between the mouth and a bore, and the breath's window onto it (ers.h) */

typedef struct { int32_t kind, off, slope; } ers_valve_t;   /* a reed's (or the lips') table */

/* the valve between the mouth (pressure pm) and the bore (sending back pb): what goes into the bore. As STK's
 * reed table: pm + (pb - pm) x the opening; a double reed's and the lips' opening is squared (it shuts harder) */
static inline int32_t ers_valve(const ers_valve_t *k, int32_t pm, int32_t pb)
{
    int32_t pdf = clamp(pb - pm, -3 * ERS_ONE, 3 * ERS_ONE), rt;
    if (k->kind == EX_DREED || k->kind == EX_LIP) {
        int32_t r = clamp(k->off + ((k->slope * pdf) >> 14), 0, ERS_ONE);
        rt = (r * r) >> 14;
    } else
        rt = clamp(k->off + ((k->slope * pdf) >> 14), -ERS_ONE, ERS_ONE);
    return pm + ((pdf * rt) >> 14);
}

/* A valve plays in a window of pressure (STK's reed table: rt is the reflection, 1 = shut): above where its small-
 * signal gain at rest, d out / d pb, times the loop's return R passes 1, and below where it shuts. With the stiffness
 * as one (x = the pressure the valve feels) a linear valve (rt = off + x) has that gain at off + 2 x, a squared one
 * (r = off + x, rt = r^2: double reed, lips) r^2 + 2 r x. The breath is mapped onto the window: 15 % of it starts the
 * note (gain ERS_GM there), all of it brings rt to rt_max, which the knob (STIFF, LIPS, BITE) sets: 0.90 never quite
 * shuts (sweet) .. 1.05 slams (buzzy, brassy). So every setting plays, in every bore, from the same breath, on a swell
 * as on a jump. (Fixed openings and stiffnesses left the window wherever the knob put it: dead settings, and near
 * them notes that started only on a jump of breath, measured 2026-10-18: 498 of 800 onsets) */
#ifndef ERS_GM
#define ERS_GM 1126                                /* the loop gain at the start, Q10 (1.1) */
#endif
static void ers_valve_set(ers_valve_t *k, int32_t t, int32_t R, int32_t *pm0, int32_t *pmk)
{
    int32_t g = (int32_t)(((1u << 28) / (uint32_t)R * (uint32_t)ERS_GM) >> 10);   /* ERS_GM / R: the gain needed */
    int32_t rtm = 14746 + t * 19, xth, xmax, xlo;  /* rt_max: 0.90 .. 1.05 */
    if (k->kind == EX_DREED || k->kind == EX_LIP) {
        int32_t off = 4096;                        /* (0.25) */
        xth = ((int32_t)ers_isqrt((uint32_t)(off * off) + 3u * ((uint32_t)g << 14)) - 2 * off) / 3;
        xmax = (int32_t)ers_isqrt((uint32_t)rtm << 14) - off;
        k->off = off;
    } else {                                       /* (a linear valve at rt 1 is shut: silent. Its rt_max 0.88 .. 0.99) */
        rtm = 14418 + t * 15;
        k->off = clamp(2 * rtm - g - 2458, 819, 14746);   /* (0.15 inside the window) */
        xth = (g - k->off) / 2;
        xmax = rtm - k->off;
    }
    if (xmax < xth + 819)
        xmax = xth + 819;
    xlo = ((xth - ((xmax * 2458) >> 14)) * 16384) / 13926;   /* x at no breath: 15 % of the breath reaches xth */
    if (xlo < 0)
        xlo = 0;
    k->slope = -ERS_ONE;                           /* (x is the pressure itself) */
    *pm0 = xlo;
    *pmk = xmax - xlo;
}
