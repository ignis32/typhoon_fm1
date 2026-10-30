/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The routes (notes/05-controls.md): what the sources (BEND, A .. D, AO .. DO: core/layers.h, set in GLO > CONTROLS) do
 * to a layer. A cell is a source -> a target of a layer, an amount -100 .. +100 %; the preset has up to NCELL of them,
 * in cells[] (layers.h), saved with it (core/presets.c). One source to many targets, many to one (they add).
 * No engine reads a source: what moves an engine is a cell, on HOME's grid (ui/ui.c).
 *
 * The targets (layers.h T_*): the layer's level (the source's share of it), its pitch, PAN, TONE's knobs, the engine's
 * EXCITE (vmod.shape: what breath moved; PANFLUTE takes it, and its own inputs T_IN1 .., from layer_in), and every
 * engine knob its descriptor marks MOD. A knob target is the knob plus what its cells add (100 % at a full source:
 * the knob's whole range), clamped to its range: the knob is the resting value. routes_apply moves the knobs in place
 * for one control block, routes_restore puts them back: the audio ISR runs whole, so the main loop (the UI, storage)
 * never sees a moved value.
 *
 * Defaults (routes_layer, routes_engine, routes_tone): every layer BEND -> PITCH 100 %; an engine the routes of
 * ROUTE_DEFAULTS (what breath moved in it before the routes); a TONE its own. And routes_preset: an engine preset's
 * knobs (its presets[], the factory's) that were depths with breath become A routes. So the presets sound as they did
 * with BR>AMP / BR>TONE / BR>SHP / BR>LP and the engines' hidden breath (until 2026-10-30: notes/05-controls.md) */

#define ROUTE_BR_TONE 31                           /* %: BR>TONE's +40 (its default, every layer's) moved a cutoff 40 of
                                                    * 127 at full breath: 31 % of the knob's range */
#define ROUTE_BR_LP 63                             /* %: BR>LP's +40 opened the breath LP's CUT 80 of 127 at full breath */
#define ROUTE_EVA_RATE 32                          /* %: EVA's vibrato quickened 1.6 Hz at full breath, RATE's 5 Hz span */

/* each engine's routes when a layer takes it: what breath moved in it */
static const struct { uint8_t id, t; int8_t amt; } ROUTE_DEFAULTS[] = {
    {ENG_ID_FM, T_EXCITE, 63},                     /* (its BR>SHP 40: 63 % of all of it) */
    {ENG_ID_FM, T_KNOB + 16, ROUTE_BR_LP},         /* LP CUT */
    {ENG_ID_ANALOG, T_KNOB + 4, ROUTE_BR_TONE},    /* CUT (its EXCITE, PWM, none) */
    {ENG_ID_ERS, T_EXCITE, 100},                   /* (the pressure: all of it) */
    {ENG_ID_ERS, T_KNOB + 7, ROUTE_BR_TONE},       /* AMOUNT, the shaper's */
    {ENG_ID_ERS, T_KNOB + 16, ROUTE_BR_LP},        /* LP CUT */
    {ENG_ID_EVA, T_EXCITE, 100},                   /* (the slots: all of it) */
    {ENG_ID_EVA, T_KNOB + 6, 50},                  /* VIB: its old default 64 came in with breath */
    {ENG_ID_EVA, T_KNOB + 12, ROUTE_EVA_RATE},     /* RATE */
    {ENG_ID_EVA, T_KNOB + 17, 100},                /* WAVE MOD: the waves breath plays */
    {ENG_ID_PAN, T_EXCITE, 100},                   /* (his breath) */
};

/* The main loop changes cells while the audio ISR reads them: a slot is free while its amount is 0, a new cell's
 * layer, target and source are written before its amount, a cell goes by its amount going to 0. Nothing moves, so
 * the ISR sees each cell whole, before or after, never a half or a copy */
static uint32_t cell_l(const cell_t *c) { return c->lt >> 6; }
static uint32_t cell_t_(const cell_t *c) { return c->lt & 63u; }
static uint32_t cells_n(void)                      /* (the cells there are) */
{
    uint32_t i, n = 0;
    for (i = 0; i < NCELL; i++)
        n += cells[i].amt != 0;
    return n;
}
static int32_t cell_find(uint32_t l, uint32_t t, uint32_t s)
{
    uint32_t i;
    for (i = 0; i < NCELL; i++)
        if (cells[i].amt && cells[i].lt == (uint8_t)(l << 6 | t) && cells[i].src == s)
            return (int32_t)i;
    return -1;
}
static int32_t cell_get(uint32_t l, uint32_t t, uint32_t s)
{
    int32_t i = cell_find(l, t, s);
    return i < 0 ? 0 : cells[i].amt;
}
/* source s -> layer l's target t at amt % (0: no cell). 0: done, -1: no room (NCELL) */
static int cell_set(uint32_t l, uint32_t t, uint32_t s, int32_t amt)
{
    int32_t i = cell_find(l, t, s);
    uint32_t k;
    amt = clamp(amt, -100, 100);
    if (i >= 0) {
        cells[i].amt = (int8_t)amt;                /* (0: gone) */
        return 0;
    }
    if (!amt)
        return 0;
    for (k = 0; k < NCELL && cells[k].amt; k++)
        ;
    if (k == NCELL)
        return -1;
    cells[k].lt = (uint8_t)(l << 6 | t);
    cells[k].src = (uint8_t)s;
    RING_PUBLISH();                                /* (the cell whole before its amount makes it one) */
    cells[k].amt = (int8_t)amt;
    return 0;
}
/* layer l's cells on targets t0 .. t1 gone */
static void cells_drop(uint32_t l, uint32_t t0, uint32_t t1)
{
    uint32_t i;
    for (i = 0; i < NCELL; i++)
        if (cells[i].amt && cell_l(&cells[i]) == l && cell_t_(&cells[i]) >= t0 && cell_t_(&cells[i]) <= t1)
            cells[i].amt = 0;
}

/* layer l takes the engine at ENGINES[idx]: its own routes (EXCITE, its inputs, the knobs) go, the engine's defaults
 * come */
static void routes_engine(uint32_t l, uint32_t idx)
{
    uint32_t i;
    cells_drop(l, T_EXCITE, NTARGET - 1u);
    if (idx >= NENGINES)
        return;
    for (i = 0; i < NELEM(ROUTE_DEFAULTS); i++)
        if (ROUTE_DEFAULTS[i].id == ENGINES[idx].id)
            cell_set(l, ROUTE_DEFAULTS[i].t, SRC_A, ROUTE_DEFAULTS[i].amt);
}
/* layer l's TONE became type: its knobs' routes go, the type's come: FILTER and SWEEP A -> CUTOFF (BR>TONE's), TALK
 * A -> VOWEL (its DEPTH's +40: 80 of 127 at full breath), SUB OCT A -> LEVEL (its BREATH's 40: the rest from LEVEL) */
static void routes_tone(uint32_t l, int32_t type)
{
    cells_drop(l, T_TONE_A, T_TONE_C);
    if (type == TN_FILTER || type == TN_SWEEP)
        cell_set(l, T_TONE_A, SRC_A, ROUTE_BR_TONE);
    else if (type == TN_TALK)
        cell_set(l, T_TONE_A, SRC_A, 63);
    else if (type == TN_SUB)
        cell_set(l, T_TONE_A, SRC_A, 17);
}
/* layer l's knobs as an engine preset (its presets[]) or a factory preset set them: the depths they held with breath
 * (before the routes, 2026-10-30) become A routes, the knobs their rest. EVA's VIB and TREM (they came in with breath),
 * its SHAPE (half its drive with breath), its vibrato's quickening; ERS's VIB; ERS's and FM's BR>LP; TONE TALK's DEPTH,
 * SUB OCT's BREATH */
static void routes_preset(uint32_t l)
{
    int16_t *p = trk[l].p;
    int32_t v;
    switch (p[P_ENGINE]) {
    case ENG_ID_EVA:
        cell_set(l, T_KNOB + 6, SRC_A, p[P_E6] * 100 / 127);
        cell_set(l, T_KNOB + 11, SRC_A, p[P_E11] * 100 / 127);
        cell_set(l, T_KNOB + 12, SRC_A, p[P_E6] || p[P_E11] ? ROUTE_EVA_RATE : 0);
        p[P_E6] = p[P_E11] = 0;
        v = p[P_E7] / 2;
        cell_set(l, T_KNOB + 7, SRC_A, v * 100 / 127);
        p[P_E7] = (int16_t)(p[P_E7] - v);
        break;
    case ENG_ID_ERS:
        cell_set(l, T_KNOB + 8, SRC_A, p[P_E8] * 100 / 127);
        p[P_E8] = 0;
        /* fall through: the breath LP */
    case ENG_ID_FM:
        cell_set(l, T_KNOB + 16, SRC_A, clamp(p[P_E18] * 200 / 127, -100, 100));
        p[P_E18] = 0;
        break;
    }
    if (p[P_TONE] == TN_TALK) {
        cell_set(l, T_TONE_A, SRC_A, clamp(p[P_TONE_B] * 200 / 127, -100, 100));
        p[P_TONE_B] = 0;
    } else if (p[P_TONE] == TN_SUB) {
        v = p[P_TONE_A] * p[P_TONE_B] / 127;       /* (LEVEL x (1 - BREATH + BREATH x breath)) */
        cell_set(l, T_TONE_A, SRC_A, v * 100 / 127);
        p[P_TONE_A] = (int16_t)(p[P_TONE_A] - v);
        p[P_TONE_B] = 0;
    }
}
/* a new layer l on ENGINES[idx]: no routes but BEND -> PITCH and the engine's */
static void routes_layer(uint32_t l, uint32_t idx)
{
    cells_drop(l, 0, NTARGET - 1u);
    cell_set(l, T_PITCH, SRC_BEND, 100);
    routes_engine(l, idx);
}

/* a target's range on layer l (e: its engine); 0: not a knob target */
static int route_range(uint32_t l, const engine_t *e, uint32_t t, uint32_t *pi, int32_t *lo, int32_t *hi)
{
    const param_desc_t *d;
    if (t == T_PAN) {
        *pi = P_PAN, *lo = -64, *hi = 63;
        return 1;
    }
    if (t >= T_TONE_A && t <= T_TONE_C) {
        d = tone_desc(trk[l].p[P_TONE], t - T_TONE_A);
        *pi = P_TONE_A + (t - T_TONE_A);
    } else if (t >= T_KNOB && t < NTARGET) {
        d = &e->edit[t - T_KNOB];
        *pi = P_E0 + (t - T_KNOB);
    } else
        return 0;
    *lo = d->min, *hi = d->max;
    return d->max > d->min;
}

/* what a layer's routes do this control block: its level (Q15, the sources' share), its pitch (1/16 cent), its EXCITE
 * (Q15, bipolar); tone_in: the sources on TONE's first knob, as one (their amounts' weights, Q15: SWEEP's accent rises
 * with them); moved: its knobs moved (orig holds them as they were). Its inputs into layer_in[l] */
typedef struct { int32_t gain, c16, excite, tone_in; uint8_t moved; } route_t;
static void routes_apply(uint32_t l, const engine_t *e, route_t *r, int16_t *orig)
{
    int16_t *p = trk[l].p;
    uint32_t i, t, pi;
    int32_t v, s, lo, hi, in[4] = {0, 0, 0, 0}, w[4] = {0, 0, 0, 0}, tw = 0, ts = 0;
    uint8_t on = 0;
    r->gain = 32767;
    r->c16 = r->excite = 0;
    r->moved = 0;
    for (i = 0; i < NCELL; i++) {
        if (!cells[i].amt || cell_l(&cells[i]) != l)
            continue;
        t = cell_t_(&cells[i]);
        s = src_val[cells[i].src % NSRC];
        v = s * cells[i].amt / 100;                /* Q15, the amount's share of the source */
        if (t == T_TONE_A) {
            ts += v;
            tw += cells[i].amt < 0 ? -cells[i].amt : cells[i].amt;
        }
        switch (t) {
        case T_LEVEL:                              /* (a share of the level: 100 % all of it the source's) */
            s = s < 0 ? 0 : s;
            v = cells[i].amt > 0 ? 32767 - (32767 - s) * cells[i].amt / 100 : 32767 - s * -cells[i].amt / 100;
            r->gain = (int32_t)(mul64(r->gain, clamp(v, 0, 32767)) >> 15);
            break;
        case T_PITCH:
            r->c16 += v * gpar[G_BEND] * 1600 / 32768;   /* (1/16 cent: 32767 x 24 x 1600 fits) */
            break;
        case T_EXCITE:
        case T_IN1:
        case T_IN2:
        case T_IN3:
            in[t - T_EXCITE] += v;
            w[t - T_EXCITE] += cells[i].amt < 0 ? -cells[i].amt : cells[i].amt;
            on |= (uint8_t)(1u << (t - T_EXCITE));
            break;
        default:
            if (!route_range(l, e, t, &pi, &lo, &hi))
                break;
            if (!r->moved) {
                memcpy(orig, p, sizeof trk[l].p);
                r->moved = 1;
            }
            p[pi] = (int16_t)clamp(p[pi] + v * (hi - lo) / 32767, lo, hi);
            break;
        }
    }
    for (i = 0; i < 4u; i++) {
        layer_in[l][i] = clamp(in[i], -32768, 32767);
        layer_in_w[l][i] = (int16_t)w[i];
    }
    layer_in_on[l] = on;
    r->excite = layer_in[l][0];
    r->tone_in = tw ? clamp(ts * 100 / tw, -32768, 32767) : 0;
}
static void routes_restore(uint32_t l, const route_t *r, const int16_t *orig)
{
    if (r->moved)
        memcpy(trk[l].p, orig, sizeof trk[l].p);
}
