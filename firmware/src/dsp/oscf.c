/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The oscillators in floating point, for any engine: every one as loud as the others (rms OSCF_LVL, a +-1 saw's), so
 * a wave changed is a colour changed, not a level (2026-10-28: EVA's waves were -5 .. +5 dB apart). Phase p and
 * increment dt in cycles (0 .. 1); band-limited by polyBLEP. Built on dsp/dspf.c's maths (dspf_sin, dspf_sqrt,
 * dspf_urand). The fixed-point ones are dsp/dsp.c's (Felucca-Engines': osc_saw ...).
 *
 *   the basic ones   oscf_saw (the reference), oscf_pulse (+ oscf_pulse_gain), oscf_tri, oscf_sine
 *   the noises       oscf_white, oscf_pink (-3 dB/oct), oscf_brown (-6 dB/oct)
 *   mixes of them    oscf_multi (saw + a pulse beside it), oscf_dbl (saw + a pulse, one phase), oscf_sub (saw + a
 *                    square an octave below), oscf_supersaw (seven saws spread), oscf_sync (a saw hard-synced),
 *                    oscf_harm (1 .. 16 harmonics): each its parts in the balance that is its timbre, then brought
 *                    to OSCF_LVL as a whole (their levels below, each measured or worked out) */

#define OSCF_LVL 0.57735f                          /* every oscillator's level: a +-1 saw's rms, 1 / sqrt(3) */

/* ---- the basic ones */
static inline float oscf_blep(float t, float dt)   /* polyBLEP */
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
static inline float oscf_saw(float p, float dt) { return 2.f * p - 1.f - oscf_blep(p, dt); }   /* (the reference) */
static inline float oscf_tri(float p) { return 4.f * (p < 0.5f ? 0.5f - p : p - 0.5f) - 1.f; }   /* (+-1: a saw's rms) */
#define OSCF_SINE_LVL 0.8165f                      /* a sine's rms 0.707 to a saw's 0.577 */
static inline float oscf_sine(float p) { return OSCF_SINE_LVL * dspf_sin(p); }
/* a pulse of width w, centred (its mean, 2w - 1, taken off: a thin pulse was mostly offset), x g. At OSCF_LVL with g =
 * oscf_pulse_gain(w), worked out once a block (a pulse's rms is 2 sqrt(w (1 - w)): a square 1, 8 % wide 0.54); g = 1
 * for the raw +-1 pulse inside a mix */
static inline float oscf_pulse_gain(float w) { return OSCF_LVL / (2.f * dspf_sqrt(w * (1.f - w))); }
static inline float oscf_pulse(float p, float dt, float w, float g)
{
    float q = p + 1.f - w;
    if (q >= 1.f) q -= 1.f;
    return g * ((p < w ? 1.f : -1.f) + oscf_blep(p, dt) - oscf_blep(q, dt) - (2.f * w - 1.f));
}

/* ---- the noises: from dspf_urand (uniform +-1, white, rms 0.577: OSCF_LVL already) */
static inline float oscf_white(uint32_t *seed) { return dspf_urand(seed); }
#define OSCF_PINK_LVL 0.3333f                      /* the three poles' sum has rms 1.73: x 1/3 */
static inline float oscf_pink(float st[3], uint32_t *seed)   /* -3 dB/oct (P. Kellet's three poles) */
{
    float w = dspf_urand(seed);
    st[0] = 0.99765f * st[0] + 0.0990460f * w;
    st[1] = 0.96300f * st[1] + 0.2965164f * w;
    st[2] = 0.57000f * st[2] + 1.0526913f * w;
    return OSCF_PINK_LVL * (st[0] + st[1] + st[2] + 0.1848f * w);
}
static inline float oscf_brown(float *st, uint32_t *seed)   /* -6 dB/oct above ~35 Hz (leaky; its rms a saw's) */
{
    return *st = 0.995f * *st + 0.1f * dspf_urand(seed);
}

/* ---- mixes */
#define OSCF_MULTI_LVL 0.518f                      /* multi: (saw + a 35 % pulse beside it, beating) x this: as two
                                                    * uncorrelated, rms sqrt(0.577^2 + 0.954^2) = 1.115 */
#define OSCF_MULTI_W 0.35f                         /* its pulse's width */
static inline float oscf_multi(float p, float p2, float dt)   /* p2: the pulse's phase (the caller detunes it) */
{
    return OSCF_MULTI_LVL * (oscf_saw(p, dt) + oscf_pulse(p2, dt, OSCF_MULTI_W, 1.f));
}

#define OSCF_DBL_LVL 0.995f                        /* dbl: (saw + a pulse of width w, the same phase) x this (measured,
                                                    * its width swept 0.2 .. 0.8) */
static inline float oscf_dbl(float p, float dt, float w)
{
    return OSCF_DBL_LVL * (oscf_saw(p, dt) + oscf_pulse(p, dt, w, 1.f));
}

