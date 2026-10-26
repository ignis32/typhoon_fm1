/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* Bluetooth for typhoon's UI (ui/ui.c, ui/page_bt.c), on fm1-base's plat_ble_* (base/device/radio.c, base/ble/;
 * advertising on / off and the name: patches/fm1-base/0003).
 *
 *   GLOBAL > BLUETOOTH, two sides on their own:
 *   BLE-P (G_BLE_P)  OFF / ON: the FM-1 advertises (as TYPHOON: fm1.toml ble_name), a computer or a phone connects
 *   BLE-C (G_BLE_C)  OFF / FIRST: the FM-1 connects to the first Bluetooth MIDI device it finds / CHOOSE: to the one
 *                    chosen in the list (KNOB 3; remembered: OBJ_BTDEV, its name)
 *   AUTO_RECONNECT   ON: whenever no device is connected (power-on, a device gone, one that did not answer), it looks
 *                    again until one is; OFF: one look (30 s) at power-on and when BLE-C is switched on, and the
 *                    list's pick
 *   Both OFF: the radio off. A change applies once the knob has rested 0.8 s, with no restart.
 *   The list (KNOB 3): every MIDI device around (a scan; fm1-base scans only with no device connected, so opening it
 *   lets the device go, and closing it looks again), REFRESH (the scan again), a device (connect, remember it, BLE-C
 *   CHOOSE), FORGET (the remembered one).
 * What happens goes to Bluetooth's log too (plat_log: the page's last lines), so a reconnect that fails says where.
 * MIDI from both comes in on PORT_BLE (app.c app_loop), the keys go out to both. ble_busy keeps the state save off
 * the flash while the radio starts (an erase stops it). Main loop (app_frame: bt_poll). */
enum { BLE_ABSENT = BLE_FAILED + 1 };            /* (plat.h: BLE_OFF, _STARTING, _READY, _FAILED) */
enum { BC_OFF, BC_FIRST, BC_CHOOSE };            /* G_BLE_C */

#define BTDEV_MAGIC 0x56444254u                  /* "TBDV": OBJ_BTDEV, the remembered device */
#define BT_LOOK_MS 30000u                        /* AUTO_RECONNECT OFF: how long the one look goes on */
#define BT_PAUSE_MS 2000u                        /* after a device did not answer or went away: before looking again */
static struct {
    struct { uint32_t magic; char name[21]; uint8_t rsv[3]; } dev;   /* (the stored part) */
    int16_t p, c;                                  /* BLE-P, BLE-C as applied (p < 0: not yet, at boot) */
    int16_t seen_p, seen_c;                        /* the knobs as last seen, and since when (t_moved) */
    uint32_t t_moved, t_ready, t_look, t_pause;    /* t_look: the look's start; t_pause: no look before it */
    int last_state, last_dev;
    uint8_t loaded, scanning, picking, look;       /* look: a look is on (AUTO_RECONNECT OFF: the one look) */
    uint8_t no_answer, lost, gave_up;              /* what the last attempt came to (the page says it) */
    char peer[21];                                 /* the device connecting or connected */
} bt = {.p = -1, .last_dev = BLE_DEV_OFF};

static void bt_log(const char *a, const char *b)  /* a line in Bluetooth's log (the page shows the last ones) */
{
    char s[48];
    uint32_t n = 0;
    while (*a && n < sizeof s - 1u)
        s[n++] = *a++;
    while (b && *b && n < sizeof s - 1u)
        s[n++] = *b++;
    s[n] = 0;
    plat_log(s);
}

static void bt_dev_load(void)
{
    if (plat_store_load(OBJ_BTDEV, &bt.dev, sizeof bt.dev) != (int)sizeof bt.dev || bt.dev.magic != BTDEV_MAGIC)
        bt.dev.name[0] = 0;
    bt.dev.name[20] = 0;
    bt.loaded = 1;
}

static void bt_scan(int on)
{
    if (bt.scanning != on) {
        bt.scanning = (uint8_t)on;
        plat_ble_scan(on);
    }
}

