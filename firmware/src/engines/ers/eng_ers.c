/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* ERS: exciter -> resonator -> shaper (ers.h: what it is, the knobs, the parts). This file: the presets, which model
 * a choice of exciter and resonator plays, the knobs' labels, and the render: the stages in order. */
#include "ers.h"
#include "ers_valve.c"
#include "ers_exciters.c"
#include "ers_bores.c"
#include "ers_resonators.c"
#include "ers_shapers.c"

#define EP(n, b, x, c, r, d, br, s, a) {n, {b, x, c, r, d, br, s, a, 0, 64, 0, 0, 64, 64, 64, 0, 60, 20, 40, 64}, \
                                        {0, 0, 127, 25}, 0, 1, {0}, 0}
/* (EDIT 3 / 4 at their defaults: VIB 0, RATE 64, TONGUE 0, LAG 0, AIR 64, TILT 64, PR>PITCH 64, DRIFT 0; EDIT 5 as ERS
 * played until 2026-10-28: the breath LP at the mix's old one, LP CUT 60, LP RES 20, BR>LP +40, VIB SWELL 64: 400 ms) */
static const preset_t ERS_PRESETS[] = {
    /*  name          BLOW EXCITER    COLOR RESONATOR  DECAY BRIGHT SHAPER    AMOUNT */
    EP("FLUTE",       0, EX_NOISE,   30, RS_TUBE,    50, 70, SH_BYPASS,  0),
    EP("CLARINET",    0, EX_REED,    64, RS_TUBE,    90, 60, SH_SAT,    20),
    EP("BOTTLE",      0, EX_NOISE,   60, RS_MODAL,  110, 30, SH_BYPASS,  0),
    EP("BOWED BAR",   0, EX_BOW,     60, RS_MODAL,  115, 70, SH_BYPASS,  0),
    EP("WHISPER",     0, EX_NOISE,  100, RS_FORMANT, 60, 50, SH_BYPASS,  0),
    EP("SINGER",      0, EX_OSC,     40, RS_FORMANT, 80, 60, SH_FILTER, 90),
    EP("REED BUZZ",   0, EX_REED,    80, RS_BYPASS,   0,  0, SH_FOLD,   30),
    EP("COMB SAW",    0, EX_OSC,      0, RS_TUBE,   100, 80, SH_FILTER, 70),
    EP("BOWED STR",   0, EX_BOW,     60, RS_TUBE,    90, 70, SH_SAT,    10),
    EP("TRUMPET",     0, EX_LIP,     64, RS_BRASS,   64, 80, SH_BELL,   50),   /* 9 */
    EP("MUTED TPT",   0, EX_LIP,     64, RS_BRASS,   64, 80, SH_MUTE,  110),
    EP("WAH TPT",     0, EX_LIP,     64, RS_BRASS,   64, 80, SH_WAH,    50),
    EP("FLUGEL",      0, EX_LIP,     60, RS_CONE,    50, 50, SH_FILTER, 80),
    EP("HORN",        0, EX_LIP,     58, RS_BRASS,   70, 30, SH_FILTER, 60),
    EP("TENOR SAX",   0, EX_SAX,     50, RS_CONE,    50, 70, SH_BODY,   40),   /* 14 */
    EP("GROWL SAX",   0, EX_SAX,     70, RS_CONE,    45, 80, SH_GROWL,   0),
    EP("OBOE",        0, EX_DREED,   70, RS_CONE,    30, 110, SH_BODY,  50),   /* (speaks C4 .. C7: measured) */
    EP("BASSOON",     0, EX_DREED,   70, RS_CONE,    70, 30, SH_BODY,   60),
    EP("CRUMHORN",    0, EX_DREED,   80, RS_TUBE,    90, 80, SH_SAT,    30),
    EP("RECORDER",    0, EX_JET,     25, RS_PIPE,    50, 80, SH_BYPASS,  0),   /* 19 */
    EP("FLUE PIPE",   0, EX_JET,     10, RS_PIPE,    50, 60, SH_BYPASS,  0),
    EP("SHAKUHACHI",  0, EX_JET,     75, RS_TUBE,    50, 50, SH_AIR,    50),
    EP("CHALUMEAU",   0, EX_REED,    70, RS_HOLE,    30, 120, SH_BODY,  40),
    EP("HARMONICA",   0, EX_FREE,    70, RS_FORMANT, 60, 40, SH_SAT,    15),   /* 23 */
    EP("BLUES HARP",  0, EX_FREE,   105, RS_FORMANT, 80, 25, SH_SAT,    45),
    EP("ACCORDION",   0, EX_FREE,    55, RS_CAVITY,  50, 75, SH_BODY,   30),
    EP("REED ORGAN",  0, EX_FREE,    35, RS_CAVITY,  30, 55, SH_FILTER, 90),
    EP("FM TUBE",     0, EX_FM,       1, RS_TUBE,    30, 70, SH_BYPASS,  0),   /* 27 (COLOR: RATIO 1) */
    EP("FM WOOD",     0, EX_FM,       3, RS_TUBE,    60, 40, SH_BODY,   40),
    EP("FM GROWL",    0, EX_FM,       2, RS_TUBE,   110, 60, SH_SAT,    40),
    /* 30 .. (2026-10-18, for the factory presets 15 .. 64): flutes */
    EP("PICCOLO",     0, EX_JET,     30, RS_TUBE,    45, 90, SH_BYPASS,  0),   /* 30 */
    EP("ALTO FLUTE",  0, EX_JET,     40, RS_TUBE,    55, 40, SH_AIR,    30),
    EP("BANSURI",     0, EX_JET,     55, RS_TUBE,    50, 55, SH_AIR,    40),
    EP("PAN FLUTE",   0, EX_JET,     45, RS_PIPE,    50, 40, SH_AIR,    60),
    EP("OCARINA",     0, EX_JET,     15, RS_PIPE,    50, 15, SH_FILTER, 40),
    EP("TIN WHISTLE", 0, EX_JET,     20, RS_PIPE,    40, 100, SH_BYPASS, 0),   /* 35 */
    /* reeds */
    EP("BASS CLAR",   0, EX_REED,    60, RS_TUBE,    95, 35, SH_SAT,    15),
    EP("SOPRANO SAX", 0, EX_SAX,     48, RS_SAX,     58, 103, SH_BODY,  30),   /* (SAX: tuned by measurement) */
    EP("ALTO SAX",    0, EX_SAX,     45, RS_SAX,     53, 85, SH_BODY,   45),
    EP("BARI SAX",    0, EX_SAX,     91, RS_SAX,     49, 72, SH_SAT,    46),
    EP("ENGL HORN",   0, EX_DREED,   60, RS_CONE,    40, 70, SH_BODY,   60),   /* 40 */
    EP("SHAWM",       0, EX_DREED,   95, RS_CONE,    30, 127, SH_SAT,   40),
    EP("MELODICA",    0, EX_FREE,    45, RS_CAVITY,  60, 95, SH_FILTER, 100),
    EP("SHENG",       0, EX_FREE,    30, RS_CAVITY,  70, 40, SH_BYPASS,  0),
    /* strings */
    EP("VIOLIN",      0, EX_BOW,     55, RS_TUBE,    95, 90, SH_BODY,   50),
    EP("VIOLA",       0, EX_BOW,     62, RS_TUBE,    90, 70, SH_BODY,   60),   /* 45 */
    EP("DOUBLE BASS", 0, EX_BOW,     70, RS_TUBE,    85, 50, SH_BODY,   70),
    EP("ERHU",        0, EX_BOW,     50, RS_TUBE,    90, 85, SH_BODY,   90),
    EP("GLASS HARM",  0, EX_BOW,     40, RS_MODAL,  100, 90, SH_BYPASS,  0),
    EP("TROMBONE",    0, EX_LIP,     50, RS_BRASS,   60, 50, SH_BELL,   40),
    /* FM in a tube */
    EP("FM FLUTE",    0, EX_FM,       1, RS_TUBE,    10, 90, SH_AIR,    30),   /* 50 */
    EP("FM REED",     0, EX_FM,       4, RS_TUBE,    50, 60, SH_BYPASS,  0),
    EP("FM BRASS",    0, EX_FM,       1, RS_TUBE,    80, 90, SH_BELL,   50),
    EP("FM CHAOS",    0, EX_FM,       5, RS_TUBE,   127, 80, SH_SAT,    30),
};
#undef EP

