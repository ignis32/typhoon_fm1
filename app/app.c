/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* typhoon on fm1-base: the app. Part of fm1-base's unity build, after the platform (base/device/unity.c on the FM-1,
 * base/host/plat_host.c on a computer), so the platform's drawing is in scope: gfx.c's cv_*, text_w, palette_set,
 * the colours, the fonts.
 *
 * typhoon itself (firmware/src) comes in below, in this order: the layers, engines, presets, UI, SysEx. What it
 * asked of its old platform (Dinghy's) is given first, on plat.h: the "tp" part. Then the platform's callbacks:
 *   app_init        the saved state, or the first preset
 *   app_render      the audio interrupt: the layers and the mix, then MASTER and 24 bits
 *   app_loop        every pass of the main loop: the keys as notes, MIDI in through LEAPS (core/leap.c)
 *   app_frame       ~60 a second: the UI's input, the lights, the screen, the state saved when quiet
 *   app_sysex       the presets and the editor over USB SysEx (core/sysex.c) */

/* ---------------------------------------------------------------- tp: what typhoon asks of its platform --- */
#ifndef FS
#define FS 44100u                                  /* (the device's main_fm1.c has it; the host not) */
#endif
#define fm1_ms (plat_ms())                         /* milliseconds since boot (typhoon's name for it; undefined at
                                                    * the end: the platform's own fm1_ms comes after the app) */
#ifndef RING_PUBLISH                              /* (the device's unity.c has it; the host not) */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* a ring's entry written before its index moves */
#endif
#define APP_VER APP_VERSION                       /* (ms_since: ui/ui.c's) */

/* the panel by typhoon's names (ui/layout.h), as fm1-base numbers them (plat.h B_*, EN_*: the numbers written out,
 * for tools/consts.mjs, and checked) */
enum { BTN_FX = 0, BTN_SEL = 1, BTN_ENV = 2, BTN_LFO = 3, BTN_EDIT = 4, BTN_GLO = 5, BTN_HOME = 6, BTN_SAVE = 7,
       BTN_ARP = 8, BTN_SEQ = 9, BTN_PLAY = 10, BTN_REC = 11, BTN_OCT_DOWN = 12, BTN_OCT_UP = 13 };
enum { ENC_SELECT = 0, ENC_ALGORITHM = 1, ENC_PRESETS = 2, ENC_KNOB1 = 3, ENC_KNOB2 = 4, ENC_KNOB3 = 5, ENC_KNOB4 = 6 };
_Static_assert(BTN_FX == B_FX && BTN_SEL == B_SEL && BTN_ENV == B_ENV && BTN_LFO == B_LFO && BTN_EDIT == B_EDIT &&
               BTN_GLO == B_GLO && BTN_HOME == B_HOME && BTN_SAVE == B_SAVE && BTN_ARP == B_ARP && BTN_SEQ == B_SEQ &&
               BTN_PLAY == B_PLAY && BTN_REC == B_REC && BTN_OCT_DOWN == B_OCTDN && BTN_OCT_UP == B_OCTUP,
               "typhoon's button names: plat.h's numbers");
_Static_assert(ENC_SELECT == EN_SELECT && ENC_ALGORITHM == EN_ALGO && ENC_PRESETS == EN_PRESET && ENC_KNOB1 == EN_K1 &&
               ENC_KNOB2 == EN_K2 && ENC_KNOB3 == EN_K3 && ENC_KNOB4 == EN_K4, "typhoon's encoder names: plat.h's");

/* input: what is held (tp_in.buttons: bit BTN_*, tp_in.notes: bit k = key k, 0 = F3), the buttons' edges since the
 * last tp_edges, the encoders' detents */
static struct { uint32_t buttons, notes, pressed, released; } tp_in;
static void tp_scan(void)
{
    uint32_t b = plat_buttons();
    tp_in.pressed |= b & ~tp_in.buttons;
    tp_in.released |= tp_in.buttons & ~b;
    tp_in.buttons = b;
    tp_in.notes = plat_keys();
}
static uint32_t tp_edges(uint32_t *released)
{
    uint32_t p = tp_in.pressed;
    *released = tp_in.released;
    tp_in.pressed = tp_in.released = 0;
    return p;
}
static int32_t tp_enc(uint32_t e) { return plat_enc((int)e); }

/* a box taller than the canvas (APP_CANVAS_PX: 48 rows of 240, as fm1-base's WiFi builds) is drawn in strips: its
 * drawing runs once a strip, each strip keeping its own rows (gfx.c clips to cv_y0 .. cv_y1, after moving by cv_oy),
 * and goes to the screen at its place. TP_BOX(w, h, bg) { the drawing ... cv_blit(x, y + tp_box.top); } (state the
 * drawing moves along, a curve's last point, starts again inside the braces) */
static struct { uint32_t w, h, top, sh; uint16_t bg; } tp_box;
static int tp_box_strip(uint32_t top)
{
    uint32_t rows = APP_CANVAS_PX / tp_box.w;
    if (top >= tp_box.h)
        return 0;
    tp_box.top = top;
    tp_box.sh = tp_box.h - top < rows ? tp_box.h - top : rows;
    cv_begin(tp_box.w, tp_box.sh, tp_box.bg);
    cv_oy = -(int32_t)top;                         /* the box's row y is the strip's row y - top */
    return 1;
}
static int tp_box_first(uint32_t w, uint32_t h, uint16_t bg)
{
    tp_box.w = w;
    tp_box.h = h;
    tp_box.bg = bg;
    return tp_box_strip(0);
}
#define TP_BOX(w, h, bg) for (int tp_ok = tp_box_first((w), (h), (bg)); tp_ok; tp_ok = tp_box_strip(tp_box.top + tp_box.sh))

/* the lights: an id is a button (BTN_*) or a key (KEY_LED(k)); lit, or glowing (dim) when not lit. They go out to
 * the panel once a frame (tp_lights) */
#define KEY_LED(k) (14u + (k))
static uint32_t tp_lit_b, tp_lit_k, tp_dim_b, tp_dim_k;
static void tp_bit(uint32_t *b, uint32_t *k, uint32_t id, int on)
{
    uint32_t *m = id < 14u ? b : k, bit = 1u << (id < 14u ? id : id - 14u);
    *m = on ? *m | bit : *m & ~bit;
}
static void tp_led(uint32_t id, int on) { tp_bit(&tp_lit_b, &tp_lit_k, id, on); }
static void tp_glow(uint32_t id, int on) { tp_bit(&tp_dim_b, &tp_dim_k, id, on); }
static void tp_lights(void)
{
    static uint32_t last[4] = {~0u};
    if (last[0] == tp_lit_b && last[1] == tp_lit_k && last[2] == tp_dim_b && last[3] == tp_dim_k)
        return;
    last[0] = tp_lit_b, last[1] = tp_lit_k, last[2] = tp_dim_b, last[3] = tp_dim_k;
    plat_lights(tp_lit_b, tp_lit_k, tp_dim_b, tp_dim_k, 1);
}

/* the state (core/presets.c state_load / _save): a store object in Felucca's settings pair (0: loaded / saved) */
static int settings_load(void *p, uint32_t n) { return plat_store_load(OBJ_STATE, p, n) == (int)n ? 0 : -1; }
static int settings_save(const void *p, uint32_t n) { return plat_store_save(OBJ_STATE, p, n) ? -1 : 0; }

/* the presets' store (core/presets.c): 25 raw sectors of the data area at 0x97000, where they always were (offsets
 * from there; 0: done) */
#define STORE_AT 0x97000u
#define STORE_LEN (25u * 4096u)
static int store_read(uint32_t off, void *p, uint32_t n)
{
    const uint8_t *s = off + n <= STORE_LEN ? plat_data(STORE_AT + off) : 0;
    uint32_t i;
    if (!s)
        return -1;
    for (i = 0; i < n; i++)
        ((uint8_t *)p)[i] = s[i];
    return 0;
}
static int store_erase(uint32_t off) { return off < STORE_LEN && !(off & 0xFFFu) ? plat_data_erase(STORE_AT + off) : -1; }
static int store_write(uint32_t off, const void *p, uint32_t n)
{
    return off + n <= STORE_LEN ? plat_data_write(STORE_AT + off, p, n) : -1;
}

/* SysEx out (core/sysex.c's answers): over USB; the simulator (web/sim/typhoon_sim.c) keeps the last one too */
#ifdef TYPHOON_SIM
static uint8_t tp_sx_out[300];
static uint32_t tp_sx_out_n;
static void tp_sysex_send(const uint8_t *f, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n && i < sizeof tp_sx_out; i++)
        tp_sx_out[i] = f[i];
    tp_sx_out_n = i;
    plat_sysex_send(f, n);
}
#else
#define tp_sysex_send plat_sysex_send
#endif