static int bt_same(const char *a, const char *b)   /* (the device's libc has no strcmp) */
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* what the central connects to among what the scan found: FIRST the first, CHOOSE the remembered one; -1 none */
static int bt_target(void)
{
    char n[21];
    int i, rssi;
    for (i = 0; i < 8 && plat_ble_found(i, n, &rssi); i++)
        if (bt.c == BC_FIRST || bt_same(n, bt.dev.name))
            return i;
    return -1;
}

/* a device's link is on: connecting, finding its MIDI, or connected (SCANNING is no link: until 2026-10-24 it was
 * taken for one, and the reconnect looked at the scan only once, before anything was found: never at power-on) */
static int bt_linked(int d) { return d == BLE_DEV_CONNECTING || d == BLE_DEV_SEARCHING || d == BLE_DEV_READY; }

static void bt_look(void)                        /* a look begins: power-on, BLE-C on, a device gone, the list shut */
{
    bt.look = 1;
    bt.gave_up = 0;
    bt.t_look = fm1_ms;
}

static void bt_apply(void)                       /* the knobs, once they rest: the radio, advertising, the central */
{
    const int was_on = bt.p > 0 || bt.c > 0;
    bt.p = gpar[G_BLE_P];
    bt.c = gpar[G_BLE_C];
    plat_ble_advertise(bt.p);
    if ((bt.p > 0 || bt.c > 0) != was_on)
        plat_ble_enable(bt.p > 0 || bt.c > 0);
    if (bt.c == BC_OFF) {
        bt.picking = 0;
        bt_scan(0);
        if (bt_linked(plat_ble_device()))
            plat_ble_disconnect();
    } else
        bt_look();
    bt_log(bt.p ? "BLE-P ON, BLE-C " : "BLE-P OFF, BLE-C ",
           bt.c == BC_FIRST ? "FIRST" : bt.c == BC_CHOOSE ? "CHOOSE" : "OFF");
}

static void bt_init(void)                        /* app_init, before the radio starts (APP_BLE_AT_BOOT) */
{
    plat_ble_advertise(gpar[G_BLE_P]);
}

static void bt_poll(void)                        /* every frame (fm1-base's ble_poll is the radio's) */
{
    int st = plat_ble_state(), d = plat_ble_device(), i;
    if (!bt.loaded)
        bt_dev_load();
    if (gpar[G_BLE_P] != bt.seen_p || gpar[G_BLE_C] != bt.seen_c) {   /* a knob moved: applied once it rests */
        bt.seen_p = gpar[G_BLE_P];
        bt.seen_c = gpar[G_BLE_C];
        bt.t_moved = fm1_ms;
    }
    if (bt.p < 0) {                                /* (boot: as APP_BLE_AT_BOOT started it) */
        bt.p = bt.seen_p;
        bt.c = bt.seen_c;
        bt_look();
    } else if ((bt.seen_p != bt.p || bt.seen_c != bt.c) && fm1_ms - bt.t_moved > 800u)
        bt_apply();
    if (st == BLE_READY && bt.last_state != BLE_READY) {
        bt.t_ready = fm1_ms | 1u;
        bt.t_look = fm1_ms;                        /* (the look's time counts from the radio up) */
    }
    bt.last_state = st;
    if (d != bt.last_dev) {                        /* the device's link moved on */
        if (d == BLE_DEV_READY) {
            bt.no_answer = bt.lost = 0;
            bt.look = 0;
            bt_log("connected: ", bt.peer);
        } else if (bt_linked(bt.last_dev) && !bt_linked(d) && !bt.picking) {   /* a link ended */
            if (bt.last_dev == BLE_DEV_READY) {
                bt.lost = 1;
                bt_log("gone: ", bt.peer);
            } else {
                bt.no_answer = 1;
                bt_log("no answer: ", bt.peer);
            }
            bt.t_pause = fm1_ms + BT_PAUSE_MS;
            if (gpar[G_BLE_AUTO])
                bt_look();
        }
        bt.last_dev = d;
    }
    if (bt.c == BC_OFF || st != BLE_READY || bt.picking)
        return;
    if (bt_linked(d)) {                            /* connecting or connected: no scan (connecting stopped it) */
        bt.scanning = 0;
        if (d == BLE_DEV_READY)
            bt.look = 0;                           /* (a look asked meanwhile, a knob: there is a device) */
        return;
    }
    if ((bt.c == BC_CHOOSE && !bt.dev.name[0]) || !bt.look || (int32_t)(fm1_ms - bt.t_pause) < 0) {
        bt_scan(0);                                /* nothing to look for, the look over, or a pause */
        return;
    }
    if (!gpar[G_BLE_AUTO] && fm1_ms - bt.t_look > BT_LOOK_MS) {   /* the one look: given up */
        bt.look = 0;
        bt.gave_up = 1;
        bt_scan(0);
        bt_log("not found: ", bt.c == BC_FIRST ? "no MIDI device" : bt.dev.name);
        return;
    }
    bt_scan(1);
    if ((i = bt_target()) >= 0) {
        int rssi;
        plat_ble_found(i, bt.peer, &rssi);
        bt.peer[20] = 0;
        if (!plat_ble_connect(i)) {
            bt.scanning = 0;                       /* (connecting stops the scan) */
            if (!gpar[G_BLE_AUTO])
                bt.look = 0;                       /* (the one look: this was its attempt) */
            bt_log("connecting: ", bt.peer);
        }
    }
}

/* ---- the UI's calls */
static int ble_ui_state(void) { return FM1B_BLE ? plat_ble_state() : BLE_ABSENT; }
static const char *ble_ui_log(int back) { return plat_ble_log_line(back); }
static const char *ble_ui_remembered(void) { return bt.dev.name; }
/* the header's dot: blue while a computer or a device is connected */
static int ble_ui_connected(void) { return plat_ble_connected() || plat_ble_device() == BLE_DEV_READY; }
/* no flash writes now (app.c: the state save waits): while the radio starts and for 5 s after it is ready (an erase
 * stops every read from the flash, the radio's code too: 2026-10-15, switching Bluetooth on hung the FM-1) */
static int ble_busy(void)
{
    return plat_ble_state() == BLE_STARTING ||
           (plat_ble_state() == BLE_READY && bt.t_ready && fm1_ms - bt.t_ready < 5000u);
}

static void put_s(char *d, const char *a, const char *b)   /* d = a b (40 bytes at most) */
{
    uint32_t n = 0;
    while (*a && n < 39u)
        d[n++] = *a++;
    while (b && *b && n < 39u)
        d[n++] = *b++;
    d[n] = 0;
}

/* BLE-P's state in words (s: 40 bytes); 1 = a computer is connected */
static int ble_p_status(char *s)
{
    const int st = plat_ble_state();
    if (!FM1B_BLE) {
        put_s(s, "NOT IN THIS BUILD", 0);
        return 0;
    }
    if (!gpar[G_BLE_P] || st == BLE_OFF) {
        put_s(s, "OFF", 0);
        return 0;
    }
    if (st != BLE_READY) {
        put_s(s, st == BLE_STARTING ? "STARTING" : "FAILED TO START", 0);
        return 0;
    }
    if (plat_ble_connected()) {
        put_s(s, "CONNECTED", 0);
        return 1;
    }
    put_s(s, "VISIBLE AS ", APP_NAME);
    return 0;
}

