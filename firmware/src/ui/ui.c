/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The UI: draws and edits what ui/layout.h says (pages, knobs, buttons, encoders, CC map). Nothing here knows
 * a particular parameter: labels, values and ranges come from their descriptors (core/params.c).
 *
 * The screen (240 x 240):
 *    0.. 27  header. Left: the preset (number, name, * when edited); right: the page, "L2 ..." on a layer's pages.
 *            A sound page (SP knobs) also says whose its values are: a grey GLOBAL tag (the preset uses the
 *            global group: the values are drawn dim) or a bright OWN tag. A global page (SG knobs) is an amber
 *            bar that says GLOBAL. The SAVE prompt is an amber bar too; a message (SAVED, > GLOBAL ...) shows
 *            on the left for a moment.
 *   30.. 53  status: the note that sounds, the transpose, the breath, the layers' loudness
 *   58..239  the page: UK_KNOBS four rows (label, value, a bar of the range), KNOB 1 at the top;
 *            UK_CURVE / UK_FOLLOW a graph, its four knobs in a row below.
 * Redrawn only when something changed: a parameter (par_dirty), the page, the layer, the header (ui.hdr), the
 * meters ~25 times a second. All of it from the main loop. */

#define C_GLOBAL RGB(235, 150, 30)                 /* the global pages' bar, the SAVE prompt */

/* ms since t; a time is kept as fm1_ms | 1 (0: none), which can be 1 ms ahead: that counts as 0, not as ~49 days */
static uint32_t ms_since(uint32_t t)
{
    uint32_t d = fm1_ms - t;
    return d > 0x80000000u ? 0 : d;
}

static struct {
    uint32_t page, drawn_dirty, drawn_page, drawn_layer, drawn_hdr, hdr, t_meter;
    int32_t drawn_breath;
    uint32_t touched_k, t_touched;                 /* the knob turned last, when (its value shows white) */
    uint8_t full;
    uint8_t solo, solo_mutes[NLAYER];              /* UA_SOLO on: the mutes as they were before */
    uint8_t last[NELEM(BUTTONS)];                  /* a page button: its cycle's page shown last, + 1 (0: none) */
    uint32_t t_down[NELEM(BUTTONS)];               /* a button with a hold: pressed when (0: not pending) */
    uint8_t ask, save_slot;                        /* a question on screen (ASK_*), the SAVE prompt's slot */
    char nm[PS_NAME + 1];                          /* SAVE AS: the name typed, the cursor (0 .. its length), redraws */
    uint8_t nm_cur, nm_n, nm_drawn;
    uint32_t nm_keys;                              /* the keys held when the legend was drawn */
    uint8_t picking;                               /* CHOOSE: the BLUETOOTH page lists devices (KNOB 2) */
    int8_t pick_idx;
    uint32_t t_ask;
    char msg[24];
    uint32_t t_msg;
    uint32_t t_splash;                             /* the transpose's splash: since when (0: none), the value drawn */
    int16_t splash_drawn;
} ui = {.drawn_page = 0xFFu, .full = 1};

static void ui_msg(const char *s)
{
    name_set(ui.msg, s, sizeof ui.msg - 1u);
    ui.t_msg = fm1_ms | 1u;
    ui.hdr++;
}

/* OCT- / OCT+ (the transpose): the new value over the page, big enough to read at arm's length, for 1.2 s after the
 * last press (ui_draw) */
#define SPLASH_MS 1200u
#define SPLASH_X 4                                 /* FONT_L x 4: 128 px high, "-24" 192 px wide */
#define C_XP_UP RGB(255, 150, 40)                  /* transposed up: warm; down: cool (the splash, the status row) */
#define C_XP_DOWN RGB(70, 170, 255)
static uint16_t xpose_colour(int32_t v) { return v > 0 ? C_XP_UP : v < 0 ? C_XP_DOWN : C_DIM; }
static void ui_splash(void)
{
    ui.t_splash = fm1_ms | 1u;
    ui.splash_drawn = 0x7FFF;
}

/* text in f scaled sc times (each pixel a square, coverage over half: lit), centred on the screen at row y; drawn in
 * strips through the canvas (it is 48 rows) */
static void big_text(int32_t y, const felucca_font_t *f, uint32_t sc, const char *t, uint16_t c)
{
    int32_t x0 = 120 - text_w(f, t) * (int32_t)sc / 2, h = (int32_t)(f->h * sc), sy;
    for (sy = 0; sy < h; sy += 48) {
        int32_t x = x0, rows = h - sy < 48 ? h - sy : 48;
        const char *q;
        cv_begin(240, (uint32_t)rows, C_BLACK);
        for (q = t; *q; q++) {
            uint32_t gi = glyph(f, (uint8_t)*q), w = f->bw[gi], bpr = (w + 1u) / 2u, gx, gy;
            const uint8_t *gd = f->data + f->off[gi];
            for (gy = (uint32_t)sy / sc; gy < f->h && (int32_t)(gy * sc) < sy + rows; gy++)
                for (gx = 0; gx < w; gx++) {
                    uint32_t a = gd[gy * bpr + gx / 2u];
                    a = (gx & 1u) ? (a & 15u) : (a >> 4);
                    if (a >= 8u)
                        cv_rect(x + ((int32_t)gx - f->pad) * (int32_t)sc, (int32_t)(gy * sc) - sy, (int32_t)sc,
                                (int32_t)sc, c);
                }
            x += f->adv[gi] * (int32_t)sc;
        }
        cv_blit(0, (uint32_t)(y + sy));
    }
}

