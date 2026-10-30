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
 * instrument's: core/params.c refuses a second). His controls are the layer's routes' targets (core/routes.c, HOME's
 * grid; CHANGES.md W5, W9), 0 .. 127 as his controllers would send them:
 *   EXCITE    his breath (A -> EXCITE 100 % by default: the breath after its curve), through his pp / mf / ff markers
 *             (EDIT 1)
 *   LIP       his bend, through his bend markers (EDIT 2): rest, the semitone's plateau (SEMI LO .. SEMI HI), the whole
 *             tone (FULL); the pitch only goes down. No route: the lip at rest
 *   WOBBLE    his vibrato: a controller's wobble (high-passed at 0.5 Hz), TILT DEPTH cents a g (EDIT 3). A head's
 *             wobble on his TEControl; a controller that does not move, no vibrato
 *   LFO DEPTH typhoon's vibrato for a controller that is held, not wobbled (CHANGES.md W9): a sine at RATE, fed to his
 *             vibrato input as a wobbling controller would be, +-40 cents at 100 %; with CENTRE above 0, its depth is
 *             how far the route's value is from CENTRE (a gyro: the re.corder's rotation, tilted either way)
 *             Either way his vibrato moves the lip's covering: up from rest it has only his 9 cents of room, so a
 *             deep vibrato dips more than it rises
 * typhoon's FINE, PITCH and QUIRKS pitch do not reach him: his pipes are cut to their notes (CHANGES.md W8). */
#include "denis/panflute.h"
#include "fastmath.h"                              /* (fm_sin_turns: the LFO) */

#define PAN_OUT_LVL 16384.f                        /* his 1.0 (his page's full scale) into the layer: -6 dB, as his page
                                                    * starts its volume (the high pipes at ff go over 1.0) */
#define PAN_OUT_MAX 65535                          /* a sample's bound into the layer (2x full scale: his ff peaks) */
#define PAN_KEY_LO 67                              /* his pipes: G4 .. */
#define PAN_KEY_HI 91                              /* .. G6 (his coefficients.h, PF_FIRST_KEY and PF_PIPES) */
/* the LFO (CHANGES.md W9) */
#define PAN_LFO_MID 61.5f                          /* (his tilt's centre: config.h PF_VIBRATO_CENTRE; his g = (value -
                                                    * 61.5) / 61, his cents = g x TILT DEPTH) */
#define PAN_LFO_UPG 61.f                           /* (his controller units a g) */
#define PAN_LFO_CPG 61                             /* his depth while only the LFO drives his vibrato (no WOBBLE route):
                                                    * 61 cents a g, one controller unit a cent (as VIB MODE LFO had it) */
#define PAN_LFO_MAX_CENTS 40.f                     /* LFO DEPTH 100 %: +-40 cents (a breath vibrato's widest) */
#define PAN_LFO_HZ_LO 3.f                          /* RATE 0 % .. */
#define PAN_LFO_HZ_HI 8.f                          /* .. 100 %: a player's vibrato rates (as ERS's VIB RATE) */
#define PAN_VIB_DEAD 4                             /* LFO DEPTH units (of 127) either side of CENTRE that count as none:
                                                    * a gyro's rest wanders */
#define PAN_VIB_SMOOTH_MS 50.f                     /* the depth follows LFO DEPTH over this (the re.corder's rotation
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

/* his vibrato depth, cents a g: TILT DEPTH with a WOBBLE route (his controller's wobble), PAN_LFO_CPG with the LFO
 * alone (layer l's routes) */
static int32_t pan_cg(const int16_t *p, uint32_t l)
{
    return (layer_in_on[l] & 4u) ? clamp(p[P_E8], 1, 127) : PAN_LFO_CPG;
}

/* the knobs onto his markers, each in order (pp < mf < ff; rest < semi low <= semi high < full): a knob out of order
 * is taken as the nearest in order. He refuses a marker that breaks the order for a moment, so they go in over a
 * few passes, the ones that fit first */
static void pan_markers(const int16_t *p, int32_t cg)   /* cg: his vibrato depth, cents a g (pan_cg) */
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
    want[PF_CAL_VIBRATO_DEPTH] = (int16_t)cg;
    for (pass = 0; pass < 4u; pass++)
        for (i = 0; i < PF_CAL_ITEMS; i++)
            if (pan.cal[i] != want[i] && pf_calibrate((int)i, (float)want[i]))
                pan.cal[i] = want[i];
}

/* a value onto one of his controls, when it moved */
static void pan_control(int c, int16_t *had, int32_t v)
{
    v = clamp(v, 0, 127);
    if (v != *had) {
        pf_control(c, (int)v);
        *had = (int16_t)v;
    }
}
/* a route's value (layer_in, Q15) as his controller's 0 .. 127 */
static int32_t pan_in7(int32_t q15) { return (q15 * 127 + 16384) >> 15; }

/* his vibrato input: WOBBLE's value (his centre with none) and the LFO's sine: its depth LFO DEPTH (with CENTRE: its
 * source's distance to CENTRE, times its route's amount; smoothed), in his units (cents x his units a g / TILT DEPTH) */
