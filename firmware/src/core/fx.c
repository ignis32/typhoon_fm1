/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The sound after the layers are mixed (core/layers.c layers_audio: once a control block, CTL frames, both sides):
 *   drive    past the breath S_DRV_AT the mix is driven into a soft clipper (S_DRIVE), faded in from there
 *   LP       a low-pass that breath opens: S_LPF at no breath, + breath x S_BR_LP; resonance S_LPRES
 *   detune   a copy a few cents up and one down, a little late, spread left / right (a doubler, micro-pitch)
 *   delay    mono, up to ~1 s, its feedback low-passed (S_DLY_TONE); the echoes onto both sides
 *   reverb   PLATE / HALL / OLD (core/reverb.c)
 *   volume   the whole preset, last (S_VOLUME)
 * Every value through sv(): the preset's own copy of its group, or the global one (core/layers.h).
 * The delay's memory lives in the pool (.pool: zeroed at boot, not .bss); turning it on (MIX from 0) clears it first,
 * so an old tail never comes back. */
#define DLY_MAX 44100u                             /* 1 s */
static int16_t dly_buf[DLY_MAX] __attribute__((section(".pool")));

static struct {
    int32_t lp1[2], lp2[2];                        /* the LP, each side */
    uint8_t dly_on, dt_on, lp_on;                  /* (lp_on: the LP below its top, filtering: last block) */
    uint32_t dt_w, dt_ph;                          /* detune: the write index, the windows' phase */
    uint32_t dly_w;
    int32_t dly_len, dly_lp;                       /* the delay in samples (slews to the set time), its feedback LP */
} fx;

static inline int32_t sat16(int32_t x) { return x > 32767 ? 32767 : x < -32768 ? -32768 : x; }
/* a * b >> 15 rounded toward 0: in a feedback loop the plain shift rounds the negative half down, and a tail then
 * never dies (it settles as a small hum, a limit cycle); toward 0 every pass loses a little until it is 0 */
static inline int32_t mulz(int32_t a, int32_t b)
{
    int32_t q = a * b;
    return q >= 0 ? q >> 15 : -((-q) >> 15);
}

static void fx_reverb(int32_t *l, int32_t *r);    /* core/reverb.c */

static void fx_drive(int32_t *l, int32_t *r, int32_t breath)
{
    int32_t th = sv(S_DRV_AT) * 32767 / 127, over = breath - th, mix, g, d;
    uint32_t i;
    if (sv(S_DRIVE) <= 0 || over <= 0)
        return;
    mix = th < 32767 ? clamp((over << 15) / (32767 - th), 0, 32767) : 32767;
    g = 256 + ((sv(S_DRIVE) * 12 * mix) >> 15);   /* x1 .. x7 at full breath, DRIVE 127 */
    for (i = 0; i < CTL; i++) {
        d = softclip((l[i] * g) >> 8) * 3 / 4;     /* (the clipper's top, ~ a voice's level) */
        l[i] += (int32_t)(mul64(d - l[i], mix) >> 15);
        d = softclip((r[i] * g) >> 8) * 3 / 4;
        r[i] += (int32_t)(mul64(d - r[i], mix) >> 15);
    }
}

static void fx_lp(int32_t *l, int32_t *r, int32_t breath)
{
    int32_t cut = (sv(S_LPF) << 8) + ((breath * sv(S_BR_LP)) >> 6);
    tsvf_t c;
    int32_t any = 0, j, on;
    uint32_t i;
    for (i = 0; i < CTL; i++)
        any |= l[i] | r[i];
    for (j = 0; j < 2; j++)                        /* (an integer filter fed silence can hold a few LSB forever) */
        if (fx.lp1[j] > 256 || fx.lp1[j] < -256 || fx.lp2[j] > 256 || fx.lp2[j] < -256)
            any = 1;
    if (!any) {                                    /* silence in and nothing left: nothing to do */
        fx.lp1[0] = fx.lp2[0] = fx.lp1[1] = fx.lp2[1] = 0;
        fx.lp_on = 0;
        return;
    }
    /* open (past ~84 % breath): the sound passes as it is, but the filter runs on at its top, so it is in step when
     * breath falls back (started from an empty state it clicked, until 2026-10-23); in or out, the
     * block blends from one to the other (no step of the filter's own small delay either) */
    tsvf_coef(&c, clamp(cut, 0, 127 << 8), sv(S_LPRES));
    on = cut < (127 << 8);
    for (i = 0; i < CTL; i++) {
        int32_t yl = tsvf_lp(&c, l[i], &fx.lp1[0], &fx.lp2[0]), yr = tsvf_lp(&c, r[i], &fx.lp1[1], &fx.lp2[1]);
        if (on != fx.lp_on) {
            int32_t g = on ? (int32_t)i + 1 : CTL - 1 - (int32_t)i;   /* (of CTL: the filtered one's share) */
            l[i] += ((yl - l[i]) * g) >> CTL_LOG2;
            r[i] += ((yr - r[i]) * g) >> CTL_LOG2;
        } else if (on) {
            l[i] = yl;
            r[i] = yr;
        }
    }
    fx.lp_on = (uint8_t)on;
}