static void draw_splash(void)
{
    char t[16], *e;
    int32_t v = gpar[G_XPOSE], o = v / 12, st = v - o * 12;
    lcd_fill(0, 56, 240, 184, C_BLACK);
    draw_text_box(0, 60, 240, &FONT_S, "TRANSPOSE", C_GRAY, 1);
    put_int(t, v, 1);
    big_text(84, &FONT_L, SPLASH_X, t, xpose_colour(v));
    if (!v)                                        /* the octaves and the semitones: +14 is "+1 OCT +2" */
        put_str(t, "AS PLAYED");
    else if (!o)
        put_str(put_int(t, st, 1), st == 1 || st == -1 ? " SEMITONE" : " SEMITONES");
    else {
        e = put_int(t, o, 1);
        put_str(e, st ? " OCT " : " OCT");
        if (st)
            put_int(e + 5, st, 1);
    }
    draw_text_box(0, 218, 240, &FONT_S, t, xpose_colour(v), 1);
}

/* a question on screen: the header asks it (amber), the status row says OCT- NO (left) / OCT+ YES (right) (ui_answer) */
enum { ASK_NONE, ASK_SAVE, ASK_SAVE_AS, ASK_RESET_GLOBAL, ASK_BT_PICK };   /* (ASK_BT_PICK: the BLUETOOTH list's entry) */
static void ui_ask(uint32_t a)
{
    ui.ask = (uint8_t)a;
    ui.t_ask = fm1_ms | 1u;
    ui.hdr++;
}

/* is page pg on the cycle that starts at page start (PAGES[].next)? */
static int page_in_cycle(uint32_t start, uint32_t pg)
{
    uint32_t p = start, n;
    for (n = 0; n < NPAGES && p < NPAGES; n++) {
        if (p == pg)
            return 1;
        p = PAGES[p].next;
        if (p == start)
            break;
    }
    return 0;
}

/* the sound group a page edits (-1: none), and whether it is the global copy */
static int32_t page_group(uint32_t pg, int *global)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        pref_t r = PAGES[pg].k[k];
        if (pr_snd(r)) {
            *global = (r & PR_GLOBAL) != 0;
            return S_GROUP[(r & 0xFFu) % S_COUNT];
        }
    }
    *global = 0;
    return -1;
}

static int page_on_layer(uint32_t pg)              /* edits the current layer (L knobs): the header says which */
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        if (pr_layer(PAGES[pg].k[k]) && ((PAGES[pg].k[k] >> 8) & 0x0Fu) == PR_SEL)
            return 1;
    return 0;
}

static int knob_follows(pref_t r)                  /* a preset's sound value that is the global one's (drawn dim) */
{
    return pr_snd(r) && !(r & PR_GLOBAL) && !snd_own((r & 0xFFu) % S_COUNT);
}

static const char *page_title(const ui_page_t *pg)
{
    const engine_t *e = layer_engine((uint32_t)gpar[G_LAYER]);
    if (!pg->title && trk[gpar[G_LAYER]].p[P_ENGINE] == ENG_ID_OFF)
        return "OFF";                              /* (an OFF layer's engine pages) */
    if (pg->title)
        return pg->title;
    return e->page_title[(pg->eng_page - 1u) & 3u] ? e->page_title[(pg->eng_page - 1u) & 3u] : e->name;
}

/* a knob's label: its descriptor's, "L2 LEVEL" when the handle names a fixed layer */
static void knob_label(pref_t r, char *buf)
{
    const param_desc_t *d = par_desc(r);
    buf[0] = 0;
    if (!d) return;
    if (pr_layer(r) && ((r >> 8) & 0x0Fu) != PR_SEL) {
        buf[0] = 'L';
        buf[1] = (char)('1' + ((r >> 8) & 0x0Fu));
        buf[2] = ' ';
        put_str(buf + 3, d->label);
    } else
        put_str(buf, d->label);
}

static int knob_live(pref_t r)                     /* has a range (engines mark unused knobs "-", 0..0) */
{
    const param_desc_t *d = par_desc(r);
    return d && d->max > d->min;
}

/* EDIT 3 / EDIT 4 of an engine with no knobs there: the EDIT button passes them by */
static int page_empty(uint32_t pg)
{
    uint32_t k;
    if (PAGES[pg].eng_page < 3u)
        return 0;
    for (k = 0; k < 4u; k++)
        if (knob_live(PAGES[pg].k[k]))
            return 0;
    return 1;
}

#include "name_edit.c"                               /* (SAVE AS) */