/* how a choice of exciter and resonator plays (ers.h M_*) */
static int32_t ers_model(int32_t exc, int32_t res)
{
    if (exc == EX_BOW)
        return M_BOW;                              /* (the string; MODAL ... take it in) */
    if (!ers_is_bore(res))
        return M_FF;
    if (exc == EX_FM)
        return M_FMB;                              /* (FM in any bore: feedback through it) */
    if (ers_is_jet(exc))
        return res == RS_PIPE ? M_FLUE : M_FLUTE;
    if (!ers_is_valve(exc))
        return M_FF;
    if (res == RS_CONE || res == RS_BRASS)         /* every valve plays them as the cone loop (BRASS: with a bell) */
        return M_CONE;
    if (res == RS_SAX)
        return M_SAX;
    return res == RS_HOLE ? M_VENT : M_CYL;
}

static void ers_init(track_t *t, voice_t *v)
{
    ers_t *st = &ers_st[(uint32_t)(t - trk) % NLAYER];
    if (v->s[7] != ERS_MAGIC) {                    /* the layer took this engine: clean lines */
        memset(st, 0, sizeof *st);
        v->s[7] = ERS_MAGIC;
    }
    if (!st->seed)
        st->seed = 0x13579BDF + (int32_t)v->age;
}

/* every block, sounding or not: a layer whose voice has faded out is at rest, as an instrument would be by its next
 * note (its lines are not rendered while silent: they would keep the last note's energy, a stale string or bore
 * sounding at the next note's start with no breath). Once: model 0 until it plays again */
