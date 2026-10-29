/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* PANFLUTE: Denis Rosset's pan flute (engines/pan/denis/: his model, MIT; every change from his own in
 * denis/CHANGES.md) as a typhoon engine. His model is one instrument: 25 stopped pipes G4 .. G6, an air jet blown
 * across each, the breath the blowing pressure, the lower lip covering the pipe's end to bend it down, the head's
 * wobble for the vibrato. Nothing is added for the attack or the release: breath starts the tone and stops it.
 *
 * This file: his three controls in, his sound out. He costs more than three of typhoon's other layers, so a whole
 * audio block is rendered at once (engine_t.audio) on the second core while the first renders the other layers
 * (cpu2_job; a build without core 2 renders it at once); one layer at a time plays him (his state is one
 * instrument's: core/params.c refuses a second).
 *   breath    typhoon's breath after the CURVE, 0 .. 127 (CHANGES.md W3), through his pp / mf / ff markers (EDIT 1)
 *   bend      a controller's raw value (BEND CC, EDIT 3; 0 none), through his bend markers (EDIT 2): rest, the
 *             semitone's plateau (SEMI LO .. SEMI HI), the whole tone (FULL); the pitch only goes down
 *   vibrato   VIB MODE (EDIT 3):
 *             TILT  his: a controller's raw value (VIB CC; 0 none), its wobble only (high-passed at 0.5 Hz), DEPTH
 *                   cents a g. A head's wobble on his TEControl; a controller that does not move, no vibrato
 *             LFO   typhoon's (CHANGES.md W9): a sine at RATE (EDIT 4), fed to his vibrato as a wobbling controller
 *                   would be; its depth DEPTH x how far VIB CC is from CENTRE (a gyro: the re.corder's rotation,
 *                   CC 52, tilted either way), or DEPTH always when VIB CC is 0
 *             Either way his vibrato moves the lip's covering: up from rest it has only his 9 cents of room, so a
 *             deep vibrato dips more than it rises
 * The controllers come from cc_raw (core/layers.h: every CC as it came). typhoon's FINE, pitch bend and QUIRKS pitch
 * do not reach him: his pipes are cut to their notes (CHANGES.md W8). */
#include "denis/panflute.h"
#include "fastmath.h"                              /* (fm_sin_turns: the LFO) */

#define PAN_OUT_LVL 16384.f                        /* his 1.0 (his page's full scale) into the layer: -6 dB, as his page
                                                    * starts its volume (the high pipes at ff go over 1.0) */
#define PAN_OUT_MAX 65535                          /* a sample's bound into the layer (2x full scale: his ff peaks) */
#define PAN_KEY_LO 67                              /* his pipes: G4 .. */
#define PAN_KEY_HI 91                              /* .. G6 (his coefficients.h, PF_FIRST_KEY and PF_PIPES) */
/* VIB MODE LFO (CHANGES.md W9) */
enum { PAN_VIB_TILT, PAN_VIB_LFO };
#define PAN_LFO_CPG 61                             /* his vibrato depth while the LFO drives it: 61 cents a g, so one
                                                    * controller unit is one cent (his g = (value - 61.5) / 61) */
#define PAN_LFO_MID 61.5f                          /* (his tilt's centre: config.h PF_VIBRATO_CENTRE) */
#define PAN_LFO_MAX_CENTS 40.f                     /* DEPTH 100 %: +-40 cents (a breath vibrato's widest) */
#define PAN_LFO_HZ_LO 3.f                          /* RATE 0 % .. */
#define PAN_LFO_HZ_HI 8.f                          /* .. 100 %: a player's vibrato rates (as ERS's VIB RATE) */
#define PAN_VIB_DEAD 4                             /* VIB CC units either side of CENTRE that count as none: a gyro's
                                                    * rest wanders */
#define PAN_VIB_SMOOTH_MS 50.f                     /* the depth follows VIB CC over this (the re.corder's rotation
                                                    * comes ~11 times a second: its steps joined); the vibrato's depth
                                                    * only, never the pitch's centre or the level (notes/03-lag-watch.md) */

static struct {
    int32_t out[APP_BLOCK];                        /* the block he rendered (on core 2) */
    int8_t key;                                    /* the key he plays, -1 none yet */
    uint8_t up;                                    /* pf_init done */
    int16_t cal[PF_CAL_ITEMS];                     /* the markers he has, as the knobs gave them */
    int16_t br, bend, vib;                         /* the values he has, -1 none yet */
    float ph, amt;                                 /* the LFO: its phase (turns), its depth now (0 .. 1) */
} pan = {.key = -1};

/* core 2 (or core 1 without it): n frames of him into pan.out, PF_BLOCK at a time */
static void pan_job(uint32_t n)
{
    uint32_t i = 0, j, k;
    while (i < n) {
        const float *o;
        k = n - i < PF_BLOCK ? n - i : PF_BLOCK;
        o = pf_render((int)k);
        for (j = 0; j < k; j++) {
            float s = o[j] * PAN_OUT_LVL;
            pan.out[i + j] = s > (float)PAN_OUT_MAX ? PAN_OUT_MAX : s < -(float)PAN_OUT_MAX ? -PAN_OUT_MAX : (int32_t)s;
        }
        i += k;
    }
}

