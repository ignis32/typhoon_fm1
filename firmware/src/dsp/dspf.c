/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* Shared DSP building blocks in floating point, for engines on the AC79's single-precision FPU (no double, no libm:
 * exp2 / log2 / tan / sin by polynomials). dsp/dsp.c is the fixed-point set (Felucca-Engines'); this is its float
 * twin, so an engine takes its filter, oscillators and noises from here instead of writing them again (EVA first,
 * 2026-10-28: these were its own).
 *
 *   maths     dspf_exp2, dspf_log2 (~1e-4), dspf_pow, dspf_sqrt, dspf_clamp, dspf_sin (sin 2 pi p), dspf_sat (tanh-like)
 *   random    dspf_rand, dspf_urand (-1 .. 1)
 *   (the oscillators and the noises: dsp/oscf.c, built on these)
 *   filter    a state-variable filter, trapezoidal (A. Simper's): dspf_svf_coef(fc, k) per block, dspf_svf a
 *             sample: its band-pass returned, its low-pass through lp; the high-pass is x - lp - k bp. k is the
 *             damping (2: none .. ~0.1: ringing); k bp is 1 at the centre. Coefficients may glide over a block:
 *             dspf_svf takes them as numbers */

typedef union { float f; uint32_t u; } dspf_fu;
static inline float dspf_exp2(float x)
{
    dspf_fu v;
    float fl, f;
    int32_t i;
    if (x < -126.f) return 0.f;
    if (x > 127.f) x = 127.f;
    i = (int32_t)x;
    if ((float)i > x) i--;
    fl = (float)i;
    f = x - fl;
    v.u = (uint32_t)(i + 127) << 23;
    return v.f * (1.f + f * (0.6957f + f * (0.2262f + f * 0.0782f)));
}
static inline float dspf_log2(float x)             /* x > 0 */
{
    dspf_fu v;
    float m;
    int32_t e;
    v.f = x;
    e = (int32_t)((v.u >> 23) & 255u) - 127;
    v.u = (v.u & 0x007FFFFFu) | 0x3F800000u;
    m = v.f;
    return (float)e - 1.7417939f + (2.8212026f + (-1.4699568f + (0.44717955f - 0.056570851f * m) * m) * m) * m;
}
static inline float dspf_pow(float y, float e) { return y <= 0.f ? 0.f : dspf_exp2(e * dspf_log2(y)); }
static inline float dspf_sqrt(float x)             /* (x <= 0: 0) one guess from the exponent, two Newton steps */
{
    dspf_fu v;
    float y;
    if (x <= 0.f) return 0.f;
    v.f = x;
    v.u = (v.u >> 1) + 0x1FC00000u;
    y = v.f;
    y = 0.5f * (y + x / y);
    return 0.5f * (y + x / y);
}
static inline float dspf_clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float dspf_sat(float x)              /* tanh-like, x in -3 .. 3 */
{
    x = dspf_clamp(x, -3.f, 3.f);
    return x * (27.f + x * x) / (27.f + 9.f * x * x);
}
static inline float dspf_sin(float p)              /* sin(2 pi p), p in 0 .. 1 (parabolic, then corrected) */
{
    float q = p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p, y = 1.f - q * q;   /* (0 at p 0 / 0.5, peaks at 1/4, 3/4) */
    y = p < 0.5f ? y : -y;
    return y * (0.775f + 0.225f * (y < 0.f ? -y : y));
}

/* ---- random numbers (the noises: dsp/oscf.c) */
static inline uint32_t dspf_rand(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }
static inline float dspf_urand(uint32_t *s) { return (float)(int32_t)dspf_rand(s) * (1.f / 2147483648.f); }   /* -1 .. 1 */

/* ---- the state-variable filter */
typedef struct { float a1, a2, a3; } dspf_svf_t;
static void dspf_svf_coef(float fc, float k, dspf_svf_t *c)   /* fc in Hz (below 0.45 fs), k the damping */
{
    float x = 3.14159265f * fc / (float)FS, x2 = x * x, g = x * (15.f - x2) / (15.f - 6.f * x2);   /* tan(pi fc / fs) */
    c->a1 = 1.f / (1.f + g * (g + k));
    c->a2 = g * c->a1;
    c->a3 = g * c->a2;
}
static inline float dspf_svf(float x, float a1, float a2, float a3, float ic[2], float *lp)   /* -> the band-pass */
{
    float v3 = x - ic[1], v1 = a1 * ic[0] + a2 * v3, v2 = ic[1] + a2 * ic[0] + a3 * v3;
    ic[0] = 2.f * v1 - ic[0];
    ic[1] = 2.f * v2 - ic[1];
    *lp = v2;
    return v1;
}