/* MIDI out (the keys): USB, and Bluetooth (whoever listens) */
static void midi_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    plat_midi_send((st >> 4) | st << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24, TO_USB | TO_BLE);
}

/* ------------------------------------------------------------------------------------- typhoon itself --- */
#include "core/mul64.h"                         /* (64-bit multiplies out of line: the CPU's fault, bsp docs/CPU.md) */
#include "fe_tables.h"
#include "core/sizes.h"
#include "engines/felucca_engine.h"
#include "core/layers.h"
#define clamp tp_clamp                             /* (dsp.c's own, as plat.h's: the same; Felucca-Engines' file kept) */
#include "dsp/dsp.c"
#include "dsp/dspx.c"                              /* (typhoon's fixed-point shared parts: the SVF's step, the vowels, ...) */
#include "dsp/dspf.c"                              /* (its float twin: maths, the filter) */
#include "dsp/oscf.c"                              /* (the float oscillators and noises, every one as loud) */
#include "dsp/breathlp.c"                          /* (an engine's breath LP: ERS's, FM's) */
#include "engines/engines.h"
#include "core/tone.c"
#include "core/layers.c"
#include "core/leap.c"
#include "core/fx.c"
#include "core/reverb.c"
#include "core/params.c"
#include "core/presets.c"
#include "ble.c"                                   /* app/ble.c: Bluetooth (BLE-P, BLE-C) for the UI, on plat_ble_* */
#include "usbaudio.c"                              /* app/usbaudio.c: GLOBAL > USB AUDIO, on plat_usb_audio_set */
#include "usbhost.c"                               /* app/usbhost.c: GLOBAL > USB HOST, on plat_usb_host */
#include "ui/layout.h"
#include "ui/ui.c"
#include "core/sysex.c"

