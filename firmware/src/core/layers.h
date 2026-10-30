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
                                     /* (P_BR_AMP, P_BR_FLT, P_BR_SHP were here until 2026-10-30: breath -> loudness,
                                      * filter, shape; the routes now, core/routes.c: A -> LEVEL, the knob, EXCITE) */
    P_DELAY, P_DEC, P_SUS,           /* the volume envelope (with P_ATK, P_REL): delay, decay, sustain level. Restarts on a
                                      * fresh attack only (legato runs on): a layer can come in late (a pad), or only
                                      * at the start (a chiff of noise) */
    P_TONE, P_TONE_A, P_TONE_B, P_TONE_C,   /* the TONE stage after the engine (core/tone.c): its type, three knobs
                                      * whose meaning follows the type */
    P_QUIRKS, P_QSET,                /* QUIRKS: each fingered note a little off in pitch and level, the same way every
                                      * time (an instrument's own intonation), 0 .. 100 %; QUIRK SET: which set of
                                      * quirks (core/layers.c layer_quirk) */
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
    P_E8, P_E9, P_E10, P_E11, P_E12, P_E13, P_E14, P_E15,
    P_E16, P_E17, P_E18, P_E19,
    P_COUNT
};
#define NCOMMON P_E0
#define NEPAR 20u                                  /* an engine's parameters (EDIT 1 .. 5); 16 until 2026-10-28, 8
                                                    * until 2026-10-21 (presets.c reads older records by version) */

typedef struct track {
    int16_t p[P_COUNT];
    voice_t v[NVOICE];
} track_t;
static track_t trk[NLAYER];

/* (cc_raw, every controller as it came, and src_breath, the breath for PANFLUTE, were here until 2026-10-30: no engine
 * reads a source now, its routes say what reaches it: layer_in) */

/* the sources (notes/05-controls.md): the controller described once, in GLO > CONTROLS. BEND (MIDI pitch bend, or the
 * tilt's CC: G_TILT_*), A .. D (a CC or aftertouch each, through its own curve, OVER and SMOOTH: G_CC_A ..), and AO ..
 * DO, the top of A .. D past its OVER, 0 .. full from there. By convention (not enforced) A is the breath. Their values
 * as the audio has them, once a control block (core/layers.c sources_tick): Q15, BEND -32768 .. 32767, the rest
 * 0 .. 32767 */
enum { SRC_BEND, SRC_A, SRC_B, SRC_C, SRC_D, SRC_AO, SRC_BO, SRC_CO, SRC_DO, NSRC };
#define NSRC_CC 4u                                 /* A .. D: a controller each */
static int32_t src_val[NSRC];

/* the routes (core/routes.c, notes/05-controls.md): a cell is a source -> a layer's target, an amount -100 .. +100 %;
 * one source to many targets, many to one (they add). The preset's, up to NCELL, each in a slot (amt 0: a free one).
 * Targets: the layer's own (T_LEVEL .. T_EXCITE), then an engine knob each (T_KNOB + k: P_E0 + k, its descriptor's
 * MOD says whether it is offered). Numbers are stable: new ones go before T_KNOB's room or after it, never between */
enum {
    T_LEVEL,                         /* the source's share of the level (BR>AMP's: 100 % all of it, -100 % less as it rises) */
    T_PITCH,                         /* semitones, 100 % = GLOBAL's BEND range at a full source (BEND +-1) */
    T_PAN,                           /* the knob PAN, moved */
    T_TONE_A, T_TONE_B, T_TONE_C,    /* TONE's knobs, moved (the ones its type marks MOD) */
    T_EXCITE,                        /* what breath moves in the engine (vmod.shape: FM's index, ERS's pressure, EVA's
                                      * slots, ANALOG's PWM; PANFLUTE's breath): 100 % = the old BR>SHP 63, all of it */
    T_IN1, T_IN2, T_IN3,             /* the engine's own inputs (engine_t.in: PANFLUTE's LIP, WOBBLE, LFO DEPTH) */
    T_KNOB = 16,                     /* + k: the engine's knob P_E0 + k, moved */
    NTARGET = 36                     /* (T_KNOB + 20 knobs; a number: tools/consts.mjs reads it) */
};
#define NCELL 32u
typedef struct { uint8_t lt, src; int8_t amt; } cell_t;   /* lt: layer << 6 | target */
static cell_t cells[NCELL];
/* each layer's inputs as its routes made them, for an engine that takes them itself (PANFLUTE): [0] EXCITE, [1 .. 3]
 * T_IN1 .. T_IN3, Q15; layer_in_w: their routes' amounts, |%| added (the sources as one: layer_in x 100 / w; an input
 * measured on its source, as PANFLUTE's LFO DEPTH from CENTRE, then scaled by w); layer_in_on bit k: a route reaches
 * [k] (none: the engine keeps its own rest) */
static int32_t layer_in[NLAYER][4];
static int16_t layer_in_w[NLAYER][4];
static uint8_t layer_in_on[NLAYER];

