/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The reverbs (core/fx.c fx_block calls fx_reverb last but one, before the VOLUME): S_REV_TYPE picks one; both
 * mono in, stereo out; SIZE, DECAY, DAMP, PRE-DLY, MIX.
 *   PLATE  Dattorro's plate (his Effect Design, Part 1, 1997): core/plate.c
 *   HALL   a room, early reflections then an 8-line feedback delay network: core/hall.c
 *   the delay lines both use: core/rvline.h
 *   DECAY is the time to fall 60 dB: HALL 0.4 s x 2^(6 DECAY / 127), ~4 s at 70; PLATE half that (rv_e_decay)
 * The memories live in the pool (.pool: zeroed at boot); turning the reverb on (MIX from 0) or another TYPE clears
 * them first, so an old tail never comes back; PLATE and HALL share one (rv_mem). Uses fx.c's sat16 / mulz.
 *
 * The contract a TYPE keeps (fx_reverb calls it): a layout, xx_layout(), run with the memory cleared, taking its lines
 * from rv_mem in order (rv_at: what is taken; at most RV_MEM samples, for the largest SIZE and PRE-DLY), and a block,
 * xx_block(l, r, mixq, pd): CTL frames of the mix in l[] and r[] (Q15 scale, +-32767 nominal), the reverb fed
 * their mono sum, its two sides added into l[] and r[] at mixq (Q15: MIX x 258), after pd samples of PRE-DLY
 * (0 .. RV_PRE_MAX); SIZE, DECAY (rv_e_decay), DAMP read with sv() each block. */

/* (OLD, the first one, 4 combs into 2 allpasses as Felucca's fx.c, was dropped 2026-10-23: a saved OLD plays HALL) */
enum { RT_PLATE, RT_HALL };
#define RV_MEM 50400u                              /* samples: PLATE needs 50033 (the largest SIZE, 200 ms PRE-DLY) */
static int16_t rv_mem[RV_MEM] __attribute__((section(".pool")));
#define RV_PRE_MAX 8820u                           /* PRE-DLY up to 200 ms */
#define RV_EXC 24u                                 /* the modulated lines' sweep, samples */
static uint32_t rv_at;

/* ---- the parts */
static inline int32_t mulz64(int32_t a, int32_t b)   /* mulz for a past 16 bits */
{
    int64_t q = mul64(a, b);
    return (int32_t)(q >= 0 ? q >> 15 : -((-q) >> 15));
}
#include "rvline.h"

static struct {
    uint8_t on, type, ok;                          /* running; the type the memory is laid out for; it fits */
    rvl_t pre, dif[4], tank[8];                    /* the predelay (+ HALL's reflections), diffusers, the tank lines */
    uint32_t base[8];                              /* HALL: the tank's delays at SIZE 1 */
    int32_t bw, damp[8];                           /* filter states: the input LP, the lines' damping */
    uint32_t lfo, lfo2;
    int32_t scale;                                 /* SIZE as it is now (Q12), gliding to the knob's */
    int32_t dec[8];                                /* each line's loss a pass (Q15; PLATE: each half's two stretches) */
} rv;

/* 2^-x, x in Q16 (>= 0) -> Q15: the whole part a shift, the rest a cubic (exact at 0 and 1, ~0.2 % between) */
static int32_t exp2_neg_q15(uint32_t x16)
{
    uint32_t ip = x16 >> 16;
    int64_t f = x16 & 0xFFFFu, t;
    if (ip >= 15u)
        return 0;
    t = 3085;                                      /* 0.04708 */
    t = 15744 - ((t * f) >> 16);                   /* 0.240227 */
    t = 45426 - ((t * f) >> 16);                   /* 0.693147 */
    t = 65536 - ((t * f) >> 16);
    return (int32_t)(t >> 1) >> ip;
}
/* 2^(-6 DECAY / 127), Q15: 0.4 s / RT60 (HALL). PLATE takes half that time, 0.2 s x 2^(6 DECAY / 127), as the old
 * PLATE rang (2.1 s at DECAY 70) and the factory presets were voiced on it; its e twice that,
 * past Q15's 1 (plate.c only multiplies by it) */
static uint32_t rv_e_decay(void)
{
    const uint32_t e = (uint32_t)exp2_neg_q15((uint32_t)sv(S_REV_DECAY) * 3096u);
    return rv.type == RT_PLATE ? 2u * e : e;
}


#include "hall.c"
#include "plate.c"

static void fx_reverb(int32_t *l, int32_t *r)
{
    int32_t mixq = Q15_127(sv(S_REV_MIX));
    uint32_t type = sv(S_REV_TYPE) == RT_PLATE ? RT_PLATE : RT_HALL, pd = (uint32_t)sv(S_REV_PRE) * 441u / 10u;   /* (a
                                                    * record from before 2026-10-23 with OLD, 2: HALL) */
    if (mixq <= 0) {
        rv.on = 0;
        return;
    }
    if (!rv.on || type != rv.type) {               /* on again, or another type: all of it cleared, laid out anew */
        memset(rv_mem, 0, sizeof rv_mem);
        memset(rv.damp, 0, sizeof rv.damp);
        rv.bw = 0;
        rv.type = (uint8_t)type;
        rv_at = 0;
        if (type == RT_HALL)
            hl_layout();
        else
            pl_layout();
        rv.ok = rv_at <= RV_MEM;
        rv.scale = 2048 + sv(S_REV_SIZE) * (type == RT_HALL ? 32 : 24);   /* (no glide into the first one) */
        rv.on = 1;
    }
    if (pd > RV_PRE_MAX)
        pd = RV_PRE_MAX;
    if (rv.ok && type == RT_HALL)
        hl_block(l, r, mixq, pd);
    else if (rv.ok)
        pl_block(l, r, mixq, pd);
}