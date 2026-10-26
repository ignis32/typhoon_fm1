/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* typhoon's simulator: the whole app (app/app.c, typhoon itself) on fm1-base's host platform (base/host/plat_host.c:
 * the simulated FM-1, its flash, MIDI queue, panel and screen), compiled to WebAssembly (web/sim/build.sh: zig cc,
 * wasm32-wasi) and driven from JavaScript instead of a script: the tests (tests/sim/, through tests/sim/lib.mjs), later
 * the page.
 *
 * The clock is fm1-base's browser emulator's (base/web/emu/emu_web.c, included here): sim_render(n) is its
 * web_render, which runs the device a millisecond at a time (plat_host.c run_ms: Bluetooth's tick, app_loop, a
 * 256-frame block when one is due, app_frame every 16 ms) until n frames exist. A SysEx frame in (sim_sx_in) is taken
 * at the end of the next millisecond (APP_HOST_MS, plat_host.c's hook). */
#define TYPHOON_SIM 1
#define APP_HOST_MS 1
static void app_host_ms(void);
#include "emu_web.c"

#define EXPORT_AS(n) __attribute__((used, export_name(#n)))   /* (wasm32-wasi: an export by its name) */

static uint32_t sim_booted;
static uint8_t tsx_in[300];
static uint32_t tsx_in_n;
static int32_t sim_peak_q;

static void app_host_ms(void)                    /* a SysEx frame in (sim_sx_in): the main loop takes it */
{
    if (tsx_in_n) {
        uint32_t n = tsx_in_n;
        tsx_in_n = 0;
        app_sysex(tsx_in, n);
    }
}

EXPORT_AS(sim_init) int sim_init(uint32_t rate)               /* 44100 only (the engines' tables) */
{
    if (rate != FS)
        return -1;
    if (!sim_booted) {
        master = 4096u;                            /* MASTER full: the render at unity (APP_GAIN 4096) */
        boot();
        sim_booted = 1;
    }
    return 0;
}
EXPORT_AS(sim_rate) uint32_t sim_rate(void) { return FS; }
EXPORT_AS(sim_out_l) float *sim_out_l(void) { return out_l; }
EXPORT_AS(sim_out_r) float *sim_out_r(void) { return out_r; }
EXPORT_AS(sim_render) void sim_render(uint32_t n)               /* n frames (<= 1024) into sim_out_l / sim_out_r, -1..1 */
{
    uint32_t i;
    if (!sim_booted)
        return;
    web_render(n);
    for (i = 0; i < n && i < 1024u; i++) {        /* the loudest, Q15 (sim_peak) */
        int32_t vl = (int32_t)(fabsf(out_l[i]) * 32768.0f), vr = (int32_t)(fabsf(out_r[i]) * 32768.0f);
        if (vl > sim_peak_q) sim_peak_q = vl;
        if (vr > sim_peak_q) sim_peak_q = vr;
    }
}
EXPORT_AS(sim_ms) uint32_t sim_ms(void) { return now_ms; }
EXPORT_AS(sim_peak) int32_t sim_peak(void) { int32_t p = sim_peak_q; sim_peak_q = 0; return p; }   /* the loudest since (Q15) */
EXPORT_AS(sim_master) void sim_master(uint32_t k10)             /* the MASTER knob, 0..1023 (1024: unity) */
{
    master = k10 >= 1024u ? 4096u : k10 * 4u;
}

/* ---- the panel */
EXPORT_AS(sim_key) void sim_key(uint32_t k, uint32_t down)   /* the 27 keys: 0 = F3 .. 26 = G5 */
{
    if (k >= NKEYS) return;
    held_keys = down ? held_keys | 1u << k : held_keys & ~(1u << k);
}
EXPORT_AS(sim_button) void sim_button(uint32_t b, uint32_t down)   /* plat.h B_* (app.c BTN_*) */
{
    if (b >= NB) return;
    held_btn = down ? held_btn | 1u << b : held_btn & ~(1u << b);
}
EXPORT_AS(sim_enc) void sim_enc(uint32_t e, int32_t steps)   /* plat.h EN_* (app.c ENC_*); + = clockwise */
{
    if (e < NE) enc_acc[e] += steps;
}
EXPORT_AS(sim_midi) void sim_midi(uint32_t st, uint32_t d1, uint32_t d2)   /* a MIDI message in, over USB */
{
    if (st < 0x80u || st == 0xF0u || st == 0xF7u || st == 0xFEu || mi_w - mi_r >= MQ) return;
    min_q[mi_w % MQ] = (st >> 4) | st << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24;   /* (USB-MIDI's: cable 0) */
    min_port[mi_w++ % MQ] = PORT_USB;
}
static uint16_t fb_plain[240 * 240];
EXPORT_AS(sim_fb) uint16_t *sim_fb(void)         /* 240 x 240 RGB565 (the host keeps it byte-swapped, as the panel takes it) */
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++)
        fb_plain[i] = (uint16_t)(fb[i] >> 8 | fb[i] << 8);
    return fb_plain;
}
EXPORT_AS(sim_fb_hash) uint32_t sim_fb_hash(void)   /* changes when the screen does */
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < 240u * 240u; i++)
        h = (h ^ fb[i]) * 16777619u;
    return h;
}
/* the lights as typhoon asks for them (app.c tp_lights: lit, and glowing when not lit; fm1-base's own record leaves the
 * lit ones out of the glowing) */
