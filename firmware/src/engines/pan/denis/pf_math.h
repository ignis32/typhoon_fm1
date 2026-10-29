/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* typhoon (CHANGES.md 3): the libm calls of Denis's panflute.c on fm1-base's fastmath.h. The FM-1's firmware links no
 * libm (its FPU is float-only; fastmath.h is the DSP's whole maths library, checked against libm: 1e-7 .. 1e-5), and
 * the simulator takes the same functions, so it computes the device's samples. fastmath has no acos: here, the
 * Abramowitz & Stegun 4.4.46 polynomial (|error| <= 2e-8 rad). */
#ifndef PF_MATH_H
#define PF_MATH_H
#include <stdint.h>
#include "fastmath.h"

/* acos(x), x in [-1, 1]: sqrt(1 - |x|) times a degree-7 polynomial in |x|; acos(-x) = pi - acos(x) */
static inline float pf_acosf(float x)
{
    float a = fm_fabsf(x), p;
    p = -0.0012624911f;
    p = p * a + 0.0066700901f;
    p = p * a - 0.0170881256f;
    p = p * a + 0.0308918810f;
    p = p * a - 0.0501743046f;
    p = p * a + 0.0889789874f;
    p = p * a - 0.2145988016f;
    p = p * a + 1.5707963050f;
    p *= fm_sqrtf(1.0f - a);
    return x < 0.0f ? FM_PI - p : p;
}

static inline float pf_roundf(float x) { return x < 0.0f ? -fm_floorf(-x + 0.5f) : fm_floorf(x + 0.5f); }

static inline int pf_isfinite(float x)            /* the exponent not all ones: not inf, not NaN */
{
    fm_bits_t b;
    b.f = x;
    return (b.u & 0x7F800000u) != 0x7F800000u;
}

#define sqrtf fm_sqrtf
#define tanhf fm_tanhf
#define expf fm_expf
#define powf fm_powf
#define log2f fm_log2f
#define sinf fm_sinf
#define cosf fm_cosf
#define tanf fm_tanf
#define fabsf fm_fabsf
#define fmaxf fm_maxf
#define fminf fm_minf
#define floorf fm_floorf
#define acosf pf_acosf
#define roundf pf_roundf
#define isfinite pf_isfinite
#endif