/* the knobs onto his markers, each in order (pp < mf < ff; rest < semi low <= semi high < full): a knob out of order
 * is taken as the nearest in order. He refuses a marker that breaks the order for a moment, so they go in over a
 * few passes, the ones that fit first */
static void pan_markers(const int16_t *p)
{
    int16_t want[PF_CAL_ITEMS];
    uint32_t i, pass;
    want[PF_CAL_BREATH_PP] = (int16_t)clamp(p[P_E0], 1, 125);
    want[PF_CAL_BREATH_MF] = (int16_t)clamp(p[P_E1], want[PF_CAL_BREATH_PP] + 1, 126);
    want[PF_CAL_BREATH_FF] = (int16_t)clamp(p[P_E2], want[PF_CAL_BREATH_MF] + 1, 127);
    want[PF_CAL_BEND_REST] = (int16_t)clamp(p[P_E4], 0, 124);
    want[PF_CAL_BEND_SEMITONE_LOW] = (int16_t)clamp(p[P_E5], want[PF_CAL_BEND_REST] + 1, 125);
    want[PF_CAL_BEND_SEMITONE_HIGH] = (int16_t)clamp(p[P_E6], want[PF_CAL_BEND_SEMITONE_LOW], 126);
    want[PF_CAL_BEND_FULL] = (int16_t)clamp(p[P_E7], want[PF_CAL_BEND_SEMITONE_HIGH] + 1, 127);
    want[PF_CAL_VIBRATO_DEPTH] = (int16_t)(p[P_E9] == PAN_VIB_LFO ? PAN_LFO_CPG : clamp(p[P_E11], 0, 127));
    for (pass = 0; pass < 4u; pass++)
        for (i = 0; i < PF_CAL_ITEMS; i++)
            if (pan.cal[i] != want[i] && pf_calibrate((int)i, (float)want[i]))
                pan.cal[i] = want[i];
}

/* a controller's raw value onto one of his controls, when it moved (cc 0: none, nothing sent) */
static void pan_control(int c, int16_t *had, int32_t cc)
{
    int16_t v;
    if (cc <= 0 || cc > 127)
        return;
    v = cc_raw[cc];
    if (v != *had) {
        pf_control(c, v);
        *had = v;
    }
}

/* VIB MODE LFO: the sine, its depth from VIB CC's distance to CENTRE (smoothed), sent to him as his tilt would be */
static void pan_lfo(const int16_t *p, uint32_t n)
{
    int32_t cc = p[P_E10], c = p[P_E13], d, span, v;
    float a = 1.f, k = (float)n * 1000.f / (float)FS;   /* (k: this block's ms) */
    if (cc > 0 && cc <= 127) {
        d = cc_raw[cc] - c;
        d = d < 0 ? -d : d;
        span = (c > 127 - c ? c : 127 - c) - PAN_VIB_DEAD;
        a = span > 0 ? fm_clampf((float)(d - PAN_VIB_DEAD) / (float)span, 0.f, 1.f) : 0.f;
    }
    pan.amt += (a - pan.amt) * k / (PAN_VIB_SMOOTH_MS + k);
    pan.ph += (float)n / (float)FS * (PAN_LFO_HZ_LO + (PAN_LFO_HZ_HI - PAN_LFO_HZ_LO) * (float)p[P_E12] / 127.f);
    pan.ph -= fm_floorf(pan.ph);
    v = (int32_t)fm_floorf(PAN_LFO_MID + pan.amt * PAN_LFO_MAX_CENTS * (float)p[P_E11] / 127.f * fm_sin_turns(pan.ph)
                           + 0.5f);
    if (v != pan.vib) {
        pf_control(PF_VIBRATO, (int)clamp(v, 0, 127));
        pan.vib = (int16_t)v;
    }
}

/* once an audio block, core 1: the knobs, the key, the controls, then his block handed to core 2 (1) or rendered
 * here (0) */
static int pan_audio(track_t *t, uint32_t n)
{
    const voice_t *v = &t->v[0];
    const int16_t *p = t->p;
    int32_t br, key;
    uint32_t i;
    if (!pan.up) {
        pf_init();
        for (i = 0; i < PF_CAL_ITEMS; i++)
            pan.cal[i] = (int16_t)pf_calibration((int)i);
        pan.key = -1;
        pan.br = pan.bend = pan.vib = -1;
        pan.up = 1;
    }
    pan_markers(p);
    if (v->active && v->gate) {                    /* the key: the note, transposed (G_XPOSE, TRANSP) */
        key = v->pitch16 >> 4;
        while (key < PAN_KEY_LO) key += 12;        /* folded by octaves into his pipes (CHANGES.md W6) */
        while (key > PAN_KEY_HI) key -= 12;
        if (key != pan.key) {                      /* (before the breath: a fresh note's breath finds it there) */
            pf_note_on((int)key);
            pan.key = (int8_t)key;
        }
    }
    br = v->active ? (src_breath * 127 + 16384) >> 15 : 0;   /* (a faded or silenced layer: no breath, he stops) */
    if (br != pan.br) {
        pf_control(PF_BREATH, (int)br);
        pan.br = (int16_t)br;
    }
    pan_control(PF_BEND, &pan.bend, p[P_E8]);
    if (p[P_E9] == PAN_VIB_LFO)
        pan_lfo(p, n);
    else
        pan_control(PF_VIBRATO, &pan.vib, p[P_E10]);
    if (cpu2_job(pan_job, n))
        return 1;
    pan_job(n);
    return 0;
}