#define OSCF_SUB_SAW 0.6f                          /* sub: the saw ... */
#define OSCF_SUB_SQ 0.5f                           /* ... and a square an octave below (its timbre: the square leads) */
#define OSCF_SUB_LVL 0.94f                         /* ... together x this (measured) */
static inline float oscf_sub(float p, float *p3, float dt)   /* p3: the octave below's phase, moved here */
{
    float o = OSCF_SUB_LVL * (OSCF_SUB_SAW * oscf_saw(p, dt) + OSCF_SUB_SQ * oscf_pulse(*p3, 0.5f * dt, 0.5f, 1.f));
    *p3 += 0.5f * dt;
    if (*p3 >= 1.f) *p3 -= 1.f;
    return o;
}

#define OSCF_SUPER_MID 0.32f                       /* supersaw: the middle saw's level ... */
#define OSCF_SUPER_SIDE 0.22f                      /* ... each of the six around it ... */
#define OSCF_SUPER_LVL 1.55f                       /* ... the seven x this (measured at a middling spread: the spread
                                                    * moves it ~+-1 dB) */
static const float OSCF_SPREAD[6] = {-1.f, -0.62f, -0.3f, 0.29f, 0.6f, 0.98f};   /* the six, x the spread: uneven,
                                                                                  * so they never line up */
static inline void oscf_supersaw_init(float ss[6])   /* (once, at a block's start: the six's phases spread from the
                                                       * start, not all at 0) */
{
    uint32_t j;
    if (ss[0] == 0.f)
        for (j = 0; j < 6u; j++) ss[j] = 0.13f + 0.141f * (float)j;
}
static inline float oscf_supersaw(float p, float ss[6], float dt, float sp)   /* sp: the spread (a ratio: 0.0115 is
                                                                               * +-20 cents); ss the six's phases */
{
    float o = OSCF_SUPER_MID * oscf_saw(p, dt), d;
    uint32_t j;
    for (j = 0; j < 6u; j++) {
        d = dt * (1.f + sp * OSCF_SPREAD[j]);
        o += OSCF_SUPER_SIDE * oscf_saw(ss[j], d);
        ss[j] += d;
        if (ss[j] >= 1.f) ss[j] -= 1.f;
    }
    return OSCF_SUPER_LVL * o;
}

/* sync: a saw at r x the note, restarted at each of the note's cycles. polyBLEP on both steps (its own wrap, the
 * restart), the sample before a step corrected too: one sample late. p, p2: the note's and the saw's phases, prev:
 * the sample held back; all moved here. (A saw's level already) */
static float oscf_sync(float *p, float *p2, float *prev, float dt, float r)
{
    float d2 = dt * r, cur, pre = 0.f, h, d;
    *p += dt;
    *p2 += d2;
    if (*p >= 1.f) {                               /* the restart, d of a sample ago */
        float at;
        *p -= 1.f;
        d = *p / dt;
        at = *p2 - d * d2;
        if (at >= 1.f) at -= 1.f;                  /* (its own wrap first: let it go) */
        *p2 = d * d2;
        cur = 2.f * *p2 - 1.f;
        h = -2.f * at;                             /* (from 2 at - 1 down to -1) */
        pre += 0.5f * h * d * d;
        cur -= 0.5f * h * (1.f - d) * (1.f - d);
    } else {
        if (*p2 >= 1.f) {
            *p2 -= 1.f;
            d = *p2 / d2;
            if (d > 1.f) d = 1.f;
            cur = 2.f * *p2 - 1.f;
            pre -= d * d;
            cur += (1.f - d) * (1.f - d);
        } else
            cur = 2.f * *p2 - 1.f;
    }
    h = *prev + pre;
    *prev = cur;
    return h;
}

/* harm: sin k x, k = 1 .. n (the last one faded in, n fractional), 1 / k each, none above OSCF_HARM_TOP x fs (Chebyshev:
 * one sine a sample) */
#define OSCF_HARM_TOP 0.45f                        /* no harmonic above 0.45 fs: no aliasing */
#define OSCF_HARM_LVL 0.648f                       /* the sum x this: 0.6 (near a saw's peak) x 1.08 (measured) */
static float oscf_harm(float p, float dt, float n)
{
    float c = dspf_sin(p + 0.25f >= 1.f ? p - 0.75f : p + 0.25f), s1 = dspf_sin(p), s0 = 0.f, sk = s1, y = 0.f;
    int32_t k, top = (int32_t)n + 1;
    if ((float)top * dt > OSCF_HARM_TOP) top = (int32_t)(OSCF_HARM_TOP / dt);
    if (top > 16) top = 16;
    for (k = 1; k <= top; k++) {
        float a = 1.f / (float)k, t;
        if (k == top && (float)k > n) a *= n - (float)(k - 1);   /* (the last one fading in) */
        y += a * sk;
        t = 2.f * c * sk - s0;
        s0 = sk;
        sk = t;
    }
    return OSCF_HARM_LVL * y;
}
