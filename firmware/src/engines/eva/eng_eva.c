/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* EVA: a breath synth in the line of the Steiner / Crumar EVI's brass synths. One breath, shaped separately for each
 * destination: the loudness, the cutoff and the vibrato each have their own threshold and curve, so blowing harder
 * is louder, brighter and livelier in different measure. Each follows the breath at once unless LAG / VIB SWELL say
 * otherwise, visibly (until 2026-10-27 fixed attacks / releases under RESPONSE; the presets have them as they were;
 * the breath's own smoothing is global, G_BSMOOTH). No ADSR: breath is the envelope.
 * typhoon's layer does the rest as for every engine: notes and legato (note_on only for an articulated note), bend and
 * tilt, the breath curve, effects.
 *
 *   oscillator (SAW, PULSE, SQUARE, TRI band-limited; MULTI saw + pulse spread; DBL saw + pulse, its width swept
 *   faster up the keyboard; and the ones breath plays: SYNC, BR PWM, HARMONIC; SUB, SUPERSAW; the noises NS_WHITE,
 *   NS_PINK, NS_BROWN, NS_TUNED) + chiff -> filter (LP2, LP4, BP, HP: TPT SVF) -> waveshaper -> formant -> gain, tremolo
 *
 * Page 1:  BREATH  how much of the breath reaches the slots (BR_LVL: 80 = 0.8; what breath moves, + vmod.shape)
 *          WAVE    SAW, PULSE, SQUARE, TRI, SINE (as loud as a saw), MULTI, DBL;
 *                  SYNC     a saw hard-synced to the note, its own pitch 1 .. 4 x up with the breath: the lead's tear
 *                  BR PWM   a pulse, thin (8 %) blown softly .. square blown hard
 *                  HARMONIC additive: the harmonics come in one by one with the breath, 1 (a sine) .. 16 (a saw's)
 *                  SUB      a saw and a square an octave below
 *                  SUPERSAW seven saws spread +-20 cents, wider blown harder
 *                  NS_WHITE, NS_PINK (-3 dB/oct), NS_BROWN (-6 dB/oct)  noise, each as loud as a saw: a layer of breath
 *                           noise, or with the layer's ENVELOPE (ATTACK 0, DECAY short, SUSTAIN 0) a puff on each tongued
 *                           note (NS_: the noises, waves and presets, told apart at a glance and listed together)
 *                  NS_TUNED noise through a resonator on the note: wide blown softly (a breath), narrow blown hard
 *                           (almost a tone); the layer's TRANS +12 / +19 rings it on a harmonic (a flute's chiff)
 *          S.E.D.  loudness against brightness: high, the level comes up fast and the filter does the dynamics (a
 *                  low-pass gate); low, a plain VCA and a more open tone
 *          CUTOFF  the filter's floor, -3 .. +3 octaves (64: none)
 * Page 2:  RES     resonance
 *          SWEEP   a kick of the cutoff on each articulated note, decaying in 80 ms: -4 .. +4 octaves (64: none)
 *          VIB     vibrato depth (0 .. 60 cents): only above 55 % breath, deeper and faster blown harder, coming in
 *                  as fast as VIB SWELL says
 *          SHAPE   the waveshaper's drive, more of it with breath (0: off)
 * Page 3:  FILTER  LP2, LP4, BP, HP
 *          VOWEL   the formant's vowel, A E I O U (in between: between them)
 *          FORMANT how much of the sound goes through the vowel's two formants (0: none)
 *          TREM    tremolo depth (0 .. 50 %), with the vibrato's onset and LFO, a quarter cycle on
 * Page 4:  RATE    the vibrato / tremolo LFO, 3 .. 8 Hz (64: 5.5)
 *          KEYTRK  the cutoff's key tracking, 0 .. 1 octave an octave from C4 (38: 0.3)
 *          CHIFF   a burst of bright noise on each articulated note, 30 ms, as loud as the breath at once (0: none)
 *          LAG     how slowly the level, the filter and the waves breath plays follow the breath: 0 at once (the
 *                  default); 64 attack 8 / release 25 ms (the filter and the waves 12 / 40: 90 % in ~16 ms), each 32
 *                  twice as slow. RESPONSE until 2026-10-27, the other way round: LAG = 128 - RESPONSE
 * Page 5:  VIB SWELL how long the vibrato and tremolo take to come in: 0 at once; 64 400 ms (out in 150), as it was
 *                  fixed until 2026-10-27; 127 ~1.6 s
 * (the humanise, 15 % fixed and hidden until 2026-10-28: gone; a fingering's own offsets are the layer's QUIRKS)
 * Everything else that shapes the sound: the block "EVA's fixed numbers" below, each with why.
 * Float (the AC79's single-precision FPU): its maths, oscillators, noises and filter are dsp/dspf.c's, shared. */

typedef struct {
    float ph, ph2, pwm, vib;                       /* oscillator phases, the DBL PWM LFO, the vibrato LFO (cycles) */
    float flt[2];                                  /* the filter's state (dsp/dspf.c dspf_svf) */
    float y, za, zf, zv;                           /* the slots: the breath in, AMP, FILTER, VIB (each at once) */
    float sweep, gain, a1, a2, a3;                 /* the sweep envelope; the last block's gain and SVF coefficients */
    float lp4[2], fm1[2], fm2[2], chz;             /* LP4's second stage; the formant's two band-passes; the chiff's
                                                    * last noise */
    float chiff, trem;                             /* the chiff's level (1 at an articulated note); the tremolo LFO */
    float ss[6], ph3, prev, nh, zw;                /* SUPERSAW's other six phases; SUB's; SYNC's sample held back; the
                                                    * harmonics HARMONIC plays (glided); the waves' breath (0 .. 1) */
    float nz[4], tz[2];                            /* the noises: PINK's three poles, BROWN's one; TUNED's resonator */
    uint32_t noise;
} eva_t;
static eva_t eva_st[NLAYER];

/* (the maths, the oscillators, the noises and the filter: dsp/dspf.c, shared; EVA's own until 2026-10-28) */

/* (SINE came in after TRI, 2026-10-28: a preset saved before it has the waves after TRI one lower, and is read so:
 * core/presets.c ps_upgrade) */
enum { EW_SAW, EW_PULSE, EW_SQUARE, EW_TRI, EW_SINE, EW_MULTI, EW_DBL, EW_SYNC, EW_BPWM, EW_HARM, EW_SUB, EW_SUPER,
       EW_WHITE, EW_PINK, EW_BROWN, EW_TUNED, EVA_NWAVE };
static const char *const EVA_N_WAVE[EVA_NWAVE] = {"SAW", "PULSE", "SQUARE", "TRI", "SINE", "MULTI", "DBL", "SYNC",
                                                  "BR PWM", "HARMONIC", "SUB", "SUPERSAW", "NS_WHITE", "NS_PINK",
                                                  "NS_BROWN", "NS_TUNED"};   /* (NS_: the noises, told apart and
                                                                              * together) */

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

/* ==== EVA's fixed numbers: everything that shapes its sound and is not a knob, and why (the rule: a knob, or here) ====
 * (the control rate: a block of CTL = 32 samples, 1378 a second at 44.1 kHz; a step a block of 1 - e^(-1000/(t f))
 * follows over t ms) */
/* -- breath into the slots */
#define EVA_BR_LVL 100.f                           /* BREATH 100: all of the breath reaches the slots (80: 0.8) */
#define EVA_AMP_THRESH 0.04f                       /* the bottom 4 % of breath does nothing: a sensor's floor is silent */
#define EVA_FILT_CURVE 0.66f                       /* the cutoff's slot y^0.66: brightness rises sooner than breath at
                                                    * the bottom (brass opens early; a curve of +0.3: 4^-0.3) */
/* -- LAG's steps at LAG 64 (as RESPONSE 64 had them, fixed until 2026-10-27; LAG scales them) */
#define EVA_A_ATK 0.0867f                          /* level 8 ms */
#define EVA_A_REL 0.0286f                          /* level 25 ms */
#define EVA_F_ATK 0.0587f                          /* filter 12 ms (and the waves breath plays) */
#define EVA_F_REL 0.0180f                          /* filter 40 ms */
/* -- S.E.D. (loudness against brightness) and the filter */
#define EVA_SED_AMP 0.75f                          /* the level's law y^(1 - 0.75 S.E.D.): blown softly already loud at
                                                    * high S.E.D. (the filter does the dynamics: a low-pass gate) */
#define EVA_FILT_FLOOR 2.3f                        /* the cutoff's floor, octaves above 20 Hz (~100 Hz) ... */
#define EVA_SED_FLOOR 3.f                          /* ... raised up to 3 octaves as S.E.D. falls (a more open tone) */
#define EVA_SED_DEPTH0 0.25f                       /* how much of the range breath opens: 0.25 at S.E.D. 0 .. */
#define EVA_SED_DEPTH1 0.75f                       /* ... + 0.75 x S.E.D. (all of it at 127) */
#define EVA_FILT_RANGE 7.f                         /* octaves breath can open the filter over */
#define EVA_CUT_RANGE 3.f                          /* CUTOFF moves the floor -3 .. +3 octaves (64: none) */
#define EVA_SWEEP_RANGE 4.f                        /* SWEEP's kick: -4 .. +4 octaves (64: none) */
#define EVA_SWEEP 0.99098f                         /* the sweep's decay a block: 80 ms, a tongued attack's bite */
#define EVA_KT_PIVOT 60.f                          /* KEYTRK pivots on C4: notes there keep CUTOFF's cutoff */
#define EVA_FC_MIN 20.f                            /* the cutoff's bottom, Hz */
#define EVA_FC_MAX 0.42f                           /* its top, x fs: below where the SVF's tan() warp runs away */
#define EVA_RES_K0 2.f                             /* RES's damping 2 (none) - 1.9 x RES: ~0.1 at 127, ringing short */
#define EVA_RES_K1 1.9f                            /* of self-oscillation */
#define EVA_LP4_K0 1.6f                            /* LP4: less, 1.6 - 1.4 x RES (two stages ring more at the same RES) */
#define EVA_LP4_K1 1.4f
#define EVA_LP4_GAIN 1.3f                          /* LP4 makes up the second stage's loss */
/* -- vibrato and tremolo (VIB, TREM, RATE, VIB SWELL are knobs) */
#define EVA_VIB_ONSET 0.55f                        /* they come in above 55 % breath: a held soft note stays straight */
#define EVA_VIB_HZ0 3.f                            /* RATE: 3 Hz .. */
#define EVA_VIB_HZ_SPAN 5.f                        /* ... + 5 (8 Hz at 127; 5.5 at 64) */
#define EVA_VIB_HZ_BR 1.6f                         /* + up to 1.6 Hz faster at full breath: a player's vibrato quickens */
#define EVA_VIB_CENTS 60.f                         /* VIB 127: +-60 cents */
#define EVA_TREM_DEPTH 0.5f                        /* TREM 127: the level -50 % at the dip */
#define EVA_TREM_PHASE 0.25f                       /* the tremolo a quarter cycle on from the vibrato: louder as the
                                                    * pitch rises, as a real vibrato's loudness follows it */
#define EVA_SWELL_MS 400.f                         /* VIB SWELL 64: in over 400 ms (x (SWELL / 64)^2) ... */
#define EVA_SWELL_OUT 0.375f                       /* ... out in 3/8 of it (150 ms): fixed so until 2026-10-27 */
/* -- the waves */
#define EVA_SINE_LVL 0.8165f                       /* SINE x 0.8165: a sine's rms 0.707 to a saw's 0.577, as loud */
#define EVA_PULSE_W 0.35f                          /* PULSE's width (and MULTI's pulse): hollower than a square */
#define EVA_MULTI_DT 1.0026f                       /* MULTI's second wave 4.5 cents up: a slow beating */
#define EVA_DBL_HZ 1.5f                            /* DBL's width LFO: 1.5 Hz at C4, faster up the keyboard */
#define EVA_DBL_DEPTH 0.3f                         /* its sweep, 0.2 .. 0.8 */
#define EVA_SYNC_SPAN 3.f                          /* SYNC's saw: 1 .. 4 x the note with breath */
#define EVA_BPWM_MIN 0.08f                         /* BR PWM: 8 % blown softly (thin, nasal) .. */
#define EVA_BPWM_SPAN 0.42f                        /* ... 50 % hard (square) */
#define EVA_HARM_MAX 15.f                          /* HARMONIC: 1 + 15 x breath harmonics */
#define EVA_HARM_GLIDE 0.2f                        /* their count glides 0.2 a block (~3.6 ms): no steps as breath
                                                    * moves in its 128 steps (a de-zipper, not a lag you play) */
#define EVA_HARM_TOP 0.45f                         /* none above 0.45 fs: no aliasing */
#define EVA_SUB_SAW 0.6f                           /* SUB: the saw ... */
#define EVA_SUB_SQ 0.5f                            /* ... and the square an octave below it */
#define EVA_SUPER_CENTS 0.0115f                    /* SUPERSAW's spread, +-20 cents (as a ratio) ... */
#define EVA_SUPER_SOFT 0.6f                        /* ... 60 % of it blown softly, all of it hard */
#define EVA_SUPER_MID 0.32f                        /* the middle saw's level ... */
#define EVA_SUPER_SIDE 0.22f                       /* ... each of the six around it */
static const float EVA_SPREAD[6] = {-1.f, -0.62f, -0.3f, 0.29f, 0.6f, 0.98f};   /* SUPERSAW's six, x the spread:
                                                    * uneven, so they never line up */
#define EVA_PINK_GAIN 0.3333f                      /* NS_PINK x 1/3: dspf_pink's rms is 1.73, a saw's 0.577 */
#define EVA_TUNED_K_SOFT 0.5f                      /* NS_TUNED's damping blown softly (wide: a breath) ... */
#define EVA_TUNED_K_LOG -4.644f                    /* ... x 2^(-4.644 breath): 0.02 hard (narrow: almost a tone) */
#define EVA_TUNED_RMS 0.577f                       /* its gain: as loud as a saw (rms ~ sqrt(k fc / fs), measured) */
/* -- the rest of the chain */
#define EVA_CHIFF_LVL 0.35f                        /* CHIFF 127: the burst at 0.35 x sqrt(breath) */
#define EVA_CHIFF 0.9761f                          /* its decay a block: 30 ms */
#define EVA_SHP_DRIVE 4.f                          /* SHAPE 127: drive x 4 at full level, half of it blown softly */
#define EVA_SHP_COMP 0.3f                          /* the shaper's gain back down: 1 / (1 + 0.3 x drive) */
#define EVA_FMT_K 0.12f                            /* the formant's band-passes: damping 0.12 (Q ~8) */
#define EVA_FMT_F2 0.7f                            /* the second formant -3 dB, as in a voice */
#define EVA_FMT_GAIN 2.5f                          /* their sum made up to the dry's level */
#define EVA_OUT_LVL 24576.f                        /* the output, the other engines' level (dsp.c VOICE_FS's scale) */
#define EVA_OUT_CLIP 1.5f                          /* clipped past 1.5 (a resonant peak's ceiling) */
/* (the humanise, a random pitch / cutoff / level on each tongued note, 15 % fixed and hidden: gone 2026-10-28; each
 * fingering's own fixed offsets are the layer's QUIRKS now, core/layers.c) */
/* VOWEL's order, A E I O U (its knob goes from A to U), from the shared table (dsp/dspx.c DSP_VOWEL_HZ: F1, F2; F3
 * not used); EVA's own table of the same until 2026-10-28 */
static const uint8_t EVA_VOWEL[5] = {DSP_VOWEL_A, DSP_VOWEL_E, DSP_VOWEL_I, DSP_VOWEL_O, DSP_VOWEL_U};
/* ==== */

/* HARMONIC: sin k x, k = 1 .. n (the last one faded in), 1 / k each, below 0.45 fs (Chebyshev: one sine a sample) */
static float eva_harm(float ph, float dt, float n)
{
    float c = dspf_sin(ph + 0.25f >= 1.f ? ph - 0.75f : ph + 0.25f), s1 = dspf_sin(ph), s0 = 0.f, sk = s1, y = 0.f;
    int32_t k, top = (int32_t)n + 1;
    if ((float)top * dt > EVA_HARM_TOP) top = (int32_t)(EVA_HARM_TOP / dt);
    if (top > 16) top = 16;
    for (k = 1; k <= top; k++) {
        float a = 1.f / (float)k, t;
        if (k == top && (float)k > n) a *= n - (float)(k - 1);   /* (the last one fading in) */
        y += a * sk;
        t = 2.f * c * sk - s0;
        s0 = sk;
        sk = t;
    }
    return 0.6f * y;                               /* (0.6: the sum of 1 / k up to 16 kept near a saw's peak) */
}

enum { EF_LP2, EF_LP4, EF_BP, EF_HP, EVA_NFILT };
static const char *const EVA_N_FILT[EVA_NFILT] = {"LP2", "LP4", "BP", "HP"};

static void eva_note_on(track_t *t, voice_t *v)   /* an articulated note: the sweep and the chiff kick */
{
    eva_t *s = &eva_st[(uint32_t)(t - trk) % NLAYER];
    (void)v;
    if (s->gain == 0.f) {                          /* from silence: the noises start the same each time (a puff, a */
        s->noise = 0x9E3779B9u;                    /* test's take: as NOISE's were, seeded by the note) */
        s->nz[0] = s->nz[1] = s->nz[2] = s->nz[3] = 0.f;
    }
    s->sweep = 1.f;
    s->chiff = 1.f;
}

/* every block, sounding or not: a layer whose voice has faded out is at rest, all of it cleared (the followers, the
 * filters' and the formant's states, the phases, the noises): a note from silence starts the same each time, not
 * from what the last note's filter was still ringing down to. (Zero is each part's start: eva_note_on seeds the
 * noises, the render takes its coefficients and SUPERSAW's phases afresh) */
static void eva_block(track_t *t)
{
    eva_t *s = &eva_st[(uint32_t)(t - trk) % NLAYER];
    if (!t->v[0].active)
        memset(s, 0, sizeof *s);
}

static void eva_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    eva_t *s = &eva_st[(uint32_t)(t - trk) % NLAYER];
    int32_t wave = clamp(p[P_E1], 0, EVA_NWAVE - 1);
    float br = (float)(m->shape - (64 << 8)) * (1.f / (63.f * 256.f));   /* the breath, 0 .. 1 (BR>SHP 63: all of it) */
    float sed = (float)p[P_E2] / 127.f, res = (float)p[P_E4] / 127.f, x, af, amp, oct, fc, g0, g1, b1, b2, b3;
    float dt, vibc, w = EVA_PULSE_W, shp = (float)p[P_E7] / 127.f, wsd, note = (float)m->pitch16 / 16.f;
    int32_t filt = clamp(p[P_E8], 0, EVA_NFILT - 1);
    float fmt = (float)p[P_E10] / 127.f, vw, tr, tr0, tr1;
    float ch0, ch1, ck = (float)p[P_E14] * (EVA_CHIFF_LVL / 127.f) * dspf_sqrt(br), fk = EVA_FMT_K;
    dspf_svf_t cf, cv1, cv2, ct = {0.f, 0.f, 0.f};   /* the filter's, the formant's two, TUNED's resonator */
    float tk = 0.f, tg = 0.f;
    uint32_t i, j;
    (void)v;
    br = dspf_clamp(br, 0.f, 1.f);
    /* the slots: BR_LVL, AMP's threshold; then AMP and FILTER each their curve, at once or after LAG (64: 8 / 25 ms,
     * the filter 12 / 40; each 32 twice as slow: RESPONSE's steps x 2^((64 - LAG) / 32), as RESPONSE = 128 - LAG did) */
    x = dspf_clamp(br * (float)p[P_E0] / EVA_BR_LVL, 0.f, 1.f);
    x = dspf_clamp((x - EVA_AMP_THRESH) / (1.f - EVA_AMP_THRESH), 0.f, 1.f);
    s->y = x;
    af = dspf_pow(s->y, EVA_FILT_CURVE);
    if (p[P_E15]) {
        float rsp = dspf_exp2((64.f - (float)p[P_E15]) / 32.f);
        s->za += dspf_clamp((s->y > s->za ? EVA_A_ATK : EVA_A_REL) * rsp, 0.f, 1.f) * (s->y - s->za);
        s->zf += dspf_clamp((af > s->zf ? EVA_F_ATK : EVA_F_REL) * rsp, 0.f, 1.f) * (af - s->zf);
    } else {
        s->za = s->y;
        s->zf = af;
    }
    /* S.E.D.: the gain's law, the filter's depth and floor */
    amp = dspf_pow(s->za, 1.f - EVA_SED_AMP * sed);
    oct = EVA_FILT_FLOOR + EVA_SED_FLOOR * (1.f - sed) + ((float)p[P_E3] - 64.f) * (EVA_CUT_RANGE / 64.f)
        + s->zf * (EVA_SED_DEPTH0 + EVA_SED_DEPTH1 * sed) * EVA_FILT_RANGE
        + s->sweep * ((float)p[P_E5] - 64.f) * (EVA_SWEEP_RANGE / 64.f)
        + (float)p[P_E13] / 127.f * (note - EVA_KT_PIVOT) / 12.f;
    s->sweep *= EVA_SWEEP;
    fc = dspf_clamp(EVA_FC_MIN * dspf_exp2(oct), EVA_FC_MIN, EVA_FC_MAX * (float)FS);
    dspf_svf_coef(fc, (filt == EF_LP4 ? EVA_LP4_K0 : EVA_RES_K0) - (filt == EF_LP4 ? EVA_LP4_K1 : EVA_RES_K1) * res, &cf);
    b1 = cf.a1;
    b2 = cf.a2;
    b3 = cf.a3;
    /* the formant: the vowel's two, in between interpolated */
    vw = (float)p[P_E9] * (4.f / 127.f);
    j = (uint32_t)vw;
    if (j > 3u) j = 3u;
    vw -= (float)j;
    {
        const float *a = DSP_VOWEL_HZ[EVA_VOWEL[j]], *b = DSP_VOWEL_HZ[EVA_VOWEL[j + 1u]];
        dspf_svf_coef(a[0] + (b[0] - a[0]) * vw, fk, &cv1);
        dspf_svf_coef(a[1] + (b[1] - a[1]) * vw, fk, &cv2);
    }
    /* vibrato: above 55 % breath, deeper and faster blown harder; in over VIB SWELL (64: 400 ms, out in 150) */
    x = dspf_clamp((br - EVA_VIB_ONSET) / (1.f - EVA_VIB_ONSET), 0.f, 1.f);
    if (p[P_E16]) {
        float ms = EVA_SWELL_MS * (float)p[P_E16] * (float)p[P_E16] / 4096.f, t = x > s->zv ? ms : EVA_SWELL_OUT * ms;
        s->zv += (x - s->zv) / (1.f + t * ((float)FS / (float)CTL / 1000.f));   /* (a block's step: ~1 - e^(-1/(t f))) */
    } else
        s->zv = x;
    s->vib += (EVA_VIB_HZ0 + EVA_VIB_HZ_SPAN * (float)p[P_E12] / 127.f + EVA_VIB_HZ_BR * s->zv) * (float)CTL / (float)FS;
    if (s->vib >= 1.f) s->vib -= 1.f;
    tr = s->vib + EVA_TREM_PHASE;                  /* the tremolo: a quarter cycle on (louder as the pitch rises) */
    if (tr >= 1.f) tr -= 1.f;
    tr0 = s->trem;
    tr1 = s->trem = 1.f - (float)p[P_E11] * (EVA_TREM_DEPTH / 127.f) * s->zv * (0.5f + 0.5f * dspf_sin(tr));
    vibc = (float)p[P_E6] * (EVA_VIB_CENTS / 127.f) * s->zv * dspf_sin(s->vib);
    dt = (float)m->inc * (1.f / 4294967296.f) * (1.f + vibc * 0.00057779f);   /* (2^(c/1200) ~ 1 + c ln2/1200) */
    if (wave == EW_DBL) {                          /* DBL: the width swept by an LFO 1.5 Hz at C4, faster up */
        s->pwm += EVA_DBL_HZ * dspf_exp2(0.5f * (note - 60.f) / 12.f) * (float)CTL / (float)FS;   /* (half an octave
                                                    * faster an octave up) */
        if (s->pwm >= 1.f) s->pwm -= 1.f;
        w = 0.5f + EVA_DBL_DEPTH * dspf_sin(s->pwm);
    }
    wsd = shp * (0.5f + 0.5f * s->za) * EVA_SHP_DRIVE;   /* the drive, more with breath */
    if (p[P_E15]) {                                /* the waves' breath: all of it, BREATH aside; after LAG as the filter
                                                    * (LAG 64: 12 / 40 ms, as it was until 2026-10-27) */
        float rsp = dspf_exp2((64.f - (float)p[P_E15]) / 32.f);
        s->zw += dspf_clamp((br > s->zw ? EVA_F_ATK : EVA_F_REL) * rsp, 0.f, 1.f) * (br - s->zw);
    } else
        s->zw = br;
    if (wave == EW_BPWM)
        w = EVA_BPWM_MIN + EVA_BPWM_SPAN * s->zw;
    if (wave == EW_HARM) {                         /* the harmonics with the breath, glided (no steps) */
        float nt = 1.f + EVA_HARM_MAX * s->zw;
        s->nh += EVA_HARM_GLIDE * (nt - s->nh);
    }
    if (wave == EW_TUNED) {                        /* TUNED: the resonator's damping, 0.5 blown softly .. 0.02 hard,
                                                    * and the gain that keeps it as loud as a saw (its noise power
                                                    * goes with its width: rms ~ sqrt(k fc / fs), measured) */
        float tf = dspf_clamp(dt * (float)FS, EVA_FC_MIN, EVA_FC_MAX * (float)FS);
        tk = EVA_TUNED_K_SOFT * dspf_exp2(EVA_TUNED_K_LOG * s->zw);
        dspf_svf_coef(tf, tk, &ct);
        tg = EVA_TUNED_RMS / dspf_sqrt(tk * tf / (float)FS);
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
        float a3 = s->a3 + (b3 - s->a3) * f, o, v1, v2, y, ch = ch0 + (ch1 - ch0) * f;
        switch (wave) {
        case EW_PULSE: o = dspf_pulse(s->ph, dt, EVA_PULSE_W); break;
        case EW_SQUARE: o = dspf_pulse(s->ph, dt, 0.5f); break;
        case EW_TRI: o = 1.f - 4.f * (s->ph < 0.5f ? 0.5f - s->ph : s->ph - 0.5f) * 1.f; o = -o; break;
        case EW_SINE: o = EVA_SINE_LVL * dspf_sin(s->ph); break;
        case EW_MULTI: o = 0.6f * (dspf_saw(s->ph, dt) + dspf_pulse(s->ph2, dt, EVA_PULSE_W)); break;   /* (0.6: the
                                                    * two together near one wave's level) */
        case EW_DBL: o = 0.5f * (dspf_saw(s->ph, dt) + dspf_pulse(s->ph, dt, w)); break;
        case EW_SYNC: o = eva_sync(s, dt, 1.f + EVA_SYNC_SPAN * s->zw); break;
        case EW_BPWM: o = dspf_pulse(s->ph, dt, w); break;
        case EW_HARM: o = eva_harm(s->ph, dt, s->nh); break;
        case EW_SUB:
            o = EVA_SUB_SAW * dspf_saw(s->ph, dt) + EVA_SUB_SQ * dspf_pulse(s->ph3, 0.5f * dt, 0.5f);
            s->ph3 += 0.5f * dt;
            if (s->ph3 >= 1.f) s->ph3 -= 1.f;
            break;
        case EW_SUPER: {
            float sp = EVA_SUPER_CENTS * (EVA_SUPER_SOFT + 0.4f * s->zw), d3;   /* (+-20 cents, 12 blown softly) */
            o = EVA_SUPER_MID * dspf_saw(s->ph, dt);
            for (j = 0; j < 6u; j++) {
                d3 = dt * (1.f + sp * EVA_SPREAD[j]);
                o += EVA_SUPER_SIDE * dspf_saw(s->ss[j], d3);
                s->ss[j] += d3;
                if (s->ss[j] >= 1.f) s->ss[j] -= 1.f;
            }
            break;
        }
        case EW_WHITE: o = dspf_urand(&s->noise); break;
        case EW_PINK: o = EVA_PINK_GAIN * dspf_pink(s->nz, dspf_urand(&s->noise)); break;
        case EW_BROWN: o = dspf_brown(&s->nz[3], dspf_urand(&s->noise)); break;
        case EW_TUNED: {                           /* white noise, its band-pass at the note (k bp: 1 at the centre) */
            float lp;
            o = tg * tk * dspf_svf(dspf_urand(&s->noise), ct.a1, ct.a2, ct.a3, s->tz, &lp);
            break;
        }
        default: o = dspf_saw(s->ph, dt);
        }
        if (wave != EW_SYNC) {                     /* (SYNC moves its own) */
            s->ph += dt;
            if (s->ph >= 1.f) s->ph -= 1.f;
        }
        s->ph2 += dt * EVA_MULTI_DT;               /* (MULTI's second wave) */
        if (s->ph2 >= 1.f) s->ph2 -= 1.f;
        v1 = dspf_svf(o, a1, a2, a3, s->flt, &v2);   /* the filter: v1 its band-pass, v2 its low-pass */
        switch (filt) {
        case EF_LP4:                               /* a second low-pass stage, the same cutoff, no resonance */
            dspf_svf(v2, a1, a2, a3, s->lp4, &y);
            y *= EVA_LP4_GAIN;
            break;
        case EF_BP: y = 2.f * (EVA_RES_K0 - EVA_RES_K1 * res) * v1; break;   /* (k v1: 1 at the centre; x 2: the
                                                    * band's narrower sum made up) */
        case EF_HP: y = o - v2 - (EVA_RES_K0 - EVA_RES_K1 * res) * v1; break;
        default: y = v2;
        }
        if (wsd > 0.f)                             /* the waveshaper, gain-compensated */
            y = dspf_sat(y * (1.f + wsd)) / (1.f + EVA_SHP_COMP * wsd);
        if (fmt > 0.f) {                           /* the formant: two band-passes, the second -3 dB */
            float lp, q = fk * dspf_svf(y, cv1.a1, cv1.a2, cv1.a3, s->fm1, &lp);
            q += EVA_FMT_F2 * fk * dspf_svf(y, cv2.a1, cv2.a2, cv2.a3, s->fm2, &lp);
            y += fmt * (EVA_FMT_GAIN * q - y);
        }
        y *= (g0 + (g1 - g0) * f) * (tr0 + (tr1 - tr0) * f);
        if (ch > 0.f) {                            /* the chiff: high-passed noise, as loud as the breath at once */
            float z = dspf_urand(&s->noise);
            y += ch * (z - s->chz);
            s->chz = z;
        }
        out[i] += voice_amp((int32_t)(dspf_clamp(y, -EVA_OUT_CLIP, EVA_OUT_CLIP) * EVA_OUT_LVL), m, i);
    }
    s->a1 = b1;
    s->a2 = b2;
    s->a3 = b3;
}

/* (rsp: each preset's RESPONSE as it was made, kept for the record and read by nothing: every preset has LAG 0, the
 * breath at once, and VIB SWELL 64, the vibrato's 400 ms swell as it was; the user, 2026-10-28: a delay only as a
 * swell) */
#define EAP(n, b, w, sed, cut, res, swp, vib, shp, flt, vow, fmt, trm, rate, kt, chf, rsp) \
    {n, {b, w, sed, cut, res, swp, vib, shp, flt, vow, fmt, trm, rate, kt, chf, 0, 64}, {0, 0, 127, 25}, 0, 1, {0}, 0}
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
    /* the noises (18 ..): for a layer of its own beside a tone, quick to follow the breath, no vibrato. A puff wants
     * the layer's ENVELOPE short (ATTACK 0, DECAY ~30 .. 70 ms, SUSTAIN 0): on each tongued note, not on a slur */
    /* (LAG none, rsp 128: a puff at once; their levels were set without a lag, 2026-10-27) */
    EAP("EVA NS_PUFF", 127, EW_PINK,  30,    80, 25, 104, 0,  0, EF_LP2,  0,  0,  0, 64, 38,  0, 128),   /* "puh" */
    EAP("EVA NS_CHIFF",127, EW_TUNED, 30,   100,  0, 64,  0,  0, EF_LP2,  0,  0,  0, 64, 38,  0, 128),   /* (TRANS +12) */
    EAP("EVA NS_HUFF", 127, EW_BROWN, 30,    90,  0, 80,  0,  0, EF_LP2,  0,  0,  0, 64, 38,  0, 128),   /* "huh" */
    EAP("EVA NS_AIR",  127, EW_PINK,  30,   104, 20, 64,  0,  0, EF_BP,   0,  0,  0, 64, 64,  0, 128),   /* (held) */
};
#undef EAP

static const engine_t ENG_EVA = {
    .name = "EVA",
    .page_title = {"EVA", "EVA +", "EVA COLOUR", "EVA PLAY", "EVA VIB"},
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
        {"LAG", F_PCT, 0, 127, 0, 0, 0},       /* (0 none; RESPONSE until 2026-10-27, the other way round) */
        {"VIB SWELL", F_PCT, 0, 127, 0, 0, 0}, /* (0 none; 64 = 400 ms, fixed until 2026-10-27: the presets have it) */
    },
    .presets = EVA_PRESETS,
    .npresets = NELEM(EVA_PRESETS),
    .knob = {0, 2, 3, 4},
    .block = eva_block,
    .note_on = eva_note_on,
    .render = eva_render,
};
