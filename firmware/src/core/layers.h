/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* typhoon's layers, the host side of the engine interface (engines/felucca_engine.h): what an engine sees.
 *
 * Up to NLAYER layers play on ONE note and breath stream (a stack, not polyphony). Each layer is a Felucca
 * "track" with one mono voice: its parameters p[] (the common ones below, then the engine's sixteen P_E0..P_E15)
 * and the voice. core/layers.c drives them from the performance (notes, legato, breath, bend); core/params.c
 * describes every parameter for the UI, MIDI CC and storage.
 *
 * Three kinds of parameters:
 *   P_*  a layer's (in the preset)
 *   S_*  the sound's, after the layers are mixed: the breath curve, the LP, drive, delay, reverb. In groups
 *        (GR_*): each group has a GLOBAL copy (sglob, the GLO pages), and a preset either uses it or has its
 *        OWN copy of the whole group (s_own bit, sown). sv() is the value that plays.
 *   G_*  global only (not in a preset): breath source, bend range, transpose, the layer the UI is on */

/* NLAYER, NPART, NVOICE: core/sizes.h (felucca_engine.h needs them first) */

/* a layer's parameters. The engine interface needs P_ATK, P_REL, P_FTYPE and P_E0..P_E15 by these names.
 * New ones go just before P_E0: stored presets keep their count and are mapped by it (core/presets.c) */
enum {
    P_ENGINE,                        /* the engine's stable id (engines.h ENG_ID_*) */
    P_LEVEL,                         /* 0 = the layer is off */
    P_MUTE,                          /* muted: fades out (P_REL), keeps its level; unmuted during a note: in at once */
    P_TRANS, P_FINE, P_PAN,
    P_ATK, P_REL,                    /* the volume envelope's attack and release (env_ms: 0 .. ~4 s) */
    P_FTYPE,                         /* filter type, for engines that have one (ANALOG): LP BP HP */
    P_BR_AMP,                        /* breath -> loudness: 0 = the note at full level, 127 = all breath */
    P_BR_FLT,                        /* breath -> the engine's own filter / brightness (vmod.cutoff), bipolar */
    P_BR_SHP,                        /* breath -> shape (vmod.shape: PWM, colour, 2ND ...), bipolar */
    P_DELAY, P_DEC, P_SUS,           /* the volume envelope (with P_ATK, P_REL): delay, decay, sustain level. Restarts on a
                                      * fresh attack only (legato runs on): a layer can come in late (a pad), or only
                                      * at the start (a chiff of noise) */
    P_TONE, P_TONE_A, P_TONE_B, P_TONE_C,   /* the TONE stage after the engine (core/tone.c): its type, three knobs
                                      * whose meaning follows the type */
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
    P_E8, P_E9, P_E10, P_E11, P_E12, P_E13, P_E14, P_E15,
    P_COUNT
};
#define NCOMMON P_E0
#define NEPAR 16u                                  /* an engine's parameters (EDIT 1 .. 4); 8 until 2026-10-21 */

typedef struct track {
    int16_t p[P_COUNT];
    voice_t v[NVOICE];
} track_t;
static track_t trk[NLAYER];

/* the sound's parameters (global, or the preset's own: see the top). New ones go at the end (presets map by index) */
enum {
    S_BC_LO, S_BC_HI,                /* GR_CURVE: CC2 below LO counts as nothing, from HI on as full; LO below 0: a head start */
    S_BC_SHAPE,                      /*   and between: -64 log (fast at first) .. 0 linear .. 63 exponential */
                                     /*   (S_BSMOOTH was here until 2026-10-23: now G_BSMOOTH, global only) */
    S_LPF, S_LPRES, S_BR_LP,         /* GR_LP: the low-pass on the mix: cutoff at no breath, resonance, breath opens it */
    S_DRIVE, S_DRV_AT,               /* GR_DRIVE: overdrive by breath: amount (0 = off), the breath where it starts */
    S_DLY_TIME, S_DLY_FB, S_DLY_TONE, S_DLY_MIX,     /* GR_DELAY */
    S_REV_SIZE, S_REV_DECAY, S_REV_DAMP, S_REV_MIX,  /* GR_REVERB */
    S_DT_CENTS, S_DT_DELAY, S_DT_WIDTH, S_DT_MIX,    /* GR_DETUNE: a copy up and one down by CENTS, after DELAY ms */
    S_REV_TYPE, S_REV_PRE,           /* GR_REVERB too: PLATE / HALL / OLD (core/fx.c), the predelay, ms */
    S_VOLUME,                        /* GR_VOLUME: the whole sound, after the effects: 100 = as it is */
    S_COUNT
};
enum { GR_CURVE, GR_LP, GR_DRIVE, GR_DELAY, GR_REVERB, GR_DETUNE, GR_VOLUME, NGROUP };
static const uint8_t S_GROUP[S_COUNT] = {
    GR_CURVE, GR_CURVE, GR_CURVE, GR_LP, GR_LP, GR_LP, GR_DRIVE, GR_DRIVE,
    GR_DELAY, GR_DELAY, GR_DELAY, GR_DELAY, GR_REVERB, GR_REVERB, GR_REVERB, GR_REVERB,
    GR_DETUNE, GR_DETUNE, GR_DETUNE, GR_DETUNE, GR_REVERB, GR_REVERB, GR_VOLUME};
static int16_t sglob[S_COUNT], sown[S_COUNT];
static volatile uint8_t s_own;                     /* bit GR_x: the preset has its own copy of group x */
static inline int32_t sv(uint32_t i) { return (s_own >> S_GROUP[i] & 1u) ? sown[i] : sglob[i]; }

/* global parameters (not in a preset) */
enum {
    G_BSRC,                          /* where loudness comes from: AUTO (CC2 when it comes, else velocity), CC2, VEL */
    G_BEND,                          /* pitch bend range, semitones */
    G_XPOSE,                         /* transpose, semitones: every note (MIDI in, the keys) */
    G_BLE_P,                         /* BLE-P: OFF, ON: a computer finds the FM-1 and connects (app/ble.c; G_BT, OFF /
                                      * PERIPH / BOTH, until 2026-10-24: PERIPH and BOTH read as ON). BLE builds only */
    G_LAYER,                         /* the layer the UI is on */
    G_TILT_CC,                       /* tilt -> pitch bend: the controller (the re.corder's rotation: CC 52, centre 64); 0 off */
    G_TILT_DEAD,                     /*   CC units either side of 64 that count as the centre */
    G_TILT_RANGE,                    /*   the bend at the extremes, % of G_BEND (the ESP32 bridge's CC2PB_RANGE_PCT) */
    G_TILT_INV,                      /*   the other way round */
    G_BREATH_CC,                     /* breath comes on this CC (1 .. 127; 128: channel aftertouch); CC11 by default */
    G_BSMOOTH,                       /* breath smoothing, ms: the one place breath is smoothed (the CC's 128 steps, a new
                                      * value every few ms: zipper otherwise). Global only, no preset or engine adds its
                                      * own (no hidden lags) */
    G_LEAP_MODE,                     /* LEAPS (core/leap.c): passing notes on slurs held back: OFF, HOLD */
    G_LEAP_ST,                       /*   the smallest upward slurred jump held back, semitones (13: past an octave) */
    G_LEAP_MS,                       /*   how long it waits for the next note, ms */
    G_USB_IN,                        /* USB AUDIO (app/usbaudio.c): the computer's audio through the FM-1: OFF (no such
                                      * device on the computer), MUTE, ON */
    G_USB_IN_LV,                     /*   its level, % (0 .. 200) */
    G_USB_OUT,                       /*   the FM-1 recorded on the computer: OFF, MUTE, MIX (with the computer's audio
                                      *   in it), SYNTH (typhoon alone) */
    G_USB_OUT_LV,                    /*   its level, % (0 .. 200) */
    G_BLE_C,                         /* BLE-C (app/ble.c): OFF, FIRST (the first Bluetooth MIDI device found), CHOOSE (the
                                      * one chosen in the BLUETOOTH page's list) */
    G_BLE_AUTO,                      /* AUTO_RECONNECT: OFF (one look), ON (looks whenever no device is connected) */
    G_USB_HOST,                      /* USB HOST (app/usbhost.c): OFF (a computer), ON (a USB MIDI device: the re.corder) */
    G_COUNT                          /* (new ones at the end: the saved state has room for G_SAVED) */
};
#define G_SAVED 24                                 /* (16 until 2026-10-24, 12 until 2026-10-23: the state 0x7A08 on) */
static int16_t gpar[G_COUNT];