static void ers_block(track_t *t)
{
    ers_t *st = &ers_st[(uint32_t)(t - trk) % NLAYER];
    if (!t->v[0].active && st->model) {
        int32_t seed = st->seed;
        memset(st, 0, sizeof *st);
        st->seed = seed;
    }
}

static void ers_note_on(track_t *t, voice_t *v)
{
    ers_init(t, v);
    ers_st[(uint32_t)(t - trk) % NLAYER].tg = ERS_ONE;   /* TONGUE's kick */
    if (t->p[P_E1] == EX_JET)
        ers_st[(uint32_t)(t - trk) % NLAYER].ch = 1764;   /* JET: a 40 ms chiff (a fresh note only: no legato) */
}

/* a one-pole's step a block (Q14) for a time of ms: 16384 / (1 + ms x 1.378 blocks a ms), rounded */
static int32_t ers_pole(int32_t ms)
{
    int32_t d = 1000 + ms * 1378;
    return (16384000 + d / 2) / d;
}

static void ers_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    ers_t *st = &ers_st[(uint32_t)(t - trk) % NLAYER];
    int32_t pr = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8);   /* the pressure, Q14: 64 = 1.0 */
    int32_t g0, g1 = ERS_ONE, c8 = 0;
    vmod_t mm = *m;                                /* (the note's increment, with VIB, PR>PITCH, DRIFT on it) */
    uint32_t inc8;
    ers_blk_t b;
    if (p[P_E11]) {                                /* LAG: the pressure follows, 1 .. 1/128 a block */
        int32_t k = p[P_E11] * 56 / 127, a = ERS_ONE >> (k >> 3);
        a -= ((a >> 1) * (k & 7)) >> 3;
        st->pl += ((pr - st->pl) * a) >> 14;
        pr = st->pl;
    } else
        st->pl = pr;
    if (p[P_E10]) {                                /* TONGUE: a kick of the pressure, 40 ms */
        pr = clamp(pr + ((((pr * p[P_E10]) >> 8) * st->tg) >> 14), 0, 127 << 8);
        st->tg = (st->tg * 16088) >> 14;
    }
    if (p[P_E8]) {                                 /* VIB: in above 55 % of a full tone, over VIB SWELL (64: in 400 ms,
                                                    * out 150; 0: at once) */
        int32_t x = clamp((pr - 9011) * 16384 / 7373, 0, ERS_ONE), sw = p[P_E19];
        if (sw) {                                  /* (SWELL 64: 400 ms in, 150 out: 30 and 79, as it was fixed) */
            int32_t ms = 400 * sw * sw / 4096;
            st->zv += ((x - st->zv) * ers_pole(x > st->zv ? ms : ms * 3 / 8)) >> 14;
        } else
            st->zv = x;
        st->vph += (uint32_t)(3u * 3117u + (uint32_t)p[P_E9] * 15340u / 127u) * 1000u;   /* 3 .. 8 Hz a block */
        c8 += (((p[P_E8] * 100) * ((sine_i(st->vph) * st->zv) >> 15)) >> 14);   /* (cents Q8: 50 at 127) */
    }
    if (p[P_E14] != 64)                            /* PR>PITCH: +-30 cents at a full tone's pressure, from 1.0 */
        c8 += ((p[P_E14] - 64) * 122 * (pr - (64 << 8))) >> 14;
    if (p[P_E15]) {                                /* DRIFT: a new place every ~0.4 s, glided to */
        if (--st->dn <= 0) {
            st->dt = (int32_t)noise32(&st->seed) >> 17;   /* +-1, Q14 */
            st->dn = 512;
        }
        st->dr += ((st->dt - st->dr) * 66) >> 14;
        c8 += (((p[P_E15] * 2048 / 127) * st->dr) >> 14);   /* (8 cents Q8 at 127) */
    }
    if (c8)
        mm.inc = m->inc + (uint32_t)(int32_t)(mul64(m->inc, c8 * 9690) >> 32);
    inc8 = mm.inc >> 8;
    st->air = p[P_E12];
    b.tilt = p[P_E13] == 64 ? 0 : (p[P_E13] - 64) * 256;
    b.st = st;
    b.v = v;
    b.m = &mm;
    b.exc = clamp(p[P_E1], 0, ERS_NEXC - 1);
    b.res = clamp(p[P_E3], 0, ERS_NRES - 1);
    b.shp = clamp(p[P_E6], 0, ERS_NSHP - 1);
    b.model = ers_model(b.exc, b.res);
    b.col = p[P_E2];
    b.dec = p[P_E4];
    b.bri = p[P_E5];
    b.amt = clamp((p[P_E7] << 8) + m->cutoff, 0, 127 << 8);
    b.per8 = inc8 ? 0xFFFFFFFFu / inc8 : ERS_BORE << 8;
    b.n = n > CTL ? CTL : n;
    ers_init(t, v);
    if (st->model != b.model + 1) {                /* another model: the lines start clean */
        memset(st->bore, 0, sizeof st->bore);
        memset(st->jet, 0, sizeof st->jet);
        st->lp = st->dcx = st->dcy = st->vl = st->bl = st->ww = 0;
        st->sl = st->sn[0] = st->sn[1] = st->dip = 0;
        st->sinc = 0;
        st->model = b.model + 1;
    }
    if (b.shp == SH_GROWL)                         /* the growl is in the pressure */
        g1 = ers_growl(st, b.amt);
    g0 = st->gg ? st->gg : g1;
    st->gg = g1;
    b.p0 = (st->pp * g0) >> 14;
    b.dp = ((pr * g1) >> 14) - b.p0;
    st->pp = pr;

    if (b.model == M_BOW) {                        /* E (+ R, coupled) */
        ers_bow(&b);
        ers_resonate(&b);
    } else if (b.model != M_FF)
        ers_bore(&b);
    else {
        ers_excite(&b);                            /* E, then R */
        ers_resonate(&b);
    }
    ers_shape(&b, out);                            /* S, out */
}

