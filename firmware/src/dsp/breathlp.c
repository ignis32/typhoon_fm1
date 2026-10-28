/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* A breath low-pass for an engine's own output: closed to CUT at no breath, opened by breath x BR>LP, resonance RES;
 * dsp/dsp.c's trapezoidal SVF, as the mix's BREATH LP was (core/fx.c, until 2026-10-28: the same maths, on one layer
 * now). An engine takes it with three of its knobs (engine_t.blp: the first of CUT, RES, BR>LP), core/layers.c runs
 * it on the engine's block before TONE. Open (past ~84 % breath at the defaults): the sound passes as it is, the
 * filter running on at its top so it is in step when breath falls back; in or out, the block blends between them. */
typedef struct {
    int32_t s1, s2;
    uint8_t on;
} blp_t;

/* the defaults, the mix's BREATH LP's: CUT 60 (~580 Hz), RES 20, BR>LP +40 (open from ~84 % breath) */
#define BLP_KNOBS {"LP CUT", F_CUTOFF, 0, 127, 60, 0, 0}, {"LP RES", F_PCT, 0, 127, 20, 0, 0}, \
                  {"BR>LP", F_BIPCT, -64, 63, 40, 0, 0}

static void blp_block(blp_t *f, int32_t *x, int32_t cut0, int32_t res, int32_t br_lp, int32_t breath)   /* CTL frames */
{
    int32_t cut = (cut0 << 8) + ((breath * br_lp) >> 6), any = 0, on, y;
    tsvf_t c;
    uint32_t i;
    for (i = 0; i < CTL; i++)
        any |= x[i];
    if (f->s1 > 256 || f->s1 < -256 || f->s2 > 256 || f->s2 < -256)   /* (an integer filter fed silence can hold a
                                                                        * few LSB forever) */
        any = 1;
    if (!any) {                                    /* silence in and nothing left: nothing to do */
        f->s1 = f->s2 = 0;
        f->on = 0;
        return;
    }
    tsvf_coef(&c, clamp(cut, 0, 127 << 8), res);
    on = cut < (127 << 8);
    for (i = 0; i < CTL; i++) {
        y = tsvf_lp(&c, x[i], &f->s1, &f->s2);
        if (on != f->on) {
            int32_t g = on ? (int32_t)i + 1 : CTL - 1 - (int32_t)i;   /* (of CTL: the filtered one's share) */
            x[i] += ((y - x[i]) * g) >> CTL_LOG2;
        } else if (on)
            x[i] = y;
    }
    f->on = (uint8_t)on;
}