static void pan_audio_done(uint32_t n) { cpu2_done(pan_job, n); }

static void pan_note_on(track_t *t, voice_t *v)
{
    (void)t;
    (void)v;                                       /* (the key goes to him at the next audio block: pan_audio) */
}

static void pan_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t i;
    (void)t;
    (void)v;
    for (i = 0; i < n && m->off + i < APP_BLOCK; i++)
        out[i] += voice_amp(pan.out[m->off + i], m, i);
}

/* his defaults but the breath's (a TEControl's 4 / 12 / 33: CHANGES.md W4); VIB MODE TILT, his */
static const preset_t PAN_PRESETS[] = {
    {"PAN FLUTE", {20, 45, 90, 0, 0, 30, 35, 90, 0, PAN_VIB_TILT, 0, 75, 64, 64}, {0, 0, 127, 30}, 0, 1, {0}, 0},
};

/* the vibrato's knobs mean what VIB MODE says (a knob the mode does not use: "-") */
static const param_desc_t *pan_desc(const track_t *t, uint32_t k)
{
    static const param_desc_t TILT_DEPTH = {"VIB DEPTH", F_INT, 0, 127, 75, 0, "c/g"};   /* cents a g of the wobble
                                                    * (his tilt: 61 controller units = 1 g) */
    static const param_desc_t LFO_DEPTH = {"VIB DEPTH", F_PCT, 0, 127, 75, 0, 0};   /* 100 %: +-PAN_LFO_MAX_CENTS */
    static const param_desc_t NONE_RATE = {"-", F_PCT, 0, 127, 64, 0, 0};
    static const param_desc_t NONE_CENTRE = {"-", F_INT, 0, 127, 64, 0, 0};
    int lfo = t->p[P_E9] == PAN_VIB_LFO;
    switch (k) {
    case 11: return lfo ? &LFO_DEPTH : &TILT_DEPTH;
    case 12: return lfo ? 0 : &NONE_RATE;
    case 13: return lfo ? 0 : &NONE_CENTRE;
    }
    return 0;
}

static const char *const N_PAN_VIB[] = {"TILT", "LFO"};

static const engine_t ENG_PAN = {
    .name = "PANFLUTE",
    .page_title = {"PAN BREATH", "PAN BEND", "PAN CONTROL", "PAN LFO", 0},
    .edit = {
        {"PP", F_INT, 1, 125, 20, 0, 0},           /* the breath value of his pp (384 Pa on G4) */
        {"MF", F_INT, 2, 126, 45, 0, 0},           /* of mf (480 Pa) */
        {"FF", F_INT, 3, 127, 90, 0, 0},           /* of ff (960 Pa); past it, the last slope on */
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"REST", F_INT, 0, 124, 0, 0, 0},          /* the bend controller's value at rest */
        {"SEMI LO", F_INT, 1, 125, 30, 0, 0},      /* where the semitone's plateau starts */
        {"SEMI HI", F_INT, 1, 126, 35, 0, 0},      /* and ends */
        {"FULL", F_INT, 2, 127, 90, 0, 0},         /* the whole tone */
        {"BEND CC", F_INT, 0, 127, 0, 0, 0},       /* the controller the lip follows (0: none) */
        {"VIB MODE", F_ENUM, 0, 1, PAN_VIB_TILT, N_PAN_VIB, 0},   /* TILT his (the CC's wobble), LFO typhoon's */
        {"VIB CC", F_INT, 0, 127, 0, 0, 0},        /* TILT: the wobbling controller; LFO: the depth's (0: none) */
        {"VIB DEPTH", F_INT, 0, 127, 75, 0, "c/g"},   /* (pan_desc: c/g for TILT, % for LFO) */
        {"RATE", F_PCT, 0, 127, 64, 0, 0},         /* LFO: 3 .. 8 Hz (64: 5.5) */
        {"CENTRE", F_INT, 0, 127, 64, 0, 0},       /* LFO: VIB CC's value of no vibrato (64: a gyro's middle; 0: a
                                                    * wheel's bottom) */
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = PAN_PRESETS,
    .npresets = NELEM(PAN_PRESETS),
    .knob = {0, 1, 2, 11},
    .desc = pan_desc,
    .note_on = pan_note_on,
    .render = pan_render,
    .audio = pan_audio,
    .audio_done = pan_audio_done,
};