EXPORT_AS(sim_leds_buttons) uint32_t sim_leds_buttons(void) { return tp_lit_b; }   /* bit B_* lit */
EXPORT_AS(sim_leds_keys) uint32_t sim_leds_keys(void) { return tp_lit_k; }     /* bit k: key k lit */
EXPORT_AS(sim_glow_keys) uint32_t sim_glow_keys(void) { return tp_dim_k; }     /* bit k: key k glows */

/* ---- the flash: fm1-base's simulated data area (the presets' store at 0x97000: core/presets.c) */
EXPORT_AS(sim_store) uint8_t *sim_store(void) { data_init(); return &data_area[STORE_AT - DATA_BASE]; }
EXPORT_AS(sim_store_size) uint32_t sim_store_size(void) { return STORE_LEN; }
EXPORT_AS(sim_store_hash) uint32_t sim_store_hash(void)   /* changes when a save does (the page keeps the store) */
{
    const uint8_t *s = sim_store();
    uint32_t h = 2166136261u, i;
    for (i = 0; i < STORE_LEN; i++)
        h = (h ^ s[i]) * 16777619u;
    return h;
}

/* ---- parameters by handle (core/params.c pref_t: LN(n, i) = n << 8 | i, SP(i) = 0x4000 | i, G(i) = 0x8000 | i,
 * SG(i) = 0xC000 | i) */
EXPORT_AS(sim_par_set) void sim_par_set(uint32_t r, int32_t v) { par_set((pref_t)r, v); }
EXPORT_AS(sim_par_get) int32_t sim_par_get(uint32_t r) { return par_get((pref_t)r); }
EXPORT_AS(sim_nengines) uint32_t sim_nengines(void) { return NENGINES; }
EXPORT_AS(sim_par_text) const char *sim_par_text(uint32_t r)   /* a parameter's value as the screen shows it */
{
    static char s[32];
    par_text((pref_t)r, s);
    return s;
}
EXPORT_AS(sim_engine_name) const char *sim_engine_name(uint32_t i) { return host_engine_name(i); }   /* engine i of this build, 0 past the last */
EXPORT_AS(sim_factory_name) const char *sim_factory_name(uint32_t k) { return host_factory_name(k); }   /* factory preset k, 0 past the last */
/* layer l's engine's preset k (the engines' own presets, as factory presets use them): its knobs set; its name, or 0
 * past the last */
EXPORT_AS(sim_engine_preset) const char *sim_engine_preset(uint32_t l, uint32_t k)
{
    const engine_t *e;
    uint32_t i;
    if (l >= NLAYER || trk[l].p[P_ENGINE] == ENG_ID_OFF) return 0;
    e = layer_engine(l);
    if (k >= e->npresets) return 0;
    for (i = 0; i < NEPAR; i++) trk[l].p[P_E0 + i] = e->presets[k].e[i];
    par_dirty++;
    return e->presets[k].name;
}
EXPORT_AS(sim_note) int32_t sim_note(void) { return perf_ui.gate ? perf_ui.note : -1; }   /* the note that sounds, -1 none */

