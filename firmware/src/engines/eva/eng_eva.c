/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* EVA: a breath synth in the line of the Steiner / Crumar EVI's brass synths. One breath, shaped separately for each
 * destination: the loudness, the cutoff and the vibrato each have their own threshold, curve and attack / release,
 * so blowing harder is louder, brighter and livelier at different rates. No ADSR: breath is the envelope.
 * typhoon's layer does the rest as for every engine: notes and legato (note_on only for an articulated note), bend and
 * tilt, the breath curve, effects.
 *
 *   oscillator (SAW, PULSE, SQUARE, TRI band-limited; MULTI saw + pulse spread; DBL saw + pulse, its width swept
 *   faster up the keyboard; and the ones breath plays: SYNC, BR PWM, HARMONIC; SUB, SUPERSAW; the noises WHITE, PINK,
 *   BROWN, TUNED) + chiff -> filter (LP2, LP4, BP, HP: TPT SVF) -> waveshaper -> formant -> gain, tremolo
 *
 * Page 1:  BREATH  how much of the breath reaches the slots (BR_LVL: 80 = 0.8; what breath moves, + vmod.shape)
 *          WAVE    SAW, PULSE, SQUARE, TRI, MULTI, DBL;
 *                  SYNC     a saw hard-synced to the note, its own pitch 1 .. 4 x up with the breath: the lead's tear
 *                  BR PWM   a pulse, thin (8 %) blown softly .. square blown hard
 *                  HARMONIC additive: the harmonics come in one by one with the breath, 1 (a sine) .. 16 (a saw's)
 *                  SUB      a saw and a square an octave below
 *                  SUPERSAW seven saws spread +-20 cents, wider blown harder
 *                  WHITE, PINK (-3 dB/oct), BROWN (-6 dB/oct)  noise, each as loud as a saw: a layer of breath noise, or
 *                           with the layer's ENVELOPE (ATTACK 0, DECAY short, SUSTAIN 0) a puff on each tongued note
 *                  TUNED    noise through a resonator on the note: wide blown softly (a breath), narrow blown hard
 *                           (almost a tone); the layer's TRANS +12 / +19 rings it on a harmonic (a flute's chiff)
 *          S.E.D.  loudness against brightness: high, the level comes up fast and the filter does the dynamics (a
 *                  low-pass gate); low, a plain VCA and a more open tone
 *          CUTOFF  the filter's floor, -3 .. +3 octaves (64: none)
 * Page 2:  RES     resonance
 *          SWEEP   a kick of the cutoff on each articulated note, decaying in 80 ms: -4 .. +4 octaves (64: none)
 *          VIB     vibrato depth (0 .. 60 cents): it comes in only above 55 % breath, over 400 ms, faster blown harder
 *          SHAPE   the waveshaper's drive, more of it with breath (0: off)
 * Page 3:  FILTER  LP2, LP4, BP, HP
 *          VOWEL   the formant's vowel, A E I O U (in between: between them)
 *          FORMANT how much of the sound goes through the vowel's two formants (0: none)
 *          TREM    tremolo depth (0 .. 50 %), with the vibrato's onset and LFO, a quarter cycle on
 * Page 4:  RATE    the vibrato / tremolo LFO, 3 .. 8 Hz (64: 5.5)
 *          KEYTRK  the cutoff's key tracking, 0 .. 1 octave an octave from C4 (38: 0.3)
 *          CHIFF   a burst of bright noise on each articulated note, 30 ms, as loud as the breath at once (0: none)
 *          RESPONSE how fast the level and the filter follow the breath: x 1/4 .. x 4 (64: attack 8 / release 25 ms,
 *                  the filter 12 / 40 ms)
 * Fixed: AMP slot threshold 4 % (no smoothing of its own: the breath's is global, G_BSMOOTH); FILTER slot curve +0.3 (y^0.66), range 7 octaves; humanise 0.15.
 * Float (the AC79's single-precision FPU; no double, no libm: exp2 / log2 / tan / sin by polynomials, per block). */

typedef struct {
    float ph, ph2, pwm, vib;                       /* oscillator phases, the DBL PWM LFO, the vibrato LFO (cycles) */
    float ic1, ic2;                                /* the SVF */
    float y, za, zf, zv;                           /* the slots: the breath in, AMP, FILTER, VIB attack / release */
    float sweep, gain, a1, a2, a3;                 /* the sweep envelope; the last block's gain and SVF coefficients */
    float hp, hf, hg;                              /* per-note humanise: cents, octaves, gain */
    float ic3, ic4, fm[5];                         /* LP4's second stage; the formant's two band-passes; the chiff's
                                                    * last noise */
    float chiff, trem;                             /* the chiff's level (1 at an articulated note); the tremolo LFO */
    float ss[6], ph3, prev, nh, zw;                /* SUPERSAW's other six phases; SUB's; SYNC's sample held back; the
                                                    * harmonics HARMONIC plays (glided); the waves' breath (0 .. 1) */
    float nz[4], tz[2];                            /* the noises: PINK's three poles, BROWN's one; TUNED's resonator */
    uint32_t seed, noise;
} eva_t;
static eva_t eva_st[NLAYER];

/* ---- the small maths (no libm): exp2, log2 to ~1e-4, tan (Pade 3/2), the waveshaper's soft clip */
typedef union { float f; uint32_t u; } eva_fu;
static inline float eva_exp2(float x)
{
    eva_fu v;
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
static inline float eva_log2(float x)              /* x > 0 */
{
    eva_fu v;
    float m;
    int32_t e;
    v.f = x;
    e = (int32_t)((v.u >> 23) & 255u) - 127;
    v.u = (v.u & 0x007FFFFFu) | 0x3F800000u;
    m = v.f;
    return (float)e - 1.7417939f + (2.8212026f + (-1.4699568f + (0.44717955f - 0.056570851f * m) * m) * m) * m;
}
static inline float eva_pow(float y, float e) { return y <= 0.f ? 0.f : eva_exp2(e * eva_log2(y)); }
static inline float eva_sqrt(float x)              /* (x <= 0: 0) one guess from the exponent, two Newton steps */
{
    eva_fu v;
    float y;
    if (x <= 0.f) return 0.f;
    v.f = x;
    v.u = (v.u >> 1) + 0x1FC00000u;
    y = v.f;
    y = 0.5f * (y + x / y);
    return 0.5f * (y + x / y);
}
static inline float eva_clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float eva_sat(float x)               /* tanh-like, x in -3 .. 3 */
{
    x = eva_clamp(x, -3.f, 3.f);
    return x * (27.f + x * x) / (27.f + 9.f * x * x);
}
static inline float eva_sin(float p)               /* sin(2 pi p), p in 0 .. 1 (parabolic, then corrected) */
{
    float q = p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p, y = 1.f - q * q;   /* (0 at p 0 / 0.5, peaks at 1/4, 3/4) */
    y = p < 0.5f ? y : -y;
    return y * (0.775f + 0.225f * (y < 0.f ? -y : y));
}
static inline float eva_blep(float t, float dt)    /* polyBLEP */
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
static inline float eva_saw(float p, float dt) { return 2.f * p - 1.f - eva_blep(p, dt); }
static inline float eva_pulse(float p, float dt, float w)
{
    float q = p + 1.f - w;
    if (q >= 1.f) q -= 1.f;
    return (p < w ? 1.f : -1.f) + eva_blep(p, dt) - eva_blep(q, dt);
}

static uint32_t eva_rand(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }
static float eva_urand(uint32_t *s) { return (float)(int32_t)eva_rand(s) * (1.f / 2147483648.f); }   /* -1 .. 1 */

/* the SVF's coefficients for fc (Hz) and damping k */
static void eva_svf(float fc, float k, float *a1, float *a2, float *a3)
{
    float x = 3.14159265f * fc / (float)FS, x2 = x * x, g = x * (15.f - x2) / (15.f - 6.f * x2);   /* tan(pi fc / fs) */
    *a1 = 1.f / (1.f + g * (g + k));
    *a2 = g * *a1;
    *a3 = g * *a2;
}

enum { EW_SAW, EW_PULSE, EW_SQUARE, EW_TRI, EW_MULTI, EW_DBL, EW_SYNC, EW_BPWM, EW_HARM, EW_SUB, EW_SUPER,
       EW_WHITE, EW_PINK, EW_BROWN, EW_TUNED, EVA_NWAVE };
static const char *const EVA_N_WAVE[EVA_NWAVE] = {"SAW", "PULSE", "SQUARE", "TRI", "MULTI", "DBL", "SYNC", "BR PWM",
                                                  "HARMONIC", "SUB", "SUPERSAW", "WHITE", "PINK", "BROWN", "TUNED"};
static const float EVA_SPREAD[6] = {-1.f, -0.62f, -0.3f, 0.29f, 0.6f, 0.98f};   /* SUPERSAW's six, x the spread */

/* SYNC: a saw at r x the note, restarted at each of the note's cycles. polyBLEP on both steps (its own wrap, the
 * restart), the sample before a step corrected too: one sample late */
static float eva_sync(eva_t *s, float dt, float r)
{
    float d2 = dt * r, cur, pre = 0.f, h, d;
    s->ph += dt;
    s->ph2 += d2;
    if (s->ph >= 1.f) {                            /* the restart, d of a sample ago */
        float at;
        s->ph -= 1.f;
        d = s->ph / dt;
        at = s->ph2 - d * d2;
        if (at >= 1.f) at -= 1.f;                  /* (its own wrap first: let it go) */
        s->ph2 = d * d2;
        cur = 2.f * s->ph2 - 1.f;
        h = -2.f * at;                             /* (from 2 at - 1 down to -1) */
        pre += 0.5f * h * d * d;
        cur -= 0.5f * h * (1.f - d) * (1.f - d);
    } else {
        if (s->ph2 >= 1.f) {
            s->ph2 -= 1.f;
            d = s->ph2 / d2;
            if (d > 1.f) d = 1.f;
            cur = 2.f * s->ph2 - 1.f;
            pre -= d * d;
            cur += (1.f - d) * (1.f - d);
        } else
            cur = 2.f * s->ph2 - 1.f;
    }
    h = s->prev + pre;
    s->prev = cur;
    return h;
}

/* HARMONIC: sin k x, k = 1 .. n (the last one faded in), 1 / k each, below 0.45 fs (Chebyshev: one sine a sample) */
static float eva_harm(float ph, float dt, float n)
{
    float c = eva_sin(ph + 0.25f >= 1.f ? ph - 0.75f : ph + 0.25f), s1 = eva_sin(ph), s0 = 0.f, sk = s1, y = 0.f;
    int32_t k, top = (int32_t)n + 1;
    if ((float)top * dt > 0.45f) top = (int32_t)(0.45f / dt);
    if (top > 16) top = 16;
    for (k = 1; k <= top; k++) {
        float a = 1.f / (float)k, t;
        if (k == top && (float)k > n) a *= n - (float)(k - 1);   /* (the last one fading in) */
        y += a * sk;
        t = 2.f * c * sk - s0;
        s0 = sk;
        sk = t;
    }
    return 0.6f * y;
}

/* the slots' attack / release coefficients at the control rate (44100 / 32 = 1378 Hz: 1 - e^(-1000 / (t f))) */
#define EVA_A_ATK 0.0867f                          /* AMP 8 ms */
#define EVA_A_REL 0.0286f                          /* AMP 25 ms */
#define EVA_F_ATK 0.0587f                          /* FILTER 12 ms */
#define EVA_F_REL 0.0180f                          /* FILTER 40 ms */
#define EVA_V_ATK 0.00181f                         /* VIB 400 ms */
#define EVA_V_REL 0.00483f                         /* VIB 150 ms */
#define EVA_SWEEP 0.99098f                         /* the sweep's decay a block, 80 ms */
#define EVA_CHIFF 0.9761f                          /* the chiff's, 30 ms */
/* the formants (Hz) of A E I O U (a man's voice) */
static const float EVA_F1[5] = {730.f, 530.f, 270.f, 570.f, 300.f}, EVA_F2[5] = {1090.f, 1840.f, 2290.f, 840.f, 870.f};
enum { EF_LP2, EF_LP4, EF_BP, EF_HP, EVA_NFILT };
static const char *const EVA_N_FILT[EVA_NFILT] = {"LP2", "LP4", "BP", "HP"};
#define EVA_HUMAN 0.15f

static void eva_note_on(track_t *t, voice_t *v)   /* an articulated note: the sweep kicks, the humanise draws */
{
    eva_t *s = &eva_st[(uint32_t)(t - trk) % NLAYER];
    if (!s->seed)
        s->seed = 0x2545F491u + (uint32_t)v->age;
    s->sweep = 1.f;
    s->chiff = 1.f;
    s->hp = 3.f * EVA_HUMAN * eva_urand(&s->seed);          /* cents */
    s->hf = 0.15f * EVA_HUMAN * eva_urand(&s->seed);        /* octaves */
    s->hg = 1.f + 0.06f * EVA_HUMAN * eva_urand(&s->seed);  /* (+-0.5 dB x HUMAN) */
}

/* every block, sounding or not: a layer whose voice has faded out is at rest (its followers would stay where the
 * last note left them: the next note would start from there, with no breath) */
static void eva_block(track_t *t)
{
    eva_t *s = &eva_st[(uint32_t)(t - trk) % NLAYER];
    if (!t->v[0].active) {
        s->y = s->za = s->zf = s->zv = s->zw = s->gain = s->sweep = s->chiff = 0.f;
        s->tz[0] = s->tz[1] = 0.f;                 /* (TUNED rings from silence on the next note's pitch) */
    }
}

static void eva_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    eva_t *s = &eva_st[(uint32_t)(t - trk) % NLAYER];
    int32_t wave = clamp(p[P_E1], 0, EVA_NWAVE - 1);
    float br = (float)(m->shape - (64 << 8)) * (1.f / (63.f * 256.f));   /* the breath, 0 .. 1 (BR>SHP 63: all of it) */
    float sed = (float)p[P_E2] / 127.f, res = (float)p[P_E4] / 127.f, x, af, amp, oct, fc, g0, g1, b1, b2, b3;
    float dt, vibc, w = 0.35f, shp = (float)p[P_E7] / 127.f, wsd, note = (float)m->pitch16 / 16.f;
    int32_t filt = clamp(p[P_E8], 0, EVA_NFILT - 1);
    float rsp = eva_exp2(((float)p[P_E15] - 64.f) / 32.f), fmt = (float)p[P_E10] / 127.f, vw, tr, tr0, tr1;
    float ch0, ch1, ck = (float)p[P_E14] * (0.35f / 127.f) * eva_sqrt(br), c1, c2, c3, d1, d2, d3, fk = 0.12f;
    float t1 = 0.f, t2 = 0.f, t3 = 0.f, tk = 0.f, tg = 0.f;   /* TUNED's resonator */
    uint32_t i, j;
    (void)v;
    br = eva_clamp(br, 0.f, 1.f);
    /* the slots: BR_LVL, AMP's threshold; then AMP and FILTER each their curve and attack / release */
    x = eva_clamp(br * (float)p[P_E0] / 100.f, 0.f, 1.f);
    x = eva_clamp((x - 0.04f) / 0.96f, 0.f, 1.f);
    s->y = x;
    s->za += eva_clamp((s->y > s->za ? EVA_A_ATK : EVA_A_REL) * rsp, 0.f, 1.f) * (s->y - s->za);
    af = eva_pow(s->y, 0.66f);                     /* (CURV +0.3: 4^-0.3) */
    s->zf += eva_clamp((af > s->zf ? EVA_F_ATK : EVA_F_REL) * rsp, 0.f, 1.f) * (af - s->zf);
    /* S.E.D.: the gain's law, the filter's depth and floor */
    amp = eva_pow(s->za, 1.f - 0.75f * sed) * s->hg;
    oct = 2.3f + 3.f * (1.f - sed) + ((float)p[P_E3] - 64.f) * (3.f / 64.f)
        + s->zf * (0.25f + 0.75f * sed) * 7.f
        + s->sweep * ((float)p[P_E5] - 64.f) * (4.f / 64.f)
        + (float)p[P_E13] / 127.f * (note - 60.f) / 12.f + s->hf;
    s->sweep *= EVA_SWEEP;
    fc = eva_clamp(20.f * eva_exp2(oct), 20.f, 0.42f * (float)FS);
    eva_svf(fc, (filt == EF_LP4 ? 1.6f : 2.f) - (filt == EF_LP4 ? 1.4f : 1.9f) * res, &b1, &b2, &b3);
    /* the formant: the vowel's two, in between interpolated */
    vw = (float)p[P_E9] * (4.f / 127.f);
    j = (uint32_t)vw;
    if (j > 3u) j = 3u;
    vw -= (float)j;
    eva_svf(EVA_F1[j] + (EVA_F1[j + 1u] - EVA_F1[j]) * vw, fk, &c1, &c2, &c3);
    eva_svf(EVA_F2[j] + (EVA_F2[j + 1u] - EVA_F2[j]) * vw, fk, &d1, &d2, &d3);
    /* vibrato: above 55 % breath, in over 400 ms, faster blown harder */
    x = eva_clamp((br - 0.55f) / 0.45f, 0.f, 1.f);
    s->zv += (x > s->zv ? EVA_V_ATK : EVA_V_REL) * (x - s->zv);
    s->vib += (3.f + 5.f * (float)p[P_E12] / 127.f + 1.6f * s->zv) * (float)CTL / (float)FS;
    if (s->vib >= 1.f) s->vib -= 1.f;
    tr = s->vib + 0.25f;                           /* the tremolo: a quarter cycle on (louder as the pitch rises) */
    if (tr >= 1.f) tr -= 1.f;
    tr0 = s->trem;
    tr1 = s->trem = 1.f - (float)p[P_E11] * (0.5f / 127.f) * s->zv * (0.5f + 0.5f * eva_sin(tr));
    vibc = (float)p[P_E6] * (60.f / 127.f) * s->zv * eva_sin(s->vib) + s->hp;
    dt = (float)m->inc * (1.f / 4294967296.f) * (1.f + vibc * 0.00057779f);   /* (2^(c/1200) ~ 1 + c ln2/1200) */
    if (wave == EW_DBL) {                          /* DBL: the width swept by an LFO 1.5 Hz at C4, faster up */
        s->pwm += 1.5f * eva_exp2(0.5f * (note - 60.f) / 12.f) * (float)CTL / (float)FS;
        if (s->pwm >= 1.f) s->pwm -= 1.f;
        w = 0.5f + 0.3f * eva_sin(s->pwm);
    }
    wsd = shp * (0.5f + 0.5f * s->za) * 4.f;       /* the drive, more with breath */
    s->zw += (br > s->zw ? EVA_F_ATK : EVA_F_REL) * (br - s->zw);   /* the waves' breath: all of it, BREATH aside */
    if (wave == EW_BPWM)
        w = 0.08f + 0.42f * s->zw;
    if (wave == EW_HARM) {                         /* the harmonics with the breath, glided (no steps) */
        float nt = 1.f + 15.f * s->zw;
        s->nh += 0.2f * (nt - s->nh);
    }
    if (wave == EW_TUNED) {                        /* TUNED: the resonator's damping, 0.5 blown softly .. 0.02 hard,
                                                    * and the gain that keeps it as loud as a saw (its noise power
                                                    * goes with its width: rms ~ sqrt(k fc / fs), measured) */
        float tf = eva_clamp(dt * (float)FS, 20.f, 0.42f * (float)FS);
        tk = 0.5f * eva_exp2(-4.644f * s->zw);
        eva_svf(tf, tk, &t1, &t2, &t3);
        tg = 0.577f / eva_sqrt(tk * tf / (float)FS);
    }
    if (wave == EW_SUPER && s->ss[0] == 0.f)       /* (their phases spread from the start) */
        for (j = 0; j < 6u; j++) s->ss[j] = 0.13f + 0.141f * (float)j;
    g0 = s->gain;
    g1 = s->gain = amp;
    ch0 = s->chiff * ck;
    s->chiff *= EVA_CHIFF;
    ch1 = s->chiff * ck;
    if (!s->noise) s->noise = 0x9E3779B9u;
    if (s->a1 == 0.f) { s->a1 = b1; s->a2 = b2; s->a3 = b3; }
    for (i = 0; i < n; i++) {
        float f = (float)i / (float)n, a1 = s->a1 + (b1 - s->a1) * f, a2 = s->a2 + (b2 - s->a2) * f;
        float a3 = s->a3 + (b3 - s->a3) * f, o, v1, v2, v3, y, ch = ch0 + (ch1 - ch0) * f;
        switch (wave) {
        case EW_PULSE: o = eva_pulse(s->ph, dt, 0.35f); break;
        case EW_SQUARE: o = eva_pulse(s->ph, dt, 0.5f); break;
        case EW_TRI: o = 1.f - 4.f * (s->ph < 0.5f ? 0.5f - s->ph : s->ph - 0.5f) * 1.f; o = -o; break;
        case EW_MULTI: o = 0.6f * (eva_saw(s->ph, dt) + eva_pulse(s->ph2, dt, 0.35f)); break;
        case EW_DBL: o = 0.5f * (eva_saw(s->ph, dt) + eva_pulse(s->ph, dt, w)); break;
        case EW_SYNC: o = eva_sync(s, dt, 1.f + 3.f * s->zw); break;
        case EW_BPWM: o = eva_pulse(s->ph, dt, w); break;
        case EW_HARM: o = eva_harm(s->ph, dt, s->nh); break;
        case EW_SUB:
            o = 0.6f * eva_saw(s->ph, dt) + 0.5f * eva_pulse(s->ph3, 0.5f * dt, 0.5f);
            s->ph3 += 0.5f * dt;
            if (s->ph3 >= 1.f) s->ph3 -= 1.f;
            break;
        case EW_SUPER: {
            float sp = 0.0115f * (0.6f + 0.4f * s->zw), d3;   /* (+-20 cents, 12 blown softly) */
            o = 0.32f * eva_saw(s->ph, dt);
            for (j = 0; j < 6u; j++) {
                d3 = dt * (1.f + sp * EVA_SPREAD[j]);
                o += 0.22f * eva_saw(s->ss[j], d3);
                s->ss[j] += d3;
                if (s->ss[j] >= 1.f) s->ss[j] -= 1.f;
            }
            break;
        }
        case EW_WHITE: o = eva_urand(&s->noise); break;
        case EW_PINK: {                            /* (P. Kellet's three-pole pink, rms 1.73: x 1/3) */
            float w0 = eva_urand(&s->noise);
            s->nz[0] = 0.99765f * s->nz[0] + 0.0990460f * w0;
            s->nz[1] = 0.96300f * s->nz[1] + 0.2965164f * w0;
            s->nz[2] = 0.57000f * s->nz[2] + 1.0526913f * w0;
            o = 0.3333f * (s->nz[0] + s->nz[1] + s->nz[2] + 0.1848f * w0);
            break;
        }
        case EW_BROWN:                             /* (leaky: -6 dB/oct above ~35 Hz, rms that of a saw) */
            o = s->nz[3] = 0.995f * s->nz[3] + 0.1f * eva_urand(&s->noise);
            break;
        case EW_TUNED: {                           /* white noise, its band-pass at the note (k v1: 1 at the centre) */
            float u = eva_urand(&s->noise) - s->tz[1], q1 = t1 * s->tz[0] + t2 * u, q2 = s->tz[1] + t2 * s->tz[0] + t3 * u;
            s->tz[0] = 2.f * q1 - s->tz[0];
            s->tz[1] = 2.f * q2 - s->tz[1];
            o = tg * tk * q1;
            break;
        }
        default: o = eva_saw(s->ph, dt);
        }
        if (wave != EW_SYNC) {                     /* (SYNC moves its own) */
            s->ph += dt;
            if (s->ph >= 1.f) s->ph -= 1.f;
        }
        s->ph2 += dt * 1.0026f;                    /* (MULTI's second wave 4.5 cents up: SPREAD 30 %) */
        if (s->ph2 >= 1.f) s->ph2 -= 1.f;
        v3 = o - s->ic2;                           /* the filter (TPT state variable) */
        v1 = a1 * s->ic1 + a2 * v3;
        v2 = s->ic2 + a2 * s->ic1 + a3 * v3;
        s->ic1 = 2.f * v1 - s->ic1;
        s->ic2 = 2.f * v2 - s->ic2;
        switch (filt) {
        case EF_LP4:                               /* a second low-pass stage, the same cutoff, no resonance */
            v3 = v2 - s->ic4;
            v1 = a1 * s->ic3 + a2 * v3;
            y = s->ic4 + a2 * s->ic3 + a3 * v3;
            s->ic3 = 2.f * v1 - s->ic3;
            s->ic4 = 2.f * y - s->ic4;
            y *= 1.3f;
            break;
        case EF_BP: y = 2.f * (2.f - 1.9f * res) * v1; break;   /* (k v1: 1 at the centre) */
        case EF_HP: y = o - v2 - (2.f - 1.9f * res) * v1; break;
        default: y = v2;
        }
        if (wsd > 0.f)                             /* the waveshaper, gain-compensated */
            y = eva_sat(y * (1.f + wsd)) / (1.f + 0.3f * wsd);
        if (fmt > 0.f) {                           /* the formant: two band-passes (k 0.12: Q ~8), the second -3 dB */
            float u, b, q = 0.f;
            u = y - s->fm[1];
            b = c1 * s->fm[0] + c2 * u;
            q += fk * b;
            s->fm[1] += 2.f * (c2 * s->fm[0] + c3 * u);
            s->fm[0] = 2.f * b - s->fm[0];
            u = y - s->fm[3];
            b = d1 * s->fm[2] + d2 * u;
            q += 0.7f * fk * b;
            s->fm[3] += 2.f * (d2 * s->fm[2] + d3 * u);
            s->fm[2] = 2.f * b - s->fm[2];
            y += fmt * (2.5f * q - y);
        }
        y *= (g0 + (g1 - g0) * f) * (tr0 + (tr1 - tr0) * f);
        if (ch > 0.f) {                            /* the chiff: high-passed noise, as loud as the breath at once */
            float z = eva_urand(&s->noise);
            y += ch * (z - s->fm[4]);
            s->fm[4] = z;
        }
        out[i] += voice_amp((int32_t)(eva_clamp(y, -1.5f, 1.5f) * 24576.f), m, i);   /* (the other engines' level) */
    }
    s->a1 = b1;
    s->a2 = b2;
    s->a3 = b3;
}

#define EAP(n, b, w, sed, cut, res, swp, vib, shp, flt, vow, fmt, trm, rate, kt, chf, rsp) \
    {n, {b, w, sed, cut, res, swp, vib, shp, flt, vow, fmt, trm, rate, kt, chf, rsp}, {0, 0, 127, 25}, 0, 1, {0}, 0}
static const preset_t EVA_PRESETS[] = {
    /*   name          BREATH WAVE      S.E.D. CUT RES SWP VIB SHP  FILTER  VOW FMT TRM RATE KT CHF RSP */
    EAP("EVA BRASS",   80, EW_SAW,    76,    64, 19, 72, 64,  0, EF_LP2,  0,  0,  0, 64, 38,  0, 64),   /* the init sound */
    EAP("EVA DBL",     80, EW_DBL,    70,    60, 25, 76, 60,  0, EF_LP2,  0,  0,  0, 64, 38,  0, 64),
    EAP("EVA LEAD",    85, EW_SQUARE, 90,    58, 55, 84, 50, 40, EF_LP2,  0,  0,  0, 64, 38,  0, 64),
    EAP("EVA FLUTE",   75, EW_TRI,    30,    70,  0, 64, 80,  0, EF_LP2,  0,  0,  0, 64, 38,  0, 64),
    EAP("EVA MULTI",   80, EW_MULTI,  60,    62, 30, 70, 45, 15, EF_LP2,  0,  0,  0, 64, 38,  0, 64),
    /* EDIT 3 / 4 at work */
    EAP("EVA VOX",     80, EW_SAW,    60,    70, 10, 66, 70,  0, EF_LP2, 10, 90,  0, 58, 30,  0, 56),   /* an "ah" */
    EAP("EVA HORN",    80, EW_SAW,    85,    50, 10, 78, 40,  0, EF_LP4, 95, 35,  0, 60, 50, 20, 44),   /* dark, slow */
    EAP("EVA REED",    85, EW_PULSE,  65,    66, 40, 68, 50, 25, EF_BP,  40, 45,  0, 70, 64, 30, 76),
    EAP("EVA TREMOLO", 80, EW_SQUARE, 55,    66, 30, 64, 20,  0, EF_LP2,  0,  0, 90, 82, 38,  0, 64),
    EAP("EVA BASS",    90, EW_SAW,    90,    52, 60, 84,  0, 30, EF_LP4,  0,  0,  0, 64,  0, 10, 96),   /* tight */
    EAP("EVA PIPE",    70, EW_TRI,    35,    72,  0, 64, 70,  0, EF_LP2, 75, 40,  0, 66, 38, 90, 64),   /* breathy start */
    EAP("EVA THIN",    80, EW_PULSE,  70,    56, 45, 70, 55,  0, EF_HP,   0,  0, 30, 60, 38,  0, 64),
    /* the waves breath plays (12 ..) */
    EAP("EVA SYNC",    85, EW_SYNC,   80,    70, 20, 72, 50, 20, EF_LP2,  0,  0,  0, 64, 38,  0, 64),
    EAP("EVA REEDY",   80, EW_BPWM,   60,    66, 15, 66, 55,  0, EF_LP2,  0,  0,  0, 62, 38, 20, 64),
    EAP("EVA HARMONIC",75, EW_HARM,   40,    90,  0, 64, 60,  0, EF_LP2,  0,  0,  0, 60, 38, 30, 64),
    EAP("EVA SUB BASS",90, EW_SUB,    85,    50, 35, 80,  0, 20, EF_LP4,  0,  0,  0, 64,  0, 10, 96),
    EAP("EVA SUPERSAW",80, EW_SUPER,  70,    66, 15, 70, 30,  0, EF_LP2,  0,  0,  0, 64, 38,  0, 64),
    EAP("EVA TRANCE",  85, EW_SUPER,  85,    58, 40, 84,  0, 10, EF_LP4,  0,  0,  0, 64, 38,  0, 80),
};
#undef EAP

static const engine_t ENG_EVA = {
    .name = "EVA",
    .page_title = {"EVA", "EVA +", "EVA COLOUR", "EVA PLAY"},
    .edit = {
        {"BREATH", F_PCT, 0, 127, 80, 0, 0},
        {"WAVE", F_ENUM, 0, EVA_NWAVE - 1, EW_SAW, EVA_N_WAVE, 0},
        {"S.E.D.", F_PCT, 0, 127, 76, 0, 0},
        {"CUTOFF", F_PCT, 0, 127, 64, 0, 0},
        {"RES", F_PCT, 0, 127, 19, 0, 0},
        {"SWEEP", F_PCT, 0, 127, 72, 0, 0},
        {"VIB", F_PCT, 0, 127, 64, 0, 0},
        {"SHAPE", F_PCT, 0, 127, 0, 0, 0},
        {"FILTER", F_ENUM, 0, EVA_NFILT - 1, EF_LP2, EVA_N_FILT, 0},
        {"VOWEL", F_PCT, 0, 127, 0, 0, 0},
        {"FORMANT", F_PCT, 0, 127, 0, 0, 0},
        {"TREM", F_PCT, 0, 127, 0, 0, 0},
        {"RATE", F_PCT, 0, 127, 64, 0, 0},
        {"KEYTRK", F_PCT, 0, 127, 38, 0, 0},
        {"CHIFF", F_PCT, 0, 127, 0, 0, 0},
        {"RESPONSE", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = EVA_PRESETS,
    .npresets = NELEM(EVA_PRESETS),
    .knob = {0, 2, 3, 4},
    .block = eva_block,
    .note_on = eva_note_on,
    .render = eva_render,
};
