/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The PERFORMANCE page (UK_PERF, GLO after USB HOST; core/cpumeter.c): is the sound keeping up.
 *   CPU AVERAGE    fm1-base's load, over ~16 blocks (90 ms)
 *   WORST, 1 S     the slowest single block in the last second (the render alone): what the average hides
 *   WORST          the slowest since the reset
 *   XRUNS          blocks not done in time since the reset (a dropout: a click), and when the last one came
 * Each load as % of the block's time with a bar, red from 90 % (past 100 %: an xrun). KNOB 1 (a turn) resets the
 * worst and the xruns. Redrawn ~5 times a second. Part of ui/ui.c (included there, with its drawing helpers). */

#define PERF_RED RGB(232, 76, 60)                  /* a load from 90 % on, an xrun: the red palette's */
#define PERF_HOT 900u                              /* % x 10: red from here (fm1-base sheds from 90 % too: APP_OVERLOAD) */

static void perf_row(int y, const char *label, uint32_t pm, int bar)   /* "CPU AVERAGE ...... 37.5 %" + a bar */
{
    char v[16], *e;
    const uint16_t c = pm >= PERF_HOT ? PERF_RED : C_AMB;
    e = put_int(v, (int32_t)(pm / 10u), 0);
    *e++ = '.';
    e = put_int(e, (int32_t)(pm % 10u), 0);
    put_str(e, " %");
    cv_text(6, y, &FONT_S, label, C_GRAY);
    cv_text(234 - text_w(&FONT_S, v), y, &FONT_S, v, c);
    if (bar) {
        int w = (int)(pm > 1000u ? 1000u : pm) * 228 / 1000;
        cv_rect(6, y + 17, 228, 3, C_LINE);
        cv_rect(6, y + 17, w, 3, c);
    }
}

static void draw_perf(void)
{
    char s[40], *e;
    const uint32_t xr = plat_xruns() - cpum.x0;
    TP_BOX(240, 182, C_BLACK) {
    perf_row(2, "CPU AVERAGE", plat_cpu_pct() * 10u, 1);
    perf_row(30, "WORST BLOCK, 1 S", cpum.sec_shown, 1);
    perf_row(58, "WORST BLOCK", cpum.peak, 0);
    cv_text(6, 82, &FONT_S, "XRUNS", C_GRAY);
    put_int(s, (int32_t)xr, 0);
    cv_text(234 - text_w(&FONT_S, s), 82, &FONT_S, s, xr ? PERF_RED : C_AMB);
    cv_text(6, 102, &FONT_S, "LAST XRUN", C_GRAY);
    if (!cpum.t_xrun)
        put_str(s, "NONE");
    else {
        uint32_t ago = ms_since(cpum.t_xrun) / 1000u;
        e = put_int(s, (int32_t)(ago < 60u ? ago : ago / 60u), 0);
        put_str(e, ago < 60u ? " S AGO" : " MIN AGO");
    }
    cv_text(234 - text_w(&FONT_S, s), 102, &FONT_S, s, cpum.t_xrun && ms_since(cpum.t_xrun) < 2000u ? PERF_RED : C_AMB);
    cv_rect(0, 126, 240, 1, C_LINE);
    cv_text(6, 134, &FONT_S, "1", C_DIM);
    cv_text(22, 134, &FONT_S, "TURN: RESET WORST, XRUNS", C_GRAY);
    put_str(s, "A BLOCK: ");                       /* "A BLOCK: 256 FRAMES, 5.8 MS" */
    e = put_int(s + par_strlen(s), (int32_t)cpum.n, 0);
    put_str(e, " FRAMES, ");
    e = put_int(s + par_strlen(s), (int32_t)(cpum.n * 10000u / FS) / 10, 0);
    *e++ = '.';
    e = put_int(e, (int32_t)(cpum.n * 10000u / FS) % 10, 0);
    put_str(e, " MS");
    cv_text(6, 156, &FONT_S, s, C_DIM);
    cv_blit(0, 58 + tp_box.top);
    }
}
