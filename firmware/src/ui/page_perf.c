/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The PERFORMANCE page (UK_PERF, GLO after USB HOST; core/cpumeter.c): is the sound keeping up. Two columns, CORE 1
 * and CORE 2, each a load as % of the block's time:
 *   AVERAGE      over ~16 blocks (90 ms)
 *   WORST 1 S    the slowest single block in the last second: what the average hides
 *   WORST        the slowest since the reset
 * CORE 1 is its own work (the render less its waits for core 2); CORE 2 its jobs (PANFLUTE), or why none: NONE (not
 * in this build: fm1.toml cpu2), OFF (not started, or given up after a fault: one core until power-off), IDLE (no job
 * yet). Then:
 *   BLOCK, 1 S   the whole render in the last second's slowest block, core 1's waits for core 2 in it: what must fit
 *                the block's time (past 100 %: an xrun)
 *   XRUNS        blocks not done in time since the reset (a dropout: a click), and when the last one came
 * Red from 90 %. KNOB 1 (a turn) resets the worst and the xruns. Redrawn ~5 times a second. Part of ui/ui.c
 * (included there, with its drawing helpers). */

#define PERF_RED RGB(232, 76, 60)                  /* a load from 90 % on, an xrun: the red palette's */
#define PERF_HOT 900u                              /* % x 10: red from here (fm1-base sheds from 90 % too: APP_OVERLOAD) */
#define PERF_C1 156                                /* the CORE 1 column's right edge */
#define PERF_C2 234                                /* the CORE 2 column's */
#define PERF_BAR 64                                /* a column's bar, px */

static void perf_pct(char *s, uint32_t pm)         /* "37.5 %" */
{
    char *e = put_int(s, (int32_t)(pm / 10u), 0);
    *e++ = '.';
    e = put_int(e, (int32_t)(pm % 10u), 0);
    put_str(e, " %");
}

/* one load at a column's right edge (a text instead when txt), a bar under it when bar */
static void perf_cell(int y, int right, uint32_t pm, const char *txt, int bar)
{
    char v[16];
    const uint16_t c = !txt && pm >= PERF_HOT ? PERF_RED : txt ? C_DIM : C_AMB;
    if (txt)
        put_str(v, txt);
    else
        perf_pct(v, pm);
    cv_text(right - text_w(&FONT_S, v), y, &FONT_S, v, c);
    if (bar && !txt) {
        int w = (int)(pm > 1000u ? 1000u : pm) * PERF_BAR / 1000;
        cv_rect(right - PERF_BAR, y + 17, PERF_BAR, 3, C_LINE);
        cv_rect(right - PERF_BAR, y + 17, w, 3, c);
    }
}

static void draw_perf(void)
{
    static const char *const C2_WHY[3] = {"NONE", "OFF", "IDLE"};   /* (cpum_c2_state 0, 1, 2) */
    char s[40], *e;
    const uint32_t xr = plat_xruns() - cpum.x0;
    const int c2s = cpum_c2_state();
    const char *why = c2s < 3 ? C2_WHY[c2s] : 0;
    TP_BOX(240, 182, C_BLACK) {
    cv_text(PERF_C1 - text_w(&FONT_S, "CORE 1"), 2, &FONT_S, "CORE 1", C_GRAY);
    cv_text(PERF_C2 - text_w(&FONT_S, "CORE 2"), 2, &FONT_S, "CORE 2", C_GRAY);
    cv_text(6, 22, &FONT_S, "AVERAGE", C_GRAY);
    perf_cell(22, PERF_C1, (cpum.c1_q12 * 1000u + 2048u) >> 12, 0, 1);
    perf_cell(22, PERF_C2, (cpum.c2_q12 * 1000u + 2048u) >> 12, why, 1);
    cv_text(6, 46, &FONT_S, "WORST 1 S", C_GRAY);
    perf_cell(46, PERF_C1, cpum.c1_shown, 0, 1);
    perf_cell(46, PERF_C2, cpum.c2_shown, why ? "-" : 0, 1);
    cv_text(6, 70, &FONT_S, "WORST", C_GRAY);
    perf_cell(70, PERF_C1, cpum.c1_peak, 0, 0);
    perf_cell(70, PERF_C2, cpum.c2_peak, why ? "-" : 0, 0);
    cv_rect(0, 90, 240, 1, C_LINE);
    cv_text(6, 96, &FONT_S, "BLOCK, 1 S", C_GRAY);  /* the whole render, waits in it: the deadline */
    perf_cell(96, PERF_C2, cpum.sec_shown, 0, 0);
    cv_text(6, 114, &FONT_S, "XRUNS", C_GRAY);
    if (!xr)
        put_str(s, "0");
    else if (!cpum.t_xrun)
        put_int(s, (int32_t)xr, 0);
    else {                                         /* "3, 12 S AGO" */
        uint32_t ago = ms_since(cpum.t_xrun) / 1000u;
        e = put_int(s, (int32_t)xr, 0);
        put_str(e, ", ");
        e = put_int(s + par_strlen(s), (int32_t)(ago < 60u ? ago : ago / 60u), 0);
        put_str(e, ago < 60u ? " S AGO" : " MIN AGO");
    }
    cv_text(PERF_C2 - text_w(&FONT_S, s), 114, &FONT_S, s, xr ? PERF_RED : C_AMB);
    cv_rect(0, 140, 240, 1, C_LINE);
    cv_text(6, 146, &FONT_S, "1", C_DIM);
    cv_text(22, 146, &FONT_S, "TURN: RESET WORST, XRUNS", C_GRAY);
    put_str(s, "A BLOCK: ");                       /* "A BLOCK: 256 FRAMES, 5.8 MS" */
    e = put_int(s + par_strlen(s), (int32_t)cpum.n, 0);
    put_str(e, " FRAMES, ");
    e = put_int(s + par_strlen(s), (int32_t)(cpum.n * 10000u / FS) / 10, 0);
    *e++ = '.';
    e = put_int(e, (int32_t)(cpum.n * 10000u / FS) % 10, 0);
    put_str(e, " MS");
    cv_text(6, 164, &FONT_S, s, C_DIM);
    cv_blit(0, 58 + tp_box.top);
    }
}
