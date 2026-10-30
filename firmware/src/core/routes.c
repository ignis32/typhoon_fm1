/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The routes (notes/05-controls.md): what the sources (BEND, A .. D, AO .. DO: core/layers.h, set in GLO > CONTROLS) do
 * to a layer. A cell is a source -> a target of a layer, an amount -100 .. +100 %; the preset has up to NCELL of them,
 * in cells[] (layers.h), saved with it (core/presets.c). One source to many targets, many to one (they add).
 * No engine reads a source: what moves an engine is a cell, on HOME's grid (ui/ui.c).
 *
 * The targets (layers.h T_*): the layer's level (the source's share of it), its pitch, PAN, TONE's knobs, the engine's
 * EXCITE (vmod.shape: what breath moved), and every engine knob its descriptor marks MOD. A knob target is the knob
 * plus what its cells add (100 % at a full source: the knob's whole range), clamped to its range: the knob is the
 * resting value. routes_apply moves the knobs in place for one control block, routes_restore puts them back: the
 * audio ISR runs whole, so the main loop (the UI, storage) never sees a moved value.
 *
 * Defaults (routes_layer, routes_engine, routes_tone): every layer BEND -> PITCH 100 %; an engine its ENGINES[]
 * routes (engines.h: A -> EXCITE, A -> the knob the layer's BR>TONE moved); a TONE FILTER or SWEEP A -> its CUTOFF
 * 31 %. So the presets sound as they did with BR>AMP / BR>TONE / BR>SHP (until 2026-10-30). */

#define ROUTE_BR_TONE 31                           /* %: BR>TONE's +40 (its default, every layer's) moved a cutoff 40 of
                                                    * 127 at full breath: 31 % of the knob's range */

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

/* layer l takes the engine at ENGINES[idx]: its own routes (EXCITE, the knobs) go, the engine's defaults come */
static void routes_engine(uint32_t l, uint32_t idx)
{
    cells_drop(l, T_EXCITE, NTARGET - 1u);
    if (idx >= NENGINES)
        return;
    if (ENGINES[idx].br_shp)
        cell_set(l, T_EXCITE, SRC_A, ENGINES[idx].br_shp);
    if (ENGINES[idx].br_knob >= 0)
        cell_set(l, T_KNOB + (uint32_t)ENGINES[idx].br_knob, SRC_A, ROUTE_BR_TONE);
}
/* layer l's TONE became type: its knobs' routes go; FILTER and SWEEP take A -> CUTOFF (BR>TONE's) */
static void routes_tone(uint32_t l, int32_t type)
{
    cells_drop(l, T_TONE_A, T_TONE_C);
    if (type == TN_FILTER || type == TN_SWEEP)
        cell_set(l, T_TONE_A, SRC_A, ROUTE_BR_TONE);
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
 * (Q15, bipolar); moved: its knobs moved (orig holds them as they were) */
typedef struct { int32_t gain, c16, excite; uint8_t moved; } route_t;
static void routes_apply(uint32_t l, const engine_t *e, route_t *r, int16_t *orig)
{
    int16_t *p = trk[l].p;
    uint32_t i, t, pi;
    int32_t v, s, lo, hi;
    r->gain = 32767;
    r->c16 = r->excite = 0;
    r->moved = 0;
    for (i = 0; i < NCELL; i++) {
        if (!cells[i].amt || cell_l(&cells[i]) != l)
            continue;
        t = cell_t_(&cells[i]);
        s = src_val[cells[i].src % NSRC];
        v = s * cells[i].amt / 100;                /* Q15, the amount's share of the source */
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
            r->excite += v;
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
    r->excite = clamp(r->excite, -32768, 32767);
}
static void routes_restore(uint32_t l, const route_t *r, const int16_t *orig)
{
    if (r->moved)
        memcpy(trk[l].p, orig, sizeof trk[l].p);
}
