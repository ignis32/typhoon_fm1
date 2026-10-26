/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* LEAPS: passing notes on slurs held back. When a fingering changes by more than one finger the
 * fingers never land together, and the re.corder reports the fingering in between as a note for 1 to 3 Bluetooth
 * packets (11 .. 37 ms). Going down from the half-thumb register its thumb reads open for a moment, and that note is
 * two octaves up (D6 -> A7 -> A5, D6 -> G7 -> G5): a squeak, in every engine.
 *
 * HOLD (G_LEAP_MODE): a slurred note-on at least G_LEAP_ST semitones above the note sounding waits up to G_LEAP_MS; the
 * note sounding goes on meanwhile.
 *   - a note-on within the wait: the jump was passing. It never plays, nor later its note-off; the new note is judged
 *     afresh (it plays at once, or waits itself if it is a big jump too)
 *   - its own note-off within the wait: passing too (the player stopped)
 *   - nothing within the wait: it plays, G_LEAP_MS late
 * Note-offs that come during the wait wait with it: a slur sends the new note-on before the old note-off, and passed on
 * at once the old note-off would break the slur into a gap and a fresh attack.
 * Only upward, only slurs: steps, downward jumps, a fresh (tongued) note and everything that is not a note pass at once.
 * MIDI in only (the FM-1's own keys do not go through it). Main loop. */

enum { LEAP_OFF, LEAP_HOLD, LEAP_NMODE };          /* (DUCK to come) */
#define LEAP_NOFFS 8u

static struct {
    int32_t cur;                                   /* the note passed on that sounds (-1: none) */
    int32_t held, vel;                             /* the jump waiting (-1: none), its velocity */
    uint32_t t_held;                               /* when it came (fm1_ms) */
    uint8_t offs[LEAP_NOFFS];                      /* note-offs waiting with it */
    uint32_t noffs;
    uint32_t gone[4];                              /* bit n: note n's note-off is swallowed (a jump that never played) */
} lp = {-1, -1, 0, 0, {0}, 0, {0}};

static void leap_offs(void)                        /* the note-offs that waited, now */
{
    uint32_t i;
    for (i = 0; i < lp.noffs; i++) {
        perf_note_off(lp.offs[i]);
        if ((int32_t)lp.offs[i] == lp.cur)
            lp.cur = -1;
    }
    lp.noffs = 0;
}

static void leap_poll(void)                        /* the wait over, nothing came: the jump plays (late) */
{
    if (lp.held < 0 || fm1_ms - lp.t_held < (uint32_t)gpar[G_LEAP_MS])
        return;
    perf_note_on((uint32_t)lp.held, (uint32_t)lp.vel);
    lp.cur = lp.held;
    lp.held = -1;
    leap_offs();
}

static void leap_note_off(uint32_t n)
{
    leap_poll();
    if ((lp.gone[n >> 5] >> (n & 31u) & 1u) && lp.held != (int32_t)n) {
        lp.gone[n >> 5] &= ~(1u << (n & 31u));     /* (a passing jump's, never played) */
        return;
    }
    if (lp.held == (int32_t)n) {                   /* the jump's own, while it waits: passing, and the player stopped */
        lp.held = -1;
        leap_offs();
        return;
    }
    if (lp.held >= 0) {
        if (lp.noffs < LEAP_NOFFS) {
            lp.offs[lp.noffs++] = (uint8_t)n;
            return;
        }
    }
    perf_note_off(n);
    if ((int32_t)n == lp.cur)
        lp.cur = -1;
}

static void leap_note_on(uint32_t n, uint32_t vel)
{
    if (!vel) {
        leap_note_off(n);
        return;
    }
    leap_poll();
    lp.gone[n >> 5] &= ~(1u << (n & 31u));         /* (played again: its next note-off is its own) */
    if (lp.held >= 0) {                            /* a note within the wait: the jump was passing */
        lp.gone[lp.held >> 5] |= 1u << (lp.held & 31);
        lp.held = -1;
    }
    if (gpar[G_LEAP_MODE] == LEAP_HOLD && gpar[G_LEAP_MS] > 0 && lp.cur >= 0 &&
        (int32_t)n - lp.cur >= gpar[G_LEAP_ST]) {
        lp.held = (int32_t)n;
        lp.vel = (int32_t)vel;
        lp.t_held = fm1_ms;
        return;
    }
    perf_note_on(n, vel);
    lp.cur = (int32_t)n;
    leap_offs();
}

static void leap_reset(void)                       /* all notes off: nothing waits, nothing is swallowed */
{
    lp.cur = lp.held = -1;
    lp.noffs = 0;
    memset(lp.gone, 0, sizeof lp.gone);
}