/* ---- input */
static void ui_act(uint32_t i, uint32_t a)
{
    switch (a) {
    case UA_PAGE: {
        uint32_t start = BUTTONS[i].arg, n;
        if (page_in_cycle(start, ui.page)) {       /* again: the next page of the cycle */
            if (PAGES[ui.page].next != PG_STAY)
                ui.page = PAGES[ui.page].next;
        } else
            ui.page = ui.last[i] ? ui.last[i] - 1u : start;
        for (n = 0; n < NPAGES && page_empty(ui.page); n++)
            ui.page = PAGES[ui.page].next;
        ui.last[i] = (uint8_t)(ui.page + 1u);
        break;
    }
    case UA_XPOSE: {
        int32_t v = gpar[G_XPOSE];
        uint32_t other = BUTTONS[i].btn == BTN_OCT_DOWN ? BTN_OCT_UP : BTN_OCT_DOWN;
        if (tp_in.buttons >> other & 1u)
            v = 0;                                 /* both: back to 0 */
        else
            v += BUTTONS[i].step * ((tp_in.buttons >> BTN_SEL & 1u) ? 12 : 1);
        par_set(G(G_XPOSE), v);
        ui_splash();
        break;
    }
    case UA_XPOSE_OCT:                             /* (a hold of OCT- / OCT+) */
        par_set(G(G_XPOSE), gpar[G_XPOSE] + BUTTONS[i].step * 12);
        ui_splash();
        break;
    case UA_MUTE:
        par_set(L(P_MUTE), !par_get(L(P_MUTE)));
        break;
    case UA_SOLO: {
        uint32_t l;
        for (l = 0; l < NLAYER; l++) {
            if (!ui.solo)
                ui.solo_mutes[l] = (uint8_t)trk[l].p[P_MUTE];
            par_set(LN(l, P_MUTE), ui.solo ? ui.solo_mutes[l] : l != (uint32_t)gpar[G_LAYER]);
        }
        ui.solo = !ui.solo;
        break;
    }
    case UA_SAVE:
        ui_ask(ASK_SAVE);
        ui.save_slot = ps.cur;
        break;
    case UA_SAVE_AS:                               /* (a hold of SAVE) the name: what plays, the cursor at its end */
        ui_ask(ASK_SAVE_AS);
        ui.save_slot = ps.cur;
        name_set(ui.nm, ps.name, PS_NAME);
        ui.nm_cur = (uint8_t)par_strlen(ui.nm);
        ui.full = 1;
        nm_changed();
        break;
    case UA_ASK_GLOBAL:
        ui_ask(ASK_RESET_GLOBAL);                  /* (hold GLO; the BLUETOOTH page's FORGET is in its list now) */
        break;
    case UA_GLOBAL: {
        int glob;
        int32_t g = page_group(ui.page, &glob);
        if (g >= 0 && !glob && (s_own >> g & 1u)) {
            group_global((uint32_t)g);
            ui_msg("> GLOBAL");
        }
        break;
    }
    case UA_ALL_OFF:
        perf_all_off();
        ui.ask = ASK_NONE;
        ui_msg("ALL NOTES OFF");
        break;
    }
}

/* the question on screen answered: UI_YES (OCT+), UI_NO (OCT-; any other button, 10 s) */
static void ui_answer(int yes)
{
    uint32_t a = ui.ask;
    ui.ask = ASK_NONE;
    ui.hdr++;
    if (!yes && a == ASK_BT_PICK) {                /* OCT-: the list shut, the central looks again */
        ble_list_close();
        ui.picking = 0;
        ui.full = 1;
    }
    if (a == ASK_SAVE_AS)
        ui.full = 1;                               /* (the page back where the name was) */
    if (!yes) {
        ui_msg("CANCELLED");
        return;
    }
    if (a == ASK_SAVE)
        ui_msg(ps_save(ui.save_slot) ? "SAVE FAILED" : "SAVED");
    else if (a == ASK_SAVE_AS)
        nm_save();
    else if (a == ASK_RESET_GLOBAL) {
        globals_defaults();
        ui_msg("GLOBAL RESET");
    } else if (a == ASK_BT_PICK) {                 /* OCT+ on the list's entry */
        const int n = ble_list_count();
        if (ui.pick_idx == 0) {                    /* REFRESH: the scan again, the list stays up */
            ble_list_refresh();
            ui_ask(ASK_BT_PICK);                   /* (a question stays while the list is up: OCT- shuts it, never
                                                    * transposes) */
        } else if (ui.pick_idx <= n) {             /* a device: connect, remember it */
            ui.picking = 0;
            ui.full = 1;
            ui_msg(ble_list_pick(ui.pick_idx - 1) == -1 ? "NOT FOUND" : "CONNECTING");
        } else {                                   /* FORGET */
            ble_list_close();
            ui.picking = 0;
            ui.full = 1;
            ui_msg(ble_forget() ? "FORGET FAILED" : "DEVICE FORGOTTEN");
        }
    }
}

/* the BLUETOOTH page's list: REFRESH, the devices found, FORGET (when one is remembered) */
static int bt_entries(void) { return 1 + ble_list_count() + (ble_ui_remembered()[0] != 0); }

