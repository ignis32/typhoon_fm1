/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* Is the sound keeping up: the render's time a block (app/app.c app_render, timed with plat_ticks24) against the time
 * its frames play, and the xruns (fm1-base counts them in the audio interrupt: the DMA moved on before a block was
 * done, a dropout heard as a click). Shown on GLO > PERFORMANCE (ui/page_perf.c) and sent by STATS (core/sysex.c).
 *   core 1        its own work a block: the render less its waits for core 2 (cpu2_done); averaged over ~16
 *                 blocks, its worst block over the last second and since a reset (KNOB 1 on the page)
 *   the block     the whole render, waits in it: what must fit the block's time (past 100 %: an xrun); its worst
 *                 over the last second, since a reset and since STATS last read it (STATS also sends plat_cpu_pct,
 *                 fm1-base's average of the whole interrupt)
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
    volatile uint32_t sec, peak, stat;             /* the whole block (core 1's render, its waits for core 2 in it), %
                                                    * x 10, the worst: this second, since a reset, since STATS. Past
                                                    * 100 %: an xrun */
    volatile uint32_t n;                           /* a block's frames */
    uint32_t sec_shown, t_sec;                     /* the last full second's worst; when this one began */
    uint32_t x_seen, x0, t_xrun;                   /* plat_xruns at the last poll, at the reset; the last one's time */
    volatile uint32_t wait_us;                     /* core 1: its waits for core 2 in this block (cpu2_done), us */
    volatile uint32_t c1_q12, c1_sec, c1_peak;     /* core 1's own work (the block less its waits): its load (1/4096,
                                                    * over ~16 blocks), its worst this second, since a reset (% x 10) */
    uint32_t c1_shown;                             /* the last full second's worst */
    volatile uint32_t c2_us;                       /* core 2: its job time in this block (us), added on core 2 */
    volatile uint32_t c2_q12, c2_sec, c2_peak, c2_jobs;   /* its load (1/4096, over ~16 blocks), its worst this second
                                                    * and since a reset (% x 10), the jobs it has taken */
    uint32_t c2_shown;                             /* the last full second's worst */
} cpum;

/* the audio interrupt: a block of n frames took us microseconds (core 1's waits for core 2 in it) */
static inline void cpum_block(uint32_t us, uint32_t n)
{
    uint32_t budget = n * 1000000u / FS, pm = n ? us * 1000u / budget : 0;   /* % x 10 of the block's time (x 10000
                                                                * until 0.3.26: every worst read 10 x too high) */
    uint32_t own = us > cpum.wait_us ? us - cpum.wait_us : 0, p1;
    cpum.wait_us = 0;
    cpum.n = n;
    if (!n)
        return;
    if (pm > cpum.sec) cpum.sec = pm;
    if (pm > cpum.peak) cpum.peak = pm;
    if (pm > cpum.stat) cpum.stat = pm;
    p1 = own * 1000u / budget;                     /* core 1's own work */
    cpum.c1_q12 = (cpum.c1_q12 * 15u + own * 4096u / budget + 8u) / 16u;   /* (fm1-base's average) */
    if (p1 > cpum.c1_sec) cpum.c1_sec = p1;
    if (p1 > cpum.c1_peak) cpum.c1_peak = p1;
    if (cpum.c2_jobs) {                            /* core 2's share of this block (its jobs are done: the render waited) */
        uint32_t c2 = cpum.c2_us, p2 = c2 * 1000u / budget;
        cpum.c2_us = 0;
        cpum.c2_q12 = (cpum.c2_q12 * 15u + c2 * 4096u / budget + 8u) / 16u;
        if (p2 > cpum.c2_sec) cpum.c2_sec = p2;
        if (p2 > cpum.c2_peak) cpum.c2_peak = p2;
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
/* after cpu2_job said 1: wait for the job; when core 2 was given up meanwhile and the job dropped (cpu2_fallback),
 * fn(arg) here */
static void cpu2_done(void (*fn)(uint32_t), uint32_t arg)
{
#if CPUM_CPU2
    uint32_t t0 = plat_ticks24();
    int r = plat_cpu2_wait();
    cpum.wait_us += (plat_ticks24() - t0) / PLAT_TICKS_PER_US;   /* (not core 1's own work: cpum_block) */
    if (r == 0)
        return;
#endif
    fn(arg);
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
    cpum.peak = cpum.c1_peak = cpum.c2_peak = 0;
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
        cpum.c1_shown = cpum.c1_sec;
        cpum.c1_sec = 0;
        cpum.c2_shown = cpum.c2_sec;
        cpum.c2_sec = 0;
    }
}
