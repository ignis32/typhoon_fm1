/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The reverbs' delay lines (core/reverb.c includes it after rv_mem, rv_at; fx.c's sat16 / mulz). From J. O. Smith's
 * Physical Audio Signal Processing (the Schroeder allpass) and Dattorro's Effect Design Parts 1 and 2 (the allpass as he draws it, a modulated line read between two samples).
 * A line is a ring of int16_t samples taken from the pool at layout (rvl_take), written one at a time, the newest
 * at w; it is read d samples back (0: the newest). */
typedef struct {
    int16_t *buf;
    uint16_t len, w;                               /* its length, samples; where the newest is */
} rvl_t;

static void rvl_take(rvl_t *ln, uint32_t len)      /* the next len samples of rv_mem (past RV_MEM: it won't run) */
{
    ln->buf = rv_mem + (rv_at < RV_MEM ? rv_at : 0u);
    ln->len = (uint16_t)len;
    ln->w = 0;
    rv_at += len;
}

static inline void rvl_put(rvl_t *ln, int32_t x)  /* x the newest, clamped to 16 bits */
{
    ln->w = ln->w + 1u >= ln->len ? 0 : (uint16_t)(ln->w + 1u);
    ln->buf[ln->w] = (int16_t)sat16(x);
}

static inline int32_t rvl_at(const rvl_t *ln, uint32_t d)   /* written d writes before the newest (d < len) */
{
    return ln->buf[ln->w >= d ? ln->w - d : ln->w + ln->len - d];
}

static inline int32_t rvl_atf(const rvl_t *ln, uint32_t d, int32_t frac)   /* d + frac / 256 back: a straight line */
{
    int32_t a = rvl_at(ln, d), b = rvl_at(ln, d + 1u);
    return a + (((b - a) * frac) >> 8);
}

/* a Schroeder allpass of delay d + 1 on the line (the line holds its inner signal w): v the oldest, w = x + g v in,
 * v - g w out; g in Q15, either sign. Dattorro's figures draw it so */
static inline int32_t rvl_ap(rvl_t *ln, int32_t x, int32_t g, uint32_t d)
{
    int32_t v = rvl_at(ln, d), w = x + mulz(g, v);
    rvl_put(ln, w);
    return v - mulz(g, sat16(w));
}

/* the same with its delay between two samples, d + frac / 256 (PLATE's two swaying ones: Dattorro Part 2 reads a
 * modulated line so, linearly; an allpass interpolator would keep the highs, but its state jumps as d steps) */
static inline int32_t rvl_apf(rvl_t *ln, int32_t x, int32_t g, uint32_t d, int32_t frac)
{
    int32_t v = rvl_atf(ln, d, frac), w = x + mulz(g, v);
    rvl_put(ln, w);
    return v - mulz(g, sat16(w));
}
