/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* Is the sound keeping up: the render's time a block (app/app.c app_render, timed with plat_ticks24) against the time
 * its frames play, and the xruns (fm1-base counts them in the audio interrupt: the DMA moved on before a block was
 * done, a dropout heard as a click). Shown on GLO > PERFORMANCE (ui/page_perf.c) and sent by STATS (core/sysex.c).
 *   the average   plat_cpu_pct: fm1-base's, over ~16 blocks (the whole interrupt, the render and its own share)
 *   the worst     one block, not averaged (the render alone): a spike the average hides; over the last second, and
 *                 the highest since a reset (KNOB 1 on the page) and since STATS last read it
 *   xruns         since a reset, and how long ago the last one came
 *   core 2        the second core's time a block on typhoon's jobs (every one goes through cpu2_job, timed on core 2
 *                 itself: TIMER4 is one counter both read), averaged as fm1-base averages core 1, and its worst a
 *                 second. While no job runs there it says why: not in this build (fm1.toml cpu2), or not started */

#if defined(FM1B_CPU2) && FM1B_CPU2
#define CPUM_CPU2 1                                /* the second core is in this build (fm1.toml cpu2 = true) */
#else
#define CPUM_CPU2 0
#endif

static struct {
    volatile uint32_t sec, peak, stat;             /* the worst block, % x 10: this second, since a reset, since STATS */
    volatile uint32_t n;                           /* a block's frames */
    uint32_t sec_shown, t_sec;                     /* the last full second's worst; when this one began */
    uint32_t x_seen, x0, t_xrun;                   /* plat_xruns at the last poll, at the reset; the last one's time */
    volatile uint32_t c2_us;                       /* core 2: its job time in this block (us), added on core 2 */
    volatile uint32_t c2_q12, c2_sec, c2_jobs;     /* its load (1/4096, over ~16 blocks), its worst this second (% x
                                                    * 10), the jobs it has taken */
    uint32_t c2_shown;                             /* the last full second's worst */
} cpum;

/* the audio interrupt: a block of n frames took us microseconds */
static inline void cpum_block(uint32_t us, uint32_t n)
{
    uint32_t budget = n * 1000000u / FS, pm = n ? us * 1000u / budget : 0;   /* % x 10 of the block's time (x 10000
                                                                * until 0.3.26: every worst read 10 x too high) */
    cpum.n = n;
    if (pm > cpum.sec) cpum.sec = pm;
    if (pm > cpum.peak) cpum.peak = pm;
    if (pm > cpum.stat) cpum.stat = pm;
    if (cpum.c2_jobs && n) {                       /* core 2's share of this block (its jobs are done: the render waited) */
        uint32_t c2 = cpum.c2_us, p2 = c2 * 1000u / budget;
        cpum.c2_us = 0;
        cpum.c2_q12 = (cpum.c2_q12 * 15u + c2 * 4096u / budget + 8u) / 16u;   /* (fm1-base's average for core 1) */
        if (p2 > cpum.c2_sec) cpum.c2_sec = p2;
    }
}

/* ---- the second core: the one way typhoon gives it a job (plat_cpu2_run), so its time is counted. 1: core 2 has
 * fn(arg), wait with plat_cpu2_wait as fm1-base says; 0: do it yourself (no core 2 in this build, or not started) */
#if CPUM_CPU2
static void (*volatile cpu2_fn)(uint32_t);
static void cpu2_timed(uint32_t arg)               /* on core 2 */
{
    uint32_t t0 = plat_ticks24();
    cpu2_fn(arg);
    cpum.c2_us += (plat_ticks24() - t0) / PLAT_TICKS_PER_US;
}
#endif
static int cpu2_job(void (*fn)(uint32_t), uint32_t arg)
{
#if CPUM_CPU2
    if (plat_cpu2_on()) {
        cpu2_fn = fn;
        if (plat_cpu2_run(cpu2_timed, arg)) {
            cpum.c2_jobs++;
            return 1;
        }
    }
#endif
    (void)fn;
    (void)arg;
    return 0;
}

/* core 2's state for the page: 0 not in this build, 1 not started (or given up), 2 started, no job yet, 3 counting */
static int cpum_c2_state(void)
{
#if CPUM_CPU2
    if (!plat_cpu2_on()) return 1;
    return cpum.c2_jobs ? 3 : 2;
#else
    return 0;
#endif
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
        cpum.c2_shown = cpum.c2_sec;
        cpum.c2_sec = 0;
    }
}