#define KEY0 53u                                   /* key 0 = F3 (transpose moves every note: core/layers.c) */

/* the key LEDs: a bar of the breath while CC2 moves, else the key of the note that sounds */
static void key_leds(uint32_t t_cc2)
{
    uint32_t k;
    if (t_cc2 && ms_since(t_cc2) < 1500u) {
        uint32_t n = ((uint32_t)perf_ui.breath * 27u + 16384u) >> 15;   /* lit keys, from F3 up */
        for (k = 0; k < 27u; k++) {
            tp_led(KEY_LED(k), k < n);
            tp_glow(KEY_LED(k), 1);
        }
        return;
    }
    for (k = 0; k < 27u; k++) {
        tp_led(KEY_LED(k), perf_ui.gate && perf_ui.note == KEY0 + k);
        tp_glow(KEY_LED(k), 0);
    }
}

/* tilt -> pitch bend (GLO, TILT>BEND): the re.corder sends its rotation as a CC centred on 64 (CC 52), no bend. A dead
 * zone either side of 64 is the centre; past it the bend ramps from the centre (no jump) to G_TILT_RANGE % of the full bend at the extremes. The
 * bend then goes the BEND range (G_BEND) as any. 14 bits out (8192 centre) */
static uint32_t tilt_bend(uint32_t cc)
{
    int32_t d = (int32_t)cc - 64, dz = gpar[G_TILT_DEAD], r = gpar[G_TILT_RANGE], pb;
    if (gpar[G_TILT_INV])
        d = -d;
    if (d >= -dz && d <= dz)
        pb = 8192;
    else if (d > 0)                                /* (no 64-bit division: 63 x 100 x 8191 fits) */
        pb = 8192 + clamp(d - dz, 0, 63 - dz) * r * 8191 / ((63 - dz) * 100);
    else
        pb = 8192 - clamp(-d - dz, 0, 64 - dz) * r * 8192 / ((64 - dz) * 100);
    return (uint32_t)clamp(pb, 0, 16383);
}