static void pan_vibrato(const int16_t *p, uint32_t l, uint32_t n)
{
    int32_t c = p[P_E10], d, span, w = layer_in_w[l][3];
    float a = 0.f, k = (float)n * 1000.f / (float)FS, v;   /* (k: this block's ms) */
    if ((layer_in_on[l] & 8u) && w) {              /* LFO DEPTH (T_IN3) */
        d = pan_in7(layer_in[l][3] * 100 / w);     /* (its source, as one) */
        if (c > 0) {                               /* (CENTRE: how far from it, either way) */
            d -= c;
            d = d < 0 ? -d : d;
            span = (c > 127 - c ? c : 127 - c) - PAN_VIB_DEAD;
            a = span > 0 ? fm_clampf((float)(d - PAN_VIB_DEAD) / (float)span, 0.f, 1.f) : 0.f;
        } else
            a = fm_clampf((float)d / 127.f, 0.f, 1.f);
        a = fm_clampf(a * (float)w / 100.f, 0.f, 1.f);
    }
    pan.amt += (a - pan.amt) * k / (PAN_VIB_SMOOTH_MS + k);
    pan.ph += (float)n / (float)FS * (PAN_LFO_HZ_LO + (PAN_LFO_HZ_HI - PAN_LFO_HZ_LO) * (float)p[P_E9] / 127.f);
    pan.ph -= fm_floorf(pan.ph);
    v = (layer_in_on[l] & 4u) ? (float)pan_in7(layer_in[l][2]) : PAN_LFO_MID;   /* WOBBLE (T_IN2) */
    v += pan.amt * PAN_LFO_MAX_CENTS * PAN_LFO_UPG / (float)pan_cg(p, l) * fm_sin_turns(pan.ph);
    pan_control(PF_VIBRATO, &pan.vib, (int32_t)fm_floorf(v + 0.5f));
}

/* once an audio block, core 1: the knobs, the key, the controls, then his block handed to core 2 (1) or rendered
 * here (0) */
static int pan_audio(track_t *t, uint32_t n)
{
    const voice_t *v = &t->v[0];
    const int16_t *p = t->p;
    int32_t key;
    uint32_t i, l = (uint32_t)(t - trk) % NLAYER;
    if (!pan.up) {
        pf_init();
        for (i = 0; i < PF_CAL_ITEMS; i++)
            pan.cal[i] = (int16_t)pf_calibration((int)i);
        pan.key = -1;
        pan.br = pan.bend = pan.vib = -1;
        pan.up = 1;
    }
    pan_markers(p, pan_cg(p, l));
    if (v->active && v->gate) {                    /* the key: the note, transposed (G_XPOSE, TRANSP) */
        key = v->pitch16 >> 4;
        while (key < PAN_KEY_LO) key += 12;        /* folded by octaves into his pipes (CHANGES.md W6) */
        while (key > PAN_KEY_HI) key -= 12;
        if (key != pan.key) {                      /* (before the breath: a fresh note's breath finds it there) */
            pf_note_on((int)key);
            pan.key = (int8_t)key;
        }
    }
    /* EXCITE his breath (a faded or silenced layer: none, he stops); LIP his bend (no route: at rest) */
    pan_control(PF_BREATH, &pan.br, v->active ? pan_in7(layer_in[l][0]) : 0);
    pan_control(PF_BEND, &pan.bend, (layer_in_on[l] & 2u) ? pan_in7(layer_in[l][1]) : p[P_E4]);
    pan_vibrato(p, l, n);
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

/* his defaults but the breath's (a TEControl's 4 / 12 / 33: CHANGES.md W4) */
static const preset_t PAN_PRESETS[] = {
    {"PAN FLUTE", {20, 45, 90, 0, 0, 30, 35, 90, 75, 64, 0}, {0, 0, 127, 30}, 0, 1, {0}, 0},
};

static const engine_t ENG_PAN = {
    .name = "PANFLUTE",
    .page_title = {"PAN BREATH", "PAN BEND", "PAN VIBRATO", 0, 0},
    .in = {"LIP", "WOBBLE", "LFO DEPTH"},          /* (routes' targets T_IN1 .. 3: his bend, his tilt, the LFO's depth) */
    .edit = {
        {"PP", F_INT, 1, 125, 20, 0, 0},           /* the breath value of his pp (384 Pa on G4) */
        {"MF", F_INT, 2, 126, 45, 0, 0},           /* of mf (480 Pa) */
        {"FF", F_INT, 3, 127, 90, 0, 0},           /* of ff (960 Pa); past it, the last slope on */
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"REST", F_INT, 0, 124, 0, 0, 0},          /* the bend controller's value at rest */
        {"SEMI LO", F_INT, 1, 125, 30, 0, 0},      /* where the semitone's plateau starts */
        {"SEMI HI", F_INT, 1, 126, 35, 0, 0},      /* and ends */
        {"FULL", F_INT, 2, 127, 90, 0, 0},         /* the whole tone */
        {"TILT DEPTH", F_INT, 1, 127, 75, 0, "c/g"},   /* his: WOBBLE's cents a g (61 of its units a g) */
        {"RATE", F_PCT, 0, 127, 64, 0, 0, MOD},    /* the LFO: 3 .. 8 Hz (64: 5.5) */
        {"CENTRE", F_INT, 0, 127, 0, 0, 0},        /* LFO DEPTH's value of no vibrato (0: none, the value is the depth;
                                                    * 64: a gyro's middle, its distance either way) */
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = PAN_PRESETS,
    .npresets = NELEM(PAN_PRESETS),
    .knob = {0, 1, 2, 8},
    .note_on = pan_note_on,
    .render = pan_render,
    .audio = pan_audio,
    .audio_done = pan_audio_done,
};
