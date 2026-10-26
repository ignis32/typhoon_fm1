/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The BLUETOOTH page (UK_BT; app/ble.c): KNOB 1 BLE-P, KNOB 2 BLE-C, KNOB 3 the device list, KNOB 4 AUTO_RECONNECT,
 * each side's state in words under it, the remembered device, and the newest lines of Bluetooth's log (it survives a
 * reset: after a crash it says what happened). While the list is up (ui.picking) it fills the page: REFRESH, the
 * devices found (the remembered one marked, the signal in bars), FORGET; the entry KNOB 3 is on is lit, and the
 * header asks for it (OCT+ yes, OCT- shuts the list). Part of ui/ui.c (included there, with its drawing helpers). */
static void bt_row(int y, const char *n, const char *label, pref_t r)   /* "1 BLE-P ........ ON" */
{
    char v[24];
    cv_text(6, y, &FONT_S, n, C_DIM);
    cv_text(22, y, &FONT_S, label, C_GRAY);
    par_text(r, v);
    cv_text(234 - text_w(&FONT_S, v), y, &FONT_S, v, C_AMB);
}

static void bt_bars(int x, int y, int rssi, uint16_t c)   /* the signal: 4 bars, -60 dBm and up all of them */
{
    const int n = rssi >= -60 ? 4 : rssi >= -70 ? 3 : rssi >= -80 ? 2 : 1;
    int i;
    for (i = 0; i < 4; i++)
        cv_rect(x + 5 * i, y + 12 - 3 * i, 3, 3 + 3 * i, i < n ? c : C_LINE);
}

static void draw_bt_list(void)
{
    char s[48];
    const int n = ble_list_count(), forget = ble_ui_remembered()[0] != 0;
    int i, y;
    put_str(s, "DEVICES ");
    put_int(s + par_strlen(s), n, 0);
    put_str(s + par_strlen(s), n < 8 ? "   SCANNING ..." : "");
    cv_text(6, 2, &FONT_S, s, C_DIM);
    for (i = 0, y = 20; i < 1 + n + forget; i++, y += 16) {
        const int hot = i == ui.pick_idx;
        const uint16_t fg = hot ? C_BLACK : C_GRAY;
        int rssi = 0;
        if (hot)
            cv_rect(0, y - 1, 240, 16, C_HI);
        if (i == 0)
            put_str(s, "~ REFRESH");
        else if (i <= n) {
            const char *nm = ble_list_name(i - 1, &rssi);
            s[0] = ble_ui_remembered()[0] && bt_same(nm, ble_ui_remembered()) ? '*' : ' ';
            s[1] = ' ';
            name_set(s + 2, nm, 20);
            bt_bars(210, y, rssi, hot ? C_BLACK : C_HI);
        } else {
            put_str(s, "x FORGET ");
            put_str(s + 9, ble_ui_remembered());
        }
        fit(s, i > 0 && i <= n ? 196 : 228);
        cv_text(6, y, &FONT_S, s, i == 0 || i > n ? (hot ? C_BLACK : C_AMB) : fg);
    }
}

static void draw_bt(void)
{
    char s[48];
    int i, y;
    TP_BOX(240, 182, C_BLACK) {
    if (ui.picking)
        draw_bt_list();
    else if (ble_ui_state() == BLE_ABSENT)
        cv_text(22, 2, &FONT_S, "NOT IN THIS BUILD", C_DIM);
    else {
        int on;
        bt_row(2, "1", "BLE-P", G(G_BLE_P));
        on = ble_p_status(s);
        fit(s, 212);
        cv_text(22, 20, &FONT_S, s, on ? C_WHITE : C_HI);
        bt_row(40, "2", "BLE-C", G(G_BLE_C));
        on = ble_c_status(s);
        fit(s, 212);
        cv_text(22, 58, &FONT_S, s, on ? C_WHITE : C_HI);
        cv_text(6, 78, &FONT_S, "3", C_DIM);
        cv_text(22, 78, &FONT_S, "CHOOSE", C_GRAY);
        name_set(s, ble_ui_remembered()[0] ? ble_ui_remembered() : "(NONE)", 20);
        fit(s, 140);
        cv_text(234 - text_w(&FONT_S, s), 78, &FONT_S, s, C_AMB);
        cv_text(22, 96, &FONT_S, "TURN: THE LIST OF DEVICES", C_DIM);
        bt_row(116, "4", "AUTO_RECONNECT", G(G_BLE_AUTO));
        cv_rect(0, 136, 240, 1, C_LINE);
        for (i = 0, y = 142; i < 2; i++, y += 18) {   /* the log, newest first */
            const char *ln = ble_ui_log(i);
            if (!ln[0])
                break;
            name_set(s, ln, 40);
            fit(s, 228);
            cv_text(6, y, &FONT_S, s, i ? C_DIM : C_GRAY);
        }
    }
    cv_blit(0, 58 + tp_box.top);
    }
}