/* MIDI in, from USB, TRS or Bluetooth (any channel) */
static uint32_t t_cc2;                             /* CC2 came last when (the key LEDs' breath bar) */
static void midi_in(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t ty = st & 0xF0u;
    if (ty == 0xB0u && gpar[G_TILT_CC] && d1 == (uint32_t)gpar[G_TILT_CC]) {   /* the tilt: a bend instead */
        uint32_t pb = tilt_bend(d2);
        perf_bend(pb & 0x7Fu, pb >> 7);
        return;
    }
    if ((ty == 0xB0u && d1 == (uint32_t)gpar[G_BREATH_CC]) || (ty == 0xD0u && gpar[G_BREATH_CC] == 128)) {
        perf_breath(ty == 0xD0u ? d1 : d2);        /* breath: the BREATH CC (CC2 by default), or aftertouch */
        t_cc2 = fm1_ms | 1u;
        return;
    }
    if (ty == 0x90u) leap_note_on(d1, d2);         /* (LEAPS: a passing note on a slur held back, core/leap.c) */
    else if (ty == 0x80u) leap_note_off(d1);
    else if (ty == 0xE0u) perf_bend(d1, d2);
    else if (ty == 0xB0u && (d1 == 120u || d1 == 123u)) { leap_reset(); perf_all_off(); }
    else if (ty == 0xB0u) ui_cc(d1, d2);
}

/* ---------------------------------------------------------------------------- the platform's callbacks --- */
static struct {
    uint32_t keys, typed;                          /* the keys held at the last pass; the ones SAVE AS typed with */
    uint32_t saved, t_change;                      /* par_dirty when last saved; when it moved since (0: not) */
    int32_t xpose;
    uint8_t up;                                    /* app_ui_init done: the loop may play */
} app;

void app_init(void)
{
    state_load();
    ua_init();                                     /* USB AUDIO as saved, before USB starts (app/usbaudio.c) */
    bt_init();                                     /* BLE-P as saved, before the radio starts (app/ble.c) */
    app.saved = par_dirty;
    app.xpose = gpar[G_XPOSE];
}

int typhoon_ble_at_boot(void) { return gpar[G_BLE_P] != 0 || gpar[G_BLE_C] != 0; }   /* (BLE-P or BLE-C on) */

/* the audio interrupt: the layers and the mix (core/layers.c, core/fx.c) in Q15, then MASTER as the old platform did it
 * (the same curve: APP_GAIN, m * m >> 12), the device's clip, and 24 bits */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i;
    for (i = 0; i < 2u * n; i++)                   /* (layers_audio adds: Dinghy cleared the block before) */
        lr[i] = 0;
    layers_audio(lr, n);
    for (i = 0; i < 2u * n; i++) {
        int32_t v = (lr[i] * (int32_t)master_q12) >> 12;
        lr[i] = clamp(v, -32767, 32767) << 8;
    }
}

void app_panic(void) { perf_all_off(); }

