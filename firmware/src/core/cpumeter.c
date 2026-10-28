/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* Is the sound keeping up: the render's time a block (app/app.c app_render, timed with plat_ticks24) against the time
 * its frames play, and the xruns (fm1-base counts them in the audio interrupt: the DMA moved on before a block was
 * done, a dropout heard as a click). Shown on GLO > PERFORMANCE (ui/page_perf.c) and sent by STATS (core/sysex.c).
 *   the average   plat_cpu_pct: fm1-base's, over ~16 blocks (the whole interrupt, the render and its own share)
 *   the worst     one block, not averaged (the render alone): a spike the average hides; over the last second, and
 *                 the highest since a reset (KNOB 1 on the page) and since STATS last read it
 *   xruns         since a reset, and how long ago the last one came */

static struct {
    volatile uint32_t sec, peak, stat;             /* the worst block, % x 10: this second, since a reset, since STATS */
    volatile uint32_t n;                           /* a block's frames */
    uint32_t sec_shown, t_sec;                     /* the last full second's worst; when this one began */
    uint32_t x_seen, x0, t_xrun;                   /* plat_xruns at the last poll, at the reset; the last one's time */
} cpum;

/* the audio interrupt: a block of n frames took us microseconds */
static inline void cpum_block(uint32_t us, uint32_t n)
{
    uint32_t pm = n ? us * 10000u / (n * 1000000u / FS) : 0;   /* % x 10 of the block's time */
    cpum.n = n;
    if (pm > cpum.sec) cpum.sec = pm;
    if (pm > cpum.peak) cpum.peak = pm;
    if (pm > cpum.stat) cpum.stat = pm;
}

static void cpum_reset(void)                       /* KNOB 1 on the page */
{
    cpum.peak = 0;
    cpum.x0 = cpum.x_seen = plat_xruns();
    cpum.t_xrun = 0;
}

static void cpum_poll(void)                        /* the main loop */
{
    uint32_t x = plat_xruns();
    if (x != cpum.x_seen) {                        /* (fm1_ms | 1: 0 is "none") */
        cpum.x_seen = x;
        cpum.t_xrun = fm1_ms | 1u;
    }
    if (fm1_ms - cpum.t_sec >= 1000u) {
        cpum.t_sec = fm1_ms;
        cpum.sec_shown = cpum.sec;
        cpum.sec = 0;
    }
}