/* BLE-C's state in words (s: 40 bytes); 1 = a device is connected */
static int ble_c_status(char *s)
{
    const int st = plat_ble_state(), d = plat_ble_device();
    if (!FM1B_BLE || !gpar[G_BLE_C] || st == BLE_OFF) {
        put_s(s, "OFF", 0);
        return 0;
    }
    if (st != BLE_READY) {
        put_s(s, st == BLE_STARTING ? "STARTING" : "FAILED TO START", 0);
        return 0;
    }
    if (d == BLE_DEV_READY) {
        put_s(s, "CONNECTED ", bt.peer);
        return 1;
    }
    if (d == BLE_DEV_CONNECTING)
        put_s(s, "CONNECTING ", bt.peer);
    else if (d == BLE_DEV_SEARCHING)
        put_s(s, "FINDING ITS MIDI: ", bt.peer);
    else if (bt.picking)
        put_s(s, "SCANNING FOR THE LIST", 0);
    else if (bt.c == BC_CHOOSE && !bt.dev.name[0])
        put_s(s, "NONE CHOSEN: KNOB 3", 0);
    else if (bt.gave_up)
        put_s(s, "NOT FOUND: ", bt.c == BC_FIRST ? "NO MIDI DEVICE" : bt.dev.name);
    else if (bt.look && (int32_t)(fm1_ms - bt.t_pause) < 0)
        put_s(s, bt.lost ? "GONE, LOOKING AGAIN" : "NO ANSWER, LOOKING AGAIN", 0);
    else if (bt.look)
        put_s(s, "LOOKING FOR ", bt.c == BC_FIRST ? "A MIDI DEVICE" : bt.dev.name);
    else
        put_s(s, bt.lost ? "GONE" : bt.no_answer ? "NO ANSWER" : "IDLE", 0);
    return 0;
}

/* the list (KNOB 3 on the page): every MIDI device around */
static int ble_list_open(void)                   /* 0 = the list is up (a scan of everything) */
{
    if (!FM1B_BLE || plat_ble_state() != BLE_READY)
        return -1;
    if (!gpar[G_BLE_C]) {                          /* (the list connects: BLE-C on, as CHOOSE) */
        par_set(G(G_BLE_C), BC_CHOOSE);
        bt.seen_c = bt.c = BC_CHOOSE;
    }
    bt.picking = 1;
    if (bt_linked(plat_ble_device()))
        plat_ble_disconnect();                     /* (fm1-base scans only with no device: back when it shuts) */
    bt.scanning = 0;
    bt_scan(1);
    return 0;
}
static int ble_list_count(void)
{
    char n[21];
    int i, rssi;
    for (i = 0; i < 8 && plat_ble_found(i, n, &rssi); i++)
        ;
    return bt.picking ? i : 0;
}
static const char *ble_list_name(int i, int *rssi)
{
    static char n[21];
    if (!plat_ble_found(i, n, rssi)) {
        n[0] = 0;
        *rssi = 0;
    }
    return n;
}
static void ble_list_refresh(void)               /* the scan again: the list starts empty */
{
    plat_ble_scan(0);
    bt.scanning = 0;
    bt_scan(1);
    bt_log("refresh", 0);
}
static void ble_list_close(void)                 /* back: the central looks again */
{
    if (!bt.picking)
        return;
    bt.picking = 0;
    bt_scan(0);
    bt_look();
}
static int ble_list_pick(int i)                  /* OCT+ on a device: connect, remember it, BLE-C CHOOSE */
{
    char n[21];
    int rssi;
    bt.picking = 0;
    if (!plat_ble_found(i, n, &rssi) || plat_ble_connect(i)) {
        bt_look();
        return -1;
    }
    bt.scanning = 0;
    memcpy(bt.peer, n, sizeof n);
    memcpy(bt.dev.name, n, sizeof n);
    bt.dev.magic = BTDEV_MAGIC;
    bt.look = 0;
    if (gpar[G_BLE_C] != BC_CHOOSE) {
        par_set(G(G_BLE_C), BC_CHOOSE);
        bt.seen_c = bt.c = BC_CHOOSE;
    }
    bt_log("chosen: ", n);
    return plat_store_save(OBJ_BTDEV, &bt.dev, sizeof bt.dev) ? -2 : 0;
}
static int ble_forget(void)                      /* the remembered device forgotten */
{
    bt_log("forgotten: ", bt.dev.name);
    bt.dev.name[0] = 0;
    bt.dev.magic = 0;
    if (bt.c == BC_CHOOSE && bt_linked(plat_ble_device()))
        plat_ble_disconnect();
    bt_scan(0);
    return plat_store_save(OBJ_BTDEV, &bt.dev, sizeof bt.dev) ? -1 : 0;
}