/* the sound's parameters (global, or the preset's own: see the top). New ones go at the end (presets map by index) */
enum {
    /* (GR_CURVE, the breath curve, and GR_LP, the mix's breath LP, were here until 2026-10-30: the curves are the
     * sources' now, global, G_LO_A ..; the LP ERS's and FM's own, dsp/breathlp.c) */
    S_DRIVE, S_DRV_AT,               /* GR_DRIVE: overdrive by breath: amount (0 = off), the breath where it starts */
    S_DLY_TIME, S_DLY_FB, S_DLY_TONE, S_DLY_MIX,     /* GR_DELAY */
    S_REV_SIZE, S_REV_DECAY, S_REV_DAMP, S_REV_MIX,  /* GR_REVERB */
    S_DT_CENTS, S_DT_DELAY, S_DT_WIDTH, S_DT_MIX,    /* GR_DETUNE: a copy up and one down by CENTS, after DELAY ms */
    S_REV_TYPE, S_REV_PRE,           /* GR_REVERB too: PLATE / HALL / OLD (core/fx.c), the predelay, ms */
    S_VOLUME,                        /* GR_VOLUME: the whole sound, after the effects: 100 = as it is */
    S_COUNT
};
enum { GR_DRIVE, GR_DELAY, GR_REVERB, GR_DETUNE, GR_VOLUME, NGROUP };
static const uint8_t S_GROUP[S_COUNT] = {
    GR_DRIVE, GR_DRIVE,
    GR_DELAY, GR_DELAY, GR_DELAY, GR_DELAY, GR_REVERB, GR_REVERB, GR_REVERB, GR_REVERB,
    GR_DETUNE, GR_DETUNE, GR_DETUNE, GR_DETUNE, GR_REVERB, GR_REVERB, GR_VOLUME};
static int16_t sglob[S_COUNT], sown[S_COUNT];
static volatile uint8_t s_own;                     /* bit GR_x: the preset has its own copy of group x */
static inline int32_t sv(uint32_t i) { return (s_own >> S_GROUP[i] & 1u) ? sown[i] : sglob[i]; }

/* global parameters (not in a preset) */
enum {
    G_BSRC,                          /* where A comes from: AUTO (its controller once it comes, else the note-on velocity:
                                      * a keyboard), CC (its controller only), VEL */
    G_BEND,                          /* pitch bend range, semitones */
    G_XPOSE,                         /* transpose, semitones: every note (MIDI in, the keys) */
    G_BLE_P,                         /* BLE-P: OFF, ON: a computer finds the FM-1 and connects (app/ble.c; G_BT, OFF /
                                      * PERIPH / BOTH, until 2026-10-24: PERIPH and BOTH read as ON). BLE builds only */
    G_LAYER,                         /* the layer the UI is on */
    G_TILT_CC,                       /* tilt -> pitch bend: the controller (the re.corder's rotation: CC 52, centre 64); 0 off */
    G_TILT_DEAD,                     /*   CC units either side of 64 that count as the centre */
    G_TILT_RANGE,                    /*   the bend at the extremes, % of G_BEND (the ESP32 bridge's CC2PB_RANGE_PCT) */
    G_TILT_INV,                      /*   the other way round */
    /* (G_BREATH_CC and G_BSMOOTH were here until 2026-10-30: A's controller and SMOOTH now, G_CC_A, G_SMOOTH_A) */
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
    /* the sources A .. D (GLO > CONTROLS, and each one's curve pages; notes/05-controls.md), four of each, in order: */
    G_CC_A, G_CC_B, G_CC_C, G_CC_D,  /* the controller: 0 OFF, 1 .. 127 a CC, 128 channel aftertouch (A: CC11, the breath;
                                      * B: CC52, the re.corder's rotation, as BEND's tilt) */
    G_LO_A, G_LO_B, G_LO_C, G_LO_D,  /* the curve: below LOW nothing (below 0: a head start, a held note sounds) .. */
    G_HI_A, G_HI_B, G_HI_C, G_HI_D,  /*   .. from HIGH on full .. */
    G_SHP_A, G_SHP_B, G_SHP_C, G_SHP_D,   /* .. and between: -64 log (fast at first) .. 0 linear .. 63 exponential */
    G_OVER_A, G_OVER_B, G_OVER_C, G_OVER_D,   /* OVER: 0 off; 1 .. 126: past it (after the curve), XO runs 0 .. full to
                                      * the top (an overblow: growl, drive), the source itself on to full */
    G_SMOOTH_A, G_SMOOTH_B, G_SMOOTH_C, G_SMOOTH_D,   /* a one-pole on the source, ms (0: none, the default: no hidden
                                      * lags; a source over 0 is a row in notes/03-lag-watch.md) */
    G_COUNT                          /* (new ones at the end: the saved state has room for G_SAVED) */
};
#define G_SAVED 48                                 /* (24 until 2026-10-30, 16 until 2026-10-24, 12 until 2026-10-23) */
static int16_t gpar[G_COUNT];