/* the labels follow the choices: what a knob does in this exciter / resonator / shaper */
static const param_desc_t *ers_desc(const track_t *t, uint32_t k)
{
#define D(l, def) {l, F_PCT, 0, 127, def, 0, 0}
    static const char *const RATIO[8] = {"0.5", "1", "1.5", "2", "3", "4", "5", "7"};
    static const param_desc_t COLOR[ERS_NEXC] = {
        D("AIR", 30), D("STIFF", 30), D("WAVE", 30), D("PRESSURE", 30), D("LIPS", 30), D("STIFF", 30), D("BITE", 30),
        D("TURBUL", 30), D("RASP", 30), {"RATIO", F_ENUM, 0, 7, 1, RATIO, 0},
    };
    enum { DC_EMB, DC_FB, DC_DECAY, DC_RES, DC_NONE, DC_POS, DC_VENT, DC_VOICE, DC_SUS };
    static const param_desc_t DECAY[9] = {
        D("EMBOUCH", 50), D("FEEDBK", 50), D("DECAY", 50), D("RES", 50), D("-", 50), D("POSITN", 50), D("VENT", 50),
        D("VOICING", 50), D("SUSTAIN", 50),
    };
    static const param_desc_t BRIGHT[4] = {D("BRIGHT", 70), D("VOWEL", 70), D("-", 70), D("FREQ", 70)};
    static const param_desc_t AMOUNT[ERS_NSHP] = {
        D("DRIVE", 0), D("FOLDS", 0), D("CUTOFF", 0), D("-", 0), D("FLARE", 0), D("MUTE", 0), D("OPEN", 0),
        D("BODY", 0), D("AIR", 0), D("GROWL", 0),
    };
#undef D
    int32_t exc = clamp(t->p[P_E1], 0, ERS_NEXC - 1), res = clamp(t->p[P_E3], 0, ERS_NRES - 1);
    int32_t model = ers_model(exc, res);
    switch (k) {
    case 2: return &COLOR[exc];
    case 4:
        if (res == RS_MODAL) return &DECAY[DC_DECAY];
        if (res == RS_FORMANT || res == RS_CAVITY) return &DECAY[DC_RES];
        if (res == RS_BYPASS) return &DECAY[DC_NONE];
        return &DECAY[model == M_FLUTE ? DC_EMB : model == M_FLUE ? DC_VOICE
                      : model == M_CONE || model == M_SAX ? DC_POS
                      : model == M_VENT ? DC_VENT : model == M_BOW ? DC_SUS : DC_FB];
    case 5: return &BRIGHT[res == RS_FORMANT ? 1 : res == RS_BYPASS ? 2 : res == RS_CAVITY ? 3 : 0];
    case 7: return &AMOUNT[clamp(t->p[P_E6], 0, ERS_NSHP - 1)];
    }
    return 0;
}

