/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* SAVE AS (ui/layout.h UA_SAVE_AS, a hold of SAVE): the name typed on the keys, edited, saved; its screen (the name,
 * the cursor, the controls) and the keys' legend. Part of ui/ui.c (included there: its ui state and questions). */

static const char NM_SET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-./";   /* what ALGORITHM turns through */

static int ui_typing(void) { return ui.ask == ASK_SAVE_AS; }   /* the keys type (app/app.c app_loop): no notes */

static void nm_changed(void)
{
    ui.nm_n++;
    ui.hdr++;
}

static void ui_type(uint32_t key)                  /* key 0 .. 25: A .. Z, 26: a space; at the cursor, inserted */
{
    uint32_t n = par_strlen(ui.nm), i;
    char c = key < 26u ? (char)('A' + key) : ' ';
    if (n >= PS_NAME || ui.nm_cur > n)
        return;
    for (i = n + 1u; i > ui.nm_cur; i--)
        ui.nm[i] = ui.nm[i - 1u];
    ui.nm[ui.nm_cur++] = c;
    nm_changed();
}

static void nm_delete(void)                        /* the character before the cursor */
{
    uint32_t i;
    if (!ui.nm_cur)
        return;
    for (i = --ui.nm_cur; ui.nm[i]; i++)
        ui.nm[i] = ui.nm[i + 1u];
    nm_changed();
}

static void nm_turn(int32_t dir)                   /* the character at the cursor (at the end: a new one) */
{
    uint32_t n = par_strlen(ui.nm), k = 0, len = sizeof NM_SET - 1u;
    if (ui.nm_cur >= n) {
        if (n >= PS_NAME)
            return;
        ui.nm[n] = ' ';
        ui.nm[n + 1u] = 0;
    }
    while (k < len && NM_SET[k] != ui.nm[ui.nm_cur])
        k++;
    ui.nm[ui.nm_cur] = NM_SET[(k + len + (uint32_t)(dir % (int32_t)len + (int32_t)len)) % len];
    nm_changed();
}

static void nm_save(void)                          /* OCT+: the name (no trailing spaces; none: "PRESET nn"), saved */
{
    uint32_t n = par_strlen(ui.nm);
    while (n && ui.nm[n - 1u] == ' ')
        ui.nm[--n] = 0;
    if (n)
        name_set(ps.name, ui.nm, PS_NAME);
    else
        name_set(ps.name, "INIT", PS_NAME);       /* (ps_save names it after the slot) */
    ui_msg(ps_save(ui.save_slot) ? "SAVE FAILED" : "SAVED");
}

/* the name, the cursor, and what the controls do */
static void draw_name(void)
{
    static const char *const HELP[] = {"KEYS: THE LETTERS BELOW", "PLAY DELETE  SELECT CURSOR",
                                       "ALGO 0-9 - . /  PRESETS SLOT"};   /* (the canvas: <= 124 rows) */
    char pre[PS_NAME + 1];                         /* (FONT_S: FONT_L would link ~26 KB for this one screen) */
    int32_t x;
    uint32_t i;
    name_set(pre, ui.nm, ui.nm_cur);
    x = 10 + text_w(&FONT_S, pre);
    TP_BOX(240, 112, C_BLACK) {
    cv_rect(0, 6, 240, 30, C_LINE);
    cv_text(10, 12, &FONT_S, ui.nm, C_WHITE);
    cv_rect(x, 9, 3, 24, C_GLOBAL);                /* the cursor */
    for (i = 0; i < NELEM(HELP); i++)
        cv_text(6, 46 + (int32_t)i * 22, &FONT_S, HELP[i], C_GRAY);
    cv_blit(0, 58 + tp_box.top);
    }
}

/* which key types which letter, the keys as they are (F3 .. G5: 16 white, 11 black), the held ones amber */
static void draw_legend(void)
{
    static const uint32_t BLACK = 1u << 1 | 1u << 3 | 1u << 5 | 1u << 8 | 1u << 10 | 1u << 13 | 1u << 15 | 1u << 17 |
                                  1u << 20 | 1u << 22 | 1u << 25;
    uint32_t k, w, held = tp_in.notes;
    char c[2] = {0, 0};
    TP_BOX(240, 64, C_BLACK) {
    for (k = 0, w = 0; k < 27u; k++) {             /* the white keys, 15 px each */
        if (BLACK >> k & 1u)
            continue;
        cv_rect((int32_t)w * 15, 0, 14, 64, held >> k & 1u ? C_GLOBAL : RGB(190, 190, 190));
        c[0] = k == 26u ? '_' : (char)('A' + k);   /* (the top key: a space) */
        cv_text((int32_t)w * 15 + (14 - text_w(&FONT_S, c)) / 2, 44, &FONT_S, c, C_BLACK);
        w++;
    }
    for (k = 0, w = 0; k < 27u; k++) {             /* the black keys, over the line between two white ones */
        int32_t x;
        if (!(BLACK >> k & 1u)) {
            w++;
            continue;
        }
        x = (int32_t)w * 15 - 7;
        cv_rect(x, 0, 13, 38, held >> k & 1u ? C_GLOBAL : RGB(25, 25, 25));
        c[0] = (char)('A' + k);
        cv_text(x + (13 - text_w(&FONT_S, c)) / 2, 12, &FONT_S, c, held >> k & 1u ? C_BLACK : C_WHITE);
    }
    cv_blit(0, 174 + tp_box.top);
    }
}
