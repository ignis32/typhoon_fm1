/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* typhoon on fm1-base: the app's half of the contract with the platform (base/device/unity.c on the FM-1,
 * base/host/plat_host.c on a computer). typhoon itself is in firmware/src; app.c includes it. */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "TYPHOON"
#define APP_TAGLINE "the voice of the re.corder"
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz (as Dinghy's halves) */
#define APP_CANVAS_PX (240u * 48u)       /* 23 KB: taller boxes are drawn in strips (app.c TP_BOX; Dinghy's canvas
                                          * was 240 x 120, 58 KB) */
#define APP_LOOP                         /* every pass of the main loop: the keys, MIDI in, LEAPS (app_loop) */
#define APP_SYSEX                        /* the presets and the editor over USB SysEx (core/sysex.c) */
#define FM1B_UAC_DEEP 1                  /* USB audio recording: a half more in hand (a 4 KB ring in the pool, +5.8 ms):
                                          * typhoon's renders take past the ~40 % of their time the shallow ring
                                          * lasts through, and a dry ring repeats its last frame, a regular crackle
                                          * in the recording (felucca-bsp chip/usb.c) */
#define APP_BLE_AT_BOOT (typhoon_ble_at_boot())   /* Bluetooth at power-on: unless GLOBAL > BLUETOOTH is OFF */
int typhoon_ble_at_boot(void);

/* the engines this build has (engines/engines.h): ./build.sh --engines (build/engines/typhoon_engines.h) or -D flags
 * (tools/engine_flags.sh), else all of them (profiles/default.txt) */
#if __has_include("typhoon_engines.h")
#include "typhoon_engines.h"
#endif
#if !(defined TYPHOON_ENG_FM || defined TYPHOON_ENG_ANALOG || defined TYPHOON_ENG_ERS || defined TYPHOON_ENG_EVA)
#define TYPHOON_ENG_FM 1
#define TYPHOON_ENG_ANALOG 1
#define TYPHOON_ENG_ERS 1
#define TYPHOON_ENG_EVA 1
#endif
#define TYPHOON_BLE FM1B_BLE
#ifdef PLAT_WEB
#define APP_WEB                          /* the browser emulator (./build.sh --web): app/web.c's exports, the presets */
#endif

/* the store's objects (plat.h): typhoon's state (the globals, the global sound, what plays) in Felucca's settings pair;
 * in the data area, above the presets (0x97000 .. 0xAFFFF, raw sectors: core/presets.c), the panel's calibration,
 * Bluetooth's address and pairings (both fm1-base's) and the Bluetooth device typhoon remembers (app/ble.c) */
enum { OBJ_STATE, OBJ_PANEL, OBJ_BLE, OBJ_BTDEV, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u, 0xDA000u}

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the saved state (or the first preset) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input, the lights and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen (and to a screen reader) */
void app_loop(void);                     /* every pass of the main loop */
int app_sysex(const uint8_t *p, uint32_t n);  /* a SysEx frame from USB (between F0 and F7): 1 = typhoon's */