static const engine_t ENG_ERS = {
    .name = "ERS",
    .page_title = {"ERS", "ERS +", "ERS PLAY", "ERS AIR", "ERS BREATH"},
    .edit = {
        {"BLOW", F_PCT, 0, 127, 0, 0, 0},
        {"EXCITER", F_ENUM, 0, ERS_NEXC - 1, EX_NOISE, ERS_N_EXC, 0},
        {"COLOR", F_PCT, 0, 127, 30, 0, 0},
        {"RESONATOR", F_ENUM, 0, ERS_NRES - 1, RS_TUBE, ERS_N_RES, 0},
        {"DECAY", F_PCT, 0, 127, 50, 0, 0},
        {"BRIGHT", F_PCT, 0, 127, 70, 0, 0},
        {"SHAPER", F_ENUM, 0, ERS_NSHP - 1, SH_BYPASS, ERS_N_SHP, 0},
        {"AMOUNT", F_PCT, 0, 127, 0, 0, 0},
        {"VIB", F_PCT, 0, 127, 0, 0, 0},
        {"RATE", F_PCT, 0, 127, 64, 0, 0},
        {"TONGUE", F_PCT, 0, 127, 0, 0, 0},
        {"LAG", F_PCT, 0, 127, 0, 0, 0},       /* (back 2026-10-28, visible: 0 none; gone 2026-10-23 .. 28) */
        {"AIR", F_PCT, 0, 127, 64, 0, 0},
        {"TILT", F_PCT, 0, 127, 64, 0, 0},
        {"PR>PITCH", F_PCT, 0, 127, 64, 0, 0},
        {"DRIFT", F_PCT, 0, 127, 0, 0, 0},
        BLP_KNOBS,                             /* EDIT 5: the breath LP (dsp/breathlp.c; the mix's until 2026-10-28) */
        {"VIB SWELL", F_PCT, 0, 127, 0, 0, 0},  /* (0 none; 64 = 400 ms, fixed until 2026-10-28: the presets have it) */
    },
    .presets = ERS_PRESETS,
    .npresets = NELEM(ERS_PRESETS),
    .knob = {0, 2, 4, 7},
    .keep = 0x80u,                                 /* s[7]: the lines are this layer's */
    .blp = 17,                                     /* (EDIT 16 .. 18) */
    .block = ers_block,
    .note_on = ers_note_on,
    .render = ers_render,
    .desc = ers_desc,
};