static void ui_input(void)
{
    uint32_t b, rel, i, k;
    for (k = 0; k < 4u; k++) {                     /* KNOB 1..4: the page's parameters */
        int32_t st = tp_enc(ENC_KNOB1 + k);
        pref_t r = PAGES[ui.page].k[k];
        if (st && ui.page == PG_BT && k == 2u) {   /* KNOB 3: the device list (app/ble.c) */
            if (!ui.picking) {                     /* the first turn: up, scanning, REFRESH first */
                if (ble_list_open()) {
                    ui_msg(ble_ui_state() == BLE_ABSENT ? "NO BLUETOOTH" : "BLUETOOTH IS OFF");
                    continue;
                }
                ui.picking = 1;
                ui.pick_idx = 0;
            } else
                ui.pick_idx = (int8_t)clamp(ui.pick_idx + st, 0, bt_entries() - 1);   /* (as far as it turned) */
            ui_ask(ASK_BT_PICK);
            ui.full = 1;
            ui.hdr++;
            continue;
        }
        if (st && knob_live(r)) {
            par_step(r, st);
            ui.touched_k = k;
            ui.t_touched = fm1_ms | 1u;
        }
    }
    for (i = 0; i < NELEM(ENCODERS); i++) {
        int32_t st = tp_enc(ENCODERS[i].enc), dir;
        if (!st) continue;
        dir = st > 0 ? 1 : -1;
        switch (ENCODERS[i].kind) {
        case UE_PRESET:
            if (ui.ask == ASK_SAVE || ui.ask == ASK_SAVE_AS) {
                ui.save_slot = (uint8_t)(((int32_t)ui.save_slot + st % (int32_t)NPRESET + (int32_t)NPRESET) % (int32_t)NPRESET);
                ui.t_ask = fm1_ms | 1u;
                ui.hdr++;
            } else {
                ps_load((uint32_t)(((int32_t)ps.cur + st % (int32_t)NPRESET + (int32_t)NPRESET) % (int32_t)NPRESET));
                ui.solo = 0;
            }
            break;
        case UE_LAYER:
            if (ui_typing()) {                     /* SAVE AS: the cursor */
                ui.nm_cur = (uint8_t)clamp((int32_t)ui.nm_cur + dir, 0, (int32_t)par_strlen(ui.nm));
                nm_changed();
                break;
            }
            par_set(G(G_LAYER), (gpar[G_LAYER] + dir + (int32_t)NLAYER) % (int32_t)NLAYER);
            break;
        default:
            if (ui_typing())                       /* SAVE AS: the character at the cursor */
                nm_turn(st);
            else
                par_step(ENCODERS[i].r, dir);
            break;
        }
    }
    b = tp_edges(&rel);
    for (i = 0; i < NELEM(BUTTONS); i++) {
        uint32_t bit = BUTTONS[i].btn;
        if (b >> bit & 1u) {
            if (ui_typing()) {                     /* SAVE AS: OCT+ saves, OCT- cancels, DELETE; the rest nothing */
                ui.t_down[i] = 0;
                if (bit == UI_YES || bit == UI_NO)
                    ui_answer(bit == UI_YES);
                else if (bit == UI_DELETE)
                    nm_delete();
                continue;
            }
            if (ui.ask) {                          /* a question: YES, NO; SAVE again in the SAVE prompt is YES too; */
                ui.t_down[i] = 0;                  /* anything else is NO. The press goes no further */
                ui_answer(bit == UI_YES || (ui.ask == ASK_SAVE && BUTTONS[i].act == UA_SAVE));
                continue;
            }
            if (BUTTONS[i].act == UA_XPOSE && (tp_in.buttons & (1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) ==
                (1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) {   /* OCT- and OCT+ together: 0 at once, */
                uint32_t j;                        /* and neither acts when let go */
                for (j = 0; j < NELEM(BUTTONS); j++)
                    if (BUTTONS[j].act == UA_XPOSE)
                        ui.t_down[j] = 0;
                ui_act(i, UA_XPOSE);
                continue;
            }
            if (BUTTONS[i].hold == UA_NONE)
                ui_act(i, BUTTONS[i].act);
            else
                ui.t_down[i] = fm1_ms | 1u;
        }
        if (ui.t_down[i]) {                        /* let go before the hold: the press; held on: the hold */
            if (!(tp_in.buttons >> bit & 1u)) {
                ui.t_down[i] = 0;
                ui_act(i, BUTTONS[i].act);
            } else if (ms_since(ui.t_down[i]) >= UI_HOLD_MS) {
                ui.t_down[i] = 0;
                ui_act(i, BUTTONS[i].hold);
            }
        }
    }
    if (ui.picking && (ui.page != PG_BT || ble_ui_state() != BLE_READY || ui.ask != ASK_BT_PICK)) {
        ble_list_close();                          /* (away from the page, the radio gone, or another button: shut) */
        ui.picking = 0;
        if (ui.ask == ASK_BT_PICK)
            ui.ask = ASK_NONE;
        ui.full = 1;
        ui.hdr++;
    }
    if (ui.picking && ui.pick_idx >= bt_entries())
        ui.pick_idx = (int8_t)(bt_entries() - 1);  /* (a refresh: the list shorter) */
    if (ui.ask && ui.ask != ASK_BT_PICK && !ui_typing() && ms_since(ui.t_ask) > 10000u)   /* (picking: scanning takes time; a name: typing) */
        ui_answer(0);
    if (ui.t_msg && ms_since(ui.t_msg) > 1500u) {
        ui.t_msg = 0;
        ui.hdr++;
    }
}

/* the button LEDs say where you are (ui/layout.h BUTTONS) */
static void ui_leds(void)
{
    uint32_t i;
    for (i = 0; i < NELEM(BUTTONS); i++) {
        int on = 0;
        switch (BUTTONS[i].act) {
        case UA_PAGE: on = page_in_cycle(BUTTONS[i].arg, ui.page); break;
        case UA_XPOSE: on = ui.ask ? (int)(fm1_ms >> 8 & 1u) : gpar[G_XPOSE] * BUTTONS[i].step > 0; break;   /* OCT-: below 0, OCT+: above; a question: both blink (NO, YES) */
        case UA_MUTE: on = ui_typing() || trk[gpar[G_LAYER]].p[P_MUTE] != 0 ||   /* (SAVE AS: it deletes) */
                           (ui.solo && (fm1_ms >> 8 & 1u)); break;   /* (solo, PLAY's hold: blinks) */
        case UA_SOLO: on = ui.solo; break;
        case UA_SAVE: on = ui.ask == ASK_SAVE || ui_typing() ? (int)(fm1_ms >> 7 & 1u) : ps_edited; break;
        }
        tp_led(BUTTONS[i].btn, on);
    }
}

/* a MIDI CC from the layout's map: 1 = taken */
static int ui_cc(uint32_t cc, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < NELEM(CC_MAP); i++)
        if (CC_MAP[i].cc == cc) {
            par_cc(CC_MAP[i].r, v);
            return 1;
        }
    return 0;
}

/* ---- drawing */
static void note_name(uint32_t n, char *s)
{
    static const char *const NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (n > 127u) { put_str(s, "-"); return; }
    put_str(s, NAME[n % 12u]);
    put_int(s + par_strlen(s), (int32_t)(n / 12u) - 1, 0);
}

static void slot_text(uint32_t k, const char *name, char *s)   /* "07 FLUTE" */
{
    s[0] = (char)('0' + (k + 1u) / 10u);
    s[1] = (char)('0' + (k + 1u) % 10u);
    s[2] = ' ';
    put_str(s + 3, name);
}

static void fit(char *s, int32_t w)                /* cut s until it fits w pixels */
{
    uint32_t n = par_strlen(s);
    while (n && text_w(&FONT_S, s) > w)
        s[--n] = 0;
}

static void draw_header(void)
{
    char left[40], right[24];
    const ui_page_t *pg = &PAGES[ui.page];
    uint32_t n = (uint32_t)gpar[G_LAYER];
    int glob, x;
    int32_t g = page_group(ui.page, &glob), wr, wt = 0;
    uint16_t bg = C_BLACK, cl = C_HI, cr = C_AMB;
    const char *tag = 0;
    right[0] = 0;
    if (page_on_layer(ui.page)) {                  /* "L2 OSC" */
        right[0] = 'L';
        right[1] = (char)('1' + n);
        right[2] = ' ';
        put_str(right + 3, page_title(pg));
        if (trk[n].p[P_MUTE])
            cr = RGB(220, 70, 70);                 /* the layer is muted */
    } else
        put_str(right, page_title(pg));
    if (ui.ask) {                                  /* a question */
        bg = C_GLOBAL;
        cl = cr = C_BLACK;
        if (ui.ask == ASK_SAVE_AS) {               /* "SAVE AS 23 CLARINET ?": the slot, what is there now */
            put_str(left, "SAVE AS ");
            slot_text(ui.save_slot, ps_ix[ui.save_slot].name, left + 8);
            put_str(left + par_strlen(left), " ?");
        } else if (ui.ask == ASK_SAVE) {
            put_str(left, "SAVE TO ");
            slot_text(ui.save_slot, ps_ix[ui.save_slot].name, left + 8);
            put_str(left + par_strlen(left), " ?");
        } else
            if (ui.ask == ASK_BT_PICK) {           /* the list's entry: "REFRESH ?", "CONNECT re.corder ?", "FORGET ..." */
                int rssi;
                if (ui.pick_idx == 0)
                    put_str(left, "REFRESH THE LIST ?");
                else if (ui.pick_idx <= ble_list_count()) {
                    put_str(left, "CONNECT ");
                    put_str(left + 8, ble_list_name(ui.pick_idx - 1, &rssi));
                    put_str(left + par_strlen(left), " ?");
                } else {
                    put_str(left, "FORGET ");
                    put_str(left + 7, ble_ui_remembered());
                    put_str(left + par_strlen(left), " ?");
                }
            } else
                put_str(left, "GLOBAL TO FACTORY ?");
        right[0] = 0;
    } else if (g >= 0 && glob) {                   /* a global page */
        bg = C_GLOBAL;
        cl = cr = C_BLACK;
        put_str(left, "GLOBAL");
    } else {
        slot_text(ps.cur, ps.name, left);
        if (ps_edited)
            put_str(left + par_strlen(left), "*");
        if (g >= 0)
            tag = (s_own >> g & 1u) ? "OWN" : "GLOBAL";
    }
    if (ui.t_msg && !ui.ask) {
        name_set(left, ui.msg, sizeof left - 1u);
        cl = bg == C_BLACK ? C_WHITE : C_BLACK;
    }
    cv_begin(240, 28, bg);
    if (tag)
        wt = text_w(&FONT_S, tag) + 8;
    wr = right[0] ? text_w(&FONT_S, right) : 0;
    x = 234 - wr;
    if (tag) {                                     /* the tag, then the title */
        int own = tag[0] == 'O';
        int32_t tx = x - wt - 6;
        cv_rect(tx, 5, wt, 19, own ? C_HI : C_LINE);
        cv_text(tx + 4, 6, &FONT_S, tag, own ? C_BLACK : C_GRAY);
        x = tx;
    }
    if (wr)
        cv_text(234 - wr, 6, &FONT_S, right, cr);
    fit(left, x - 12);
    cv_text(6, 6, &FONT_S, left, cl);
    cv_rect(0, 27, 240, 1, C_LINE);
    cv_blit(0, 0);
}

static void draw_status(void)
{
    char s[8];
    uint32_t l;
    int32_t w = (perf_ui.breath * 92) >> 15;
    cv_begin(240, 24, C_BLACK);
    if (ui.ask) {                                  /* a question: how to answer it */
        /* where the buttons are: OCT- (NO) left, OCT+ (YES) right; the SAVE prompt's slot knob between */
        cv_text(6, 4, &FONT_S, "OCT- NO", C_GLOBAL);
        cv_text(234 - text_w(&FONT_S, "OCT+ YES"), 4, &FONT_S, "OCT+ YES", C_GLOBAL);
        if (ui.ask == ASK_SAVE || ui.ask == ASK_SAVE_AS)
            cv_text(120 - text_w(&FONT_S, "PRESETS") / 2, 4, &FONT_S, "PRESETS", C_GRAY);
        cv_blit(0, 30);
        return;
    }
    note_name(perf_ui.note < 128u ? (uint32_t)clamp(perf_ui.note + gpar[G_XPOSE], 0, 127) : perf_ui.note, s);
    cv_text(6, 4, &FONT_S, s, perf_ui.gate ? C_WHITE : C_DIM);
    if (gpar[G_XPOSE]) {                           /* the transpose */
        put_int(s, gpar[G_XPOSE], 1);
        cv_text(46, 4, &FONT_S, s, xpose_colour(gpar[G_XPOSE]));
    }
    cv_rect(84, 8, 92, 8, C_LINE);                 /* the breath */
    cv_rect(84, 8, w, 8, C_HI);
    if (ble_ui_state() == BLE_STARTING || ble_ui_state() == BLE_READY)   /* Bluetooth: dim on, blue connected */
        cv_rect(180, 9, 6, 6, ble_ui_connected() ? RGB(70, 140, 255) : C_DIM);
    for (l = 0; l < NLAYER; l++) {                 /* each layer's loudness */
        int32_t h = (perf_ui.amp[l] * 20) >> 15;
        uint16_t c = perf_ui.missing[l] ? RGB(200, 40, 40) : trk[l].p[P_MUTE] ? RGB(90, 30, 30)   /* muted */
                   : trk[l].p[P_LEVEL] ? C_LINE : C_BLACK;
        cv_rect(190 + (int32_t)l * 15, 2, 11, 20, c);
        cv_rect(190 + (int32_t)l * 15, 22 - h, 11, h, l == (uint32_t)gpar[G_LAYER] ? C_WHITE : C_AMB);
    }
    cv_blit(0, 30);
}

static void draw_knob_row(uint32_t k)
{
    pref_t r = PAGES[ui.page].k[k];
    const param_desc_t *d = par_desc(r);
    char lab[24], val[24];
    int hot = ui.t_touched && ui.touched_k == k && ms_since(ui.t_touched) < 1500u, dim = knob_follows(r);
    int here = pr_layer(r) && ((r >> 8) & 0x0Fu) != PR_SEL && par_layer(r) == (uint32_t)gpar[G_LAYER];
    cv_begin(240, 44, C_BLACK);
    if (here)                                      /* (MIX: the layer SELECT is on, the one PLAY mutes) */
        cv_rect(0, 0, 3, 36, C_GLOBAL);
    if (knob_live(r)) {
        int32_t v = par_get(r), span = d->max - d->min, x;
        uint16_t cv = hot ? C_WHITE : dim ? C_DIM : C_HI, cb = hot ? C_WHITE : dim ? C_DIM : C_AMB;
        knob_label(r, lab);
        par_text(r, val);
        if (pr_layer(r) && (r & 0xFFu) == P_LEVEL && trk[par_layer(r)].p[P_MUTE])
            put_str(val + par_strlen(val), " MUTED");   /* (the MIX page: a level kept while muted) */
        lab[0] = lab[0] ? lab[0] : ' ';
        cv_text(6, 3, &FONT_S, (const char[]){(char)('1' + k), 0}, C_DIM);
        cv_text(22, 3, &FONT_S, lab, here ? C_WHITE : C_GRAY);
        if (pr_layer(r) && ((r >> 8) & 0x0Fu) != PR_SEL && (r & 0xFFu) == P_LEVEL) {   /* (MIX: whose level) */
            char en[24];
            int32_t ex = 22 + text_w(&FONT_S, lab) + 10;
            par_text(LN(par_layer(r), P_ENGINE), en);
            fit(en, 234 - text_w(&FONT_S, val) - 8 - ex);
            cv_text(ex, 3, &FONT_S, en, C_AMB);
        }
        cv_text(234 - text_w(&FONT_S, val), 3, &FONT_S, val, cv);
        cv_rect(22, 28, 212, 6, C_LINE);
        if (pr_layer(r) && (r & 0xFFu) == P_ENGINE) {   /* the engine: its place in this build's list */
            v = (int32_t)eng_pos((uint32_t)v);       /* (0 .. NENGINES - 1 the engines, NENGINES OFF) */
            span = (int32_t)NENGINES;
            d = &(const param_desc_t){"", F_INT, 0, 0, 0, 0, 0};
        }
        x = span ? (v - d->min) * 212 / span : 0;
        if (d->min < 0 && d->max > 0) {            /* bipolar: from the middle */
            int32_t z = (0 - d->min) * 212 / span;
            cv_rect(22 + (x < z ? x : z), 28, x < z ? z - x : x - z + 1, 6, cb);
        } else
            cv_rect(22, 28, x + 1, 6, cb);
    }
    cv_blit(0, 58 + k * 45);
}

/* the breath curve the page edits (its first three knobs: LOW, HIGH, SHAPE): CC2 in (x) -> breath out (y), the
 * live breath as a dot */
#define CRV_W 224
#define CRV_H 112
static void draw_curve(void)
{
    const pref_t *kk = PAGES[ui.page].k;
    int32_t x, py, bx, by, lo = par_get(kk[0]), hi = par_get(kk[1]), sh = par_get(kk[2]);
    uint16_t c = knob_follows(kk[0]) ? C_DIM : C_HI;
    TP_BOX(240, CRV_H + 8, C_BLACK) {
    py = -1;
    cv_rect(8, 4, 1, CRV_H, C_LINE);
    cv_rect(8, 4 + CRV_H - 1, CRV_W, 1, C_LINE);
    cv_rect(8 + (lo > 0 ? lo : 0) * CRV_W / 127, 4, 1, CRV_H, C_DIM);   /* LOW (below 0: at the edge) and HIGH */
    cv_rect(8 + (hi * CRV_W) / 127, 4, 1, CRV_H, C_DIM);
    for (x = 0; x < CRV_W; x++) {
        int32_t y = (breath_curve_of(x * 32767 / (CRV_W - 1), lo, hi, sh) * (CRV_H - 1)) >> 15, yy = 4 + CRV_H - 1 - y;
        if (py < 0) py = yy;
        cv_rect(8 + x, yy < py ? yy : py, 2, (yy < py ? py - yy : yy - py) + 2, c);
        py = yy;
    }
    bx = 8 + ((perf_ui.breath_in * (CRV_W - 1)) >> 15);              /* the live breath */
    by = 4 + CRV_H - 1 - ((perf_ui.breath * (CRV_H - 1)) >> 15);
    cv_rect(bx - 3, by - 3, 7, 7, C_WHITE);
    cv_blit(0, 58 + tp_box.top);
    }
}

/* the current layer's volume envelope (core/layers.c env_step: the same steps), over a note held long enough to see
 * DELAY, ATTACK and DECAY into SUSTAIN, then let go (the line) and its RELEASE. With BR>AMP the breath scales it too
 * (not drawn) */
static void draw_follow(void)
{
    const int16_t *p = trk[gpar[G_LAYER]].p;
    uint32_t held = clamp((int32_t)(env_ms(p[P_DELAY]) + env_ms(p[P_ATK]) + env_ms(p[P_DEC]) + 400u), 800, 6000);
    uint32_t total = held + clamp((int32_t)env_ms(p[P_REL]) + 200, 200, 4000), nb = total * BLOCKS_PER_S / 1000u;
    uint32_t b, off = held * BLOCKS_PER_S / 1000u;
    env_t e;
    int32_t x, pa;
    char s[16];
    TP_BOX(240, CRV_H + 8, C_BLACK) {
    b = 0;
    e = (env_t){ENV_OFF, 0, 0};
    pa = -1;
    cv_rect(8, 4, 1, CRV_H, C_LINE);
    cv_rect(8, 4 + CRV_H - 1, CRV_W, 1, C_LINE);
    cv_rect(8 + (int32_t)(held * CRV_W / total), 4, 1, CRV_H, C_DIM);   /* let go */
    env_start(&e, p);
    for (x = 0; x < CRV_W; x++) {
        uint32_t upto = (uint32_t)(x + 1) * nb / CRV_W;
        int32_t ya;
        for (; b < upto; b++)                      /* the blocks of this pixel column */
            env_step(&e, p, b < off, 0);
        ya = 4 + CRV_H - 1 - ((e.lev * (CRV_H - 1)) >> 15);
        if (pa < 0) pa = ya;
        cv_rect(8 + x, ya < pa ? ya : pa, 2, (ya < pa ? pa - ya : ya - pa) + 2, C_HI);
        pa = ya;
    }
    put_int(s, (int32_t)(total / 100u) / 10, 0);   /* the time shown */
    put_str(s + par_strlen(s), ".");
    put_int(s + par_strlen(s), (int32_t)(total / 100u) % 10, 0);
    put_str(s + par_strlen(s), " s");
    cv_text(238 - text_w(&FONT_S, s), 6, &FONT_S, s, C_DIM);
    cv_blit(0, 58 + tp_box.top);
    }
}

static void draw_curve_knobs(void)                 /* (the knob row under a graph page) */
{
    uint32_t k;
    TP_BOX(240, 52, C_BLACK) {
    for (k = 0; k < 4u; k++) {
        pref_t r = PAGES[ui.page].k[k];
        char lab[24], val[24];
        int hot = ui.t_touched && ui.touched_k == k && ms_since(ui.t_touched) < 1500u;
        if (!knob_live(r)) continue;
        knob_label(r, lab);
        par_text(r, val);
        cv_text(6 + (int32_t)k * 59, 4, &FONT_S, lab, C_GRAY);
        cv_text(6 + (int32_t)k * 59, 26, &FONT_S, val, hot ? C_WHITE : knob_follows(r) ? C_DIM : C_HI);
    }
    cv_blit(0, 186 + tp_box.top);
    }
}

#include "page_bt.c"                                 /* (the BLUETOOTH page) */

#if defined(TYPHOON_SIM)
#endif
#if defined(TYPHOON_SIM)
static void host_page(uint32_t pg) { ui.page = pg % NPAGES; }   /* the simulator (sim_page, a script's "page") */
static void host_preset(uint32_t k)                     /* the simulator (sim_preset, a script's "preset"); 255: INIT (ps_load_init) */
{
    if (k == 255u)
        ps_load_init();
    else
        ps_load(k);
}
#endif

static void ui_draw(void)
{
    const ui_page_t *pg = &PAGES[ui.page];
    uint32_t k, full = ui.full || ui.page != ui.drawn_page;
    if (full) {                                    /* another page (or after a screen of its own): cleared first */
        lcd_fill(0, 28, 240, 212, C_BLACK);
        ui.full = 0;
        ui.drawn_page = ui.page;
        ui.drawn_dirty = par_dirty - 1;
        ui.nm_drawn = (uint8_t)(ui.nm_n - 1u);
    }
    if ((uint32_t)gpar[G_LAYER] != ui.drawn_layer) {   /* another layer: the same page drawn over, no clear (every
                                                    * row, graph and the BT page fills its own box: no black flash) */
        ui.drawn_layer = (uint32_t)gpar[G_LAYER];
        ui.drawn_dirty = par_dirty - 1;
        ui.hdr++;
    }
    if (ui.drawn_hdr != ui.hdr) {
        ui.drawn_hdr = ui.hdr;
        draw_header();
    }
    if (ui.t_splash && ms_since(ui.t_splash) < SPLASH_MS) {   /* the transpose's splash, over the page */
        if (ui.splash_drawn != gpar[G_XPOSE]) {
            ui.splash_drawn = gpar[G_XPOSE];
            draw_splash();
        }
    } else if (ui.t_splash) {                      /* over: the page again, whole */
        ui.t_splash = 0;
        ui.full = 1;
    } else if (ui_typing()) {                      /* SAVE AS: the name instead of the page */
        if (ui.nm_drawn != ui.nm_n) {
            ui.nm_drawn = ui.nm_n;
            draw_name();
            ui.nm_keys = ~tp_in.notes;            /* (the legend too) */
        }
        if (ui.nm_keys != tp_in.notes) {          /* the legend: the held keys lit */
            ui.nm_keys = tp_in.notes;
            draw_legend();
        }
    } else if (ui.drawn_dirty != par_dirty || (ui.t_touched && ms_since(ui.t_touched) > 1500u && ms_since(ui.t_touched) < 1600u)) {
        ui.drawn_dirty = par_dirty;
        draw_header();
        if (pg->kind == UK_CURVE || pg->kind == UK_FOLLOW) {
            if (pg->kind == UK_CURVE)
                draw_curve();
            else
                draw_follow();
            draw_curve_knobs();
        } else if (pg->kind == UK_BT)
            draw_bt();
        else
            for (k = 0; k < 4u; k++)
                draw_knob_row(k);
    }
    if (fm1_ms - ui.t_meter >= 40u) {              /* the meters, ~25 a second */
        ui.t_meter = fm1_ms;
        draw_status();
        if (pg->kind == UK_BT && fm1_ms % 400u < 40u)   /* (the log, a few times a second) */
            draw_bt();
        if (pg->kind == UK_CURVE && perf_ui.breath != ui.drawn_breath) {
            ui.drawn_breath = perf_ui.breath;
            draw_curve();
        }
    }
}