/* ---- presets, the UI */
EXPORT_AS(sim_preset) void sim_preset(uint32_t k) { host_preset(k); }   /* load preset slot k (255: INIT) */
EXPORT_AS(sim_preset_cur) uint32_t sim_preset_cur(void) { return ps.cur; }
EXPORT_AS(sim_preset_edited) uint32_t sim_preset_edited(void) { return ps_edited; }
EXPORT_AS(sim_preset_user) uint32_t sim_preset_user(uint32_t k) { return k < NPRESET ? ps_ix[k].user : 0; }
/* the firmware's counts: 0 P_COUNT, 1 S_COUNT, 2 G_COUNT, 3 NPAGES, 4 NGROUP (tools/consts.mjs checks its own) */
EXPORT_AS(sim_count) uint32_t sim_count(uint32_t w)
{
    static const uint32_t N[] = {P_COUNT, S_COUNT, G_COUNT, NPAGES, NGROUP};
    return w < NELEM(N) ? N[w] : 0;
}
EXPORT_AS(sim_preset_name) const char *sim_preset_name(uint32_t k) { return ps_ix[k % NPRESET].name; }
/* SysEx in / out (core/sysex.c): the frame (the bytes between F0 and F7) at sim_sx_in_buf, then sim_sx_in(n); the
 * next millisecond answers into sim_sx_out_buf, sim_sx_out_n bytes (F0 .. F7; 0: none yet) */
EXPORT_AS(sim_sx_in_buf) uint8_t *sim_sx_in_buf(void) { return tsx_in; }
EXPORT_AS(sim_sx_in) void sim_sx_in(uint32_t n) { tsx_in_n = n <= sizeof tsx_in ? n : 0; tp_sx_out_n = 0; }
EXPORT_AS(sim_sx_out_buf) uint8_t *sim_sx_out_buf(void) { return tp_sx_out; }
EXPORT_AS(sim_sx_out_n) uint32_t sim_sx_out_n(void) { return tp_sx_out_n; }
EXPORT_AS(sim_own) uint32_t sim_own(void) { return s_own; }  /* bit GR_*: the preset's own sound groups */
EXPORT_AS(sim_page) void sim_page(uint32_t pg) { host_page(pg); }
EXPORT_AS(sim_layer_amp) uint32_t sim_layer_amp(uint32_t l) { return l < NLAYER ? (uint32_t)perf_ui.amp[l] : 0; }
EXPORT_AS(sim_page_cur) uint32_t sim_page_cur(void) { return ui.page; }
EXPORT_AS(sim_bend16) int32_t sim_bend16(void) { return pf.bend16; }   /* the bend now, 1/16 semitone */
EXPORT_AS(sim_bend_fine) int32_t sim_bend_fine(void) { return pf.bend_fine; }
/* Bluetooth (fm1-base's host stand-in: "BLE MIDI" found half a second into a scan, connected 0.3 s after; app/ble.c):
 * 0 plat_ble_state, 1 plat_ble_device, 2 advertising wanted (plat_ble_advertise), 3 a computer connected, 4 the list
 * up, 5 its entry */
EXPORT_AS(sim_ble) int32_t sim_ble(uint32_t i)
{
    const int32_t v[6] = {plat_ble_state(), plat_ble_device(), host_ble_adv, ble_conn, bt.picking, ui.pick_idx};
    return i < 6u ? v[i] : 0;
}
/* 0: the device goes away (v ignored); 1: a computer connects (v 1) or goes (v 0) */
EXPORT_AS(sim_ble_set) void sim_ble_set(uint32_t what, int32_t v)
{
    if (what == 0u)
        bdev_state = BLE_DEV_OFF;
    else if (what == 1u)
        ble_conn = v != 0 && host_ble_adv;
}
/* the page's words: 0 BLE-P's state, 1 BLE-C's, 2 the remembered device, 3 the newest log line */
EXPORT_AS(sim_ble_text) const char *sim_ble_text(uint32_t i)
{
    static char s[48];
    s[0] = 0;
    if (i == 0u)
        ble_p_status(s);
    else if (i == 1u)
        ble_c_status(s);
    else if (i == 2u)
        name_set(s, ble_ui_remembered(), 40);
    else if (i == 3u)
        name_set(s, ble_ui_log(0), 40);
    return s;
}
/* USB AUDIO as the platform was last asked (fm1-base's host plat_usb_audio_set, patches/fm1-base/0001): 0 rec, 1 its
 * level Q12, 2 play, 3 its level, 4 from (USB_REC_*); 5 how many times it was asked */
EXPORT_AS(sim_usb_audio) int32_t sim_usb_audio(uint32_t i)
{
    return i < 5u ? host_usb_audio[i] : i == 5u ? (int32_t)host_usb_audio_calls : 0;
}