static void fx_delay(int32_t *l, int32_t *r)
{
    int32_t mixq = sv(S_DLY_MIX) * 258, fb = sv(S_DLY_FB) * 250, k = 2000 + sv(S_DLY_TONE) * 220;   /* Q15 */
    int32_t want = (int32_t)(time_ms(sv(S_DLY_TIME)) * 441u / 10u), step;
    uint32_t i, w = fx.dly_w;
    if (mixq <= 0) {
        fx.dly_on = 0;
        return;
    }
    if (!fx.dly_on) {
        memset(dly_buf, 0, sizeof dly_buf);
        fx.dly_on = 1;
        fx.dly_len = want;
        fx.dly_lp = 0;
    }
    want = clamp(want, CTL, (int32_t)DLY_MAX - 1);
    step = clamp(want - fx.dly_len, -64, 64);     /* a new time glides in (a tape's pitch bend, no click) */
    fx.dly_len = clamp(fx.dly_len + step, CTL, (int32_t)DLY_MAX - 1);
    for (i = 0; i < CTL; i++) {
        uint32_t rp = w >= (uint32_t)fx.dly_len ? w - (uint32_t)fx.dly_len : w + DLY_MAX - (uint32_t)fx.dly_len;
        int32_t rd = dly_buf[rp], in = (l[i] + r[i]) >> 1, wet;
        fx.dly_lp += mulz(rd - fx.dly_lp, k);
        dly_buf[w] = (int16_t)sat16(in + mulz(fx.dly_lp, fb));
        if (++w >= DLY_MAX)
            w = 0;
        wet = (rd * mixq) >> 15;
        l[i] += wet;
        r[i] += wet;
    }
    fx.dly_w = w;
}

/* ---- detune: two pitch-shifted copies of the mix, S_DT_CENTS up and down, S_DT_DELAY ms late; WIDTH puts the up
 * one left and the down one right (0: both in the middle). A shifter is a delay read at a slowly changing delay
 * (the pitch moves by the rate of change), as two taps half a window apart, each faded by a triangle so the jump
 * back to the window's start is never heard (the two windows add up to 1) */
#define DT_LEN 4096u                               /* (a power of 2) */
#define DT_W 1024u                                 /* the window, samples (23 ms) */
static int16_t dt_buf[DT_LEN] __attribute__((section(".pool")));
static inline int32_t dt_tap(uint32_t w, uint32_t d16)   /* d16: the delay in 1/65536 samples, interpolated */
{
    uint32_t di = d16 >> 16, f = (d16 & 0xFFFFu) >> 1;
    int32_t a = dt_buf[(w - di) & (DT_LEN - 1u)], b = dt_buf[(w - di - 1u) & (DT_LEN - 1u)];
    return a + (((b - a) * (int32_t)f) >> 15);
}
static int32_t dt_voice(uint32_t w, uint32_t p, uint32_t base16, int up)
{
    int32_t s = 0;
    uint32_t k;
    for (k = 0; k < 2u; k++, p += 0x80000000u) {
        uint32_t pos16 = (p >> 16) * DT_W, u = p >> 17;
        uint32_t d16 = up ? base16 + (DT_W << 16) - pos16 : base16 + pos16;   /* up: the delay shrinks */
        int32_t g = u < 16384u ? (int32_t)u * 2 : (32767 - (int32_t)u) * 2;
        s += (dt_tap(w, d16) * g) >> 15;
    }
    return s;
}
static void fx_detune(int32_t *l, int32_t *r)
{
    int32_t mixq = sv(S_DT_MIX) * 258, half = sv(S_DT_WIDTH) * 129;   /* Q15; half the width, Q15 */
    uint32_t inc = (uint32_t)sv(S_DT_CENTS) * 2423u;   /* the window's phase a sample: cents x ln2 / 1200 / DT_W x 2^32 */
    uint32_t base16 = ((uint32_t)sv(S_DT_DELAY) * 441u / 10u + 2u) << 16, i, w = fx.dt_w;
    if (mixq <= 0) {
        fx.dt_on = 0;
        return;
    }
    if (!fx.dt_on) {
        memset(dt_buf, 0, sizeof dt_buf);
        fx.dt_on = 1;
    }
    for (i = 0; i < CTL; i++) {
        int32_t up, dn, wl, wr;
        dt_buf[w & (DT_LEN - 1u)] = (int16_t)sat16((l[i] + r[i]) >> 1);
        up = dt_voice(w, fx.dt_ph, base16, 1);
        dn = dt_voice(w, fx.dt_ph + 0x40000000u, base16, 0);   /* (a quarter apart: the windows' dips do not meet) */
        fx.dt_ph += inc;
        wl = (up * (16384 + half) + dn * (16384 - half)) >> 15;
        wr = (dn * (16384 + half) + up * (16384 - half)) >> 15;
        l[i] += (wl * mixq) >> 15;
        r[i] += (wr * mixq) >> 15;
        w++;
    }
    fx.dt_w = w;
}

/* ---- volume: the whole preset louder or softer, effects and all (S_VOLUME, 100 = as it is) */
static void fx_volume(int32_t *l, int32_t *r)
{
    int32_t v = sv(S_VOLUME), g;
    uint32_t i;
    if (v == 100)
        return;
    g = v * 1024 / 100;                            /* Q10 */
    for (i = 0; i < CTL; i++) {
        l[i] = (l[i] * g) >> 10;
        r[i] = (r[i] * g) >> 10;
    }
}

static void fx_block(int32_t *l, int32_t *r, int32_t breath)
{
    fx_drive(l, r, breath);
    fx_lp(l, r, breath);
    fx_detune(l, r);
    fx_delay(l, r);
    fx_reverb(l, r);
    fx_volume(l, r);
}