void app_ui_init(void)
{
    static const uint16_t DX[5] = {RGB(16, 46, 46), RGB(26, 90, 92), RGB(40, 150, 152), RGB(56, 210, 212),
                                   RGB(66, 245, 245)};   /* typhoon's palette, sloopDX's cyan ("DX": Dinghy's 6th) */
    uint32_t k;
    for (k = 0; k < 5u; k++)
        pal[k] = DX[k];
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.full = 1;
    ui_msg(APP_NAME " " APP_VER);                  /* the version, a moment in the header */
    app.up = 1;
}

void app_loop(void)
{
    uint32_t keys, down, up, k, pkt;
    int port;
    if (!app.up)
        return;
    tp_scan();
    keys = tp_in.notes;                            /* bit k: key k (0 = F3 .. 26 = G5) */
    down = keys & ~app.keys;
    up = app.keys & ~keys;
    app.keys = keys;
    for (k = 0; k < 27u; k++) {                    /* SAVE AS: the keys type (ui.c), and their let-go is no note */
        if (down >> k & 1u) {
            if (ui_typing()) { ui_type(k); app.typed |= 1u << k; }
            else { perf_note_on(KEY0 + k, 100); midi_send(0x90, KEY0 + k, 100); }
        }
        if (up >> k & 1u) {
            if (app.typed >> k & 1u) app.typed &= ~(1u << k);
            else { perf_note_off(KEY0 + k); midi_send(0x80, KEY0 + k, 0); }
        }
    }
    leap_poll();                                   /* (a held jump whose wait is over plays now) */
    while (plat_midi_in(&pkt, &port))              /* USB, TRS, Bluetooth: USB-MIDI's packet layout */
        if ((pkt >> 8 & 0xFFu) >= 0x80u)
            midi_in(pkt >> 8 & 0xFFu, pkt >> 16 & 0x7Fu, pkt >> 24 & 0x7Fu);
    tsx_tick();                                    /* (a write the editor left half way is forgotten) */
}

void app_input_only(void)
{
    tp_scan();
    ui_input();
}

void app_frame(void)
{
    tp_scan();
    ui_input();
    bt_poll();                                     /* Bluetooth's mode and the device it looks for (app/ble.c) */
    ua_poll();                                     /* USB AUDIO: levels at once, to or from OFF once the knob rests */
    uhost_poll();                                   /* USB HOST: on or off once the knob rests; its news on the screen */
    if (gpar[G_XPOSE] != app.xpose) {              /* transposed: the sounding note is cut, the next one is in the */
        app.xpose = gpar[G_XPOSE];                 /* new key (notes are kept by the number that came in) */
        perf_cut();
    }
    key_leds(t_cc2);
    ui_leds();
    tp_lights();
    ui_draw();
    audio_load = plat_cpu_pct() * 10u;             /* (core/sysex.c STATS: the render's load, % x 10) */
    if (audio_load > audio_peak)
        audio_peak = audio_load;
    if (par_dirty != app.saved) {                  /* saved 2 s after the last change, when silent */
        if (!app.t_change)
            app.t_change = fm1_ms | 1u;
        if (ms_since(app.t_change) > 2000u && layers_quiet() && !ble_busy()) {   /* (a flash erase stalls the sound) */
            state_save();
            app.saved = par_dirty;
            app.t_change = 0;
        }
    }
}

void app_say(const char *a, const char *b)
{
    char t[40];
    uint32_t i = 0;
    while (a && *a && i < 38u)
        t[i++] = *a++;
    if (b && *b && i < 38u)
        t[i++] = ' ';
    while (b && *b && i < 38u)
        t[i++] = *b++;
    t[i] = 0;
    ui_msg(t);
}

int app_sysex(const uint8_t *p, uint32_t n) { return tsx_frame(p, n); }

#undef fm1_ms                                      /* (the platform's main_fm1.c, after this file, has its own) */
