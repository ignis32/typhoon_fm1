/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* THE UI, AS DATA. Rearrange the panel here: pages, what is on KNOB 1..4, what the buttons and the other
 * encoders do, which MIDI CC moves what. ui/ui.c draws and edits whatever this file says; parameters are named
 * by handle (core/params.c: L(...) the current layer's, LN(n, ...) layer n's, SP(...) the sound as the preset
 * plays it, SG(...) the sound's global copy, G(...) a global), and each shows its own label, value and range from
 * its descriptor. No other file needs to change for a new layout.
 *
 * A page:  {title, kind, engine page, next, {KNOB 1, KNOB 2, KNOB 3, KNOB 4}}
 *   title        0 = the engine's own page title (engine page 1 or 2)
 *   kind         UK_KNOBS four rows of label / value / bar; UK_CURVE the breath curve, drawn, with the live
 *                breath on it; UK_FOLLOW the layer's volume envelope, drawn (delay, attack, decay, sustain,
 *                release); UK_BT Bluetooth's state and log; the graph pages have their four knobs in a row below
 *   next         the page the same button goes to again (its cycle: EDIT 1 -> 2 (-> 3 -> 4) -> TONE, FX DRIVE -> DETUNE -> DELAY -> REVERB -> REVERB +);
 *                PG_STAY: none
 *   PR_NONE      an empty knob
 * Where a page is, the header says (ui.c): the preset (number, name, * when edited) and the layer on layer pages;
 * on SP pages whether the preset uses the GLOBAL group or its OWN; SG pages are the global ones (an amber bar). */

enum { UK_KNOBS, UK_CURVE, UK_FOLLOW, UK_BT };
typedef struct {
    const char *title;
    uint8_t kind, eng_page, next;
    pref_t k[4];
} ui_page_t;

enum {
    PG_BREATH, PG_CURVE, PG_ENG1, PG_ENG2, PG_DRIVE, PG_DETUNE, PG_DELAY, PG_REVERB, PG_ENV, PG_ENV2, PG_LAYER, PG_LAYER2,
    PG_MIX,
    PG_GLOBAL, PG_BT, PG_TILT, PG_G_CURVE, PG_G_LP, PG_G_DRIVE, PG_G_DETUNE, PG_G_DELAY, PG_G_REVERB,
    PG_REVERB2, PG_G_REVERB2, PG_G_BREATH, PG_TONE, PG_ENG3, PG_ENG4, PG_LEAP,
    PG_USB, PG_USBH, PG_ENG5,                     /* (new pages at the end: the others keep their numbers) */
    NPAGES
};
#define PG_STAY 0xFFu
static const ui_page_t PAGES[NPAGES] = {
    [PG_BREATH] = {"BREATH LP", UK_KNOBS, 0, PG_STAY, {SP(S_LPF), SP(S_LPRES), SP(S_BR_LP), PR_NONE}},
    [PG_CURVE] = {"CURVE", UK_CURVE, 0, PG_STAY, {SP(S_BC_LO), SP(S_BC_HI), SP(S_BC_SHAPE), PR_NONE}},
    [PG_ENG1] = {0, UK_KNOBS, 1, PG_ENG2, {L(P_E0), L(P_E1), L(P_E2), L(P_E3)}},
    [PG_ENG2] = {0, UK_KNOBS, 2, PG_ENG3, {L(P_E4), L(P_E5), L(P_E6), L(P_E7)}},
    [PG_ENG3] = {0, UK_KNOBS, 3, PG_ENG4, {L(P_E8), L(P_E9), L(P_E10), L(P_E11)}},   /* (EDIT 3, 4: passed by when empty) */
    [PG_ENG4] = {0, UK_KNOBS, 4, PG_ENG5, {L(P_E12), L(P_E13), L(P_E14), L(P_E15)}},
    [PG_ENG5] = {0, UK_KNOBS, 5, PG_TONE, {L(P_E16), L(P_E17), L(P_E18), L(P_E19)}},   /* (2026-10-28) */
    [PG_TONE] = {"TONE", UK_KNOBS, 0, PG_ENG1, {L(P_TONE), L(P_TONE_A), L(P_TONE_B), L(P_TONE_C)}},   /* (after the engine: core/tone.c) */
    [PG_DRIVE] = {"DRIVE", UK_KNOBS, 0, PG_DETUNE, {SP(S_DRIVE), SP(S_DRV_AT), PR_NONE, PR_NONE}},
    [PG_DETUNE] = {"DETUNE", UK_KNOBS, 0, PG_DELAY, {SP(S_DT_CENTS), SP(S_DT_DELAY), SP(S_DT_WIDTH), SP(S_DT_MIX)}},
    [PG_DELAY] = {"DELAY", UK_KNOBS, 0, PG_REVERB, {SP(S_DLY_TIME), SP(S_DLY_FB), SP(S_DLY_TONE), SP(S_DLY_MIX)}},
    [PG_REVERB] = {"REVERB", UK_KNOBS, 0, PG_REVERB2, {SP(S_REV_SIZE), SP(S_REV_DECAY), SP(S_REV_DAMP), SP(S_REV_MIX)}},
    [PG_REVERB2] = {"REVERB +", UK_KNOBS, 0, PG_DRIVE, {SP(S_REV_TYPE), SP(S_REV_PRE), PR_NONE, PR_NONE}},
    [PG_ENV] = {"ENVELOPE", UK_FOLLOW, 0, PG_ENV2, {L(P_ATK), L(P_DEC), L(P_SUS), L(P_REL)}},
    [PG_ENV2] = {"ENVELOPE +", UK_FOLLOW, 0, PG_ENV, {L(P_DELAY), L(P_BR_AMP), PR_NONE, PR_NONE}},
    [PG_LAYER] = {"LAYER", UK_KNOBS, 0, PG_LAYER2, {L(P_ENGINE), L(P_LEVEL), L(P_TRANS), L(P_PAN)}},
    [PG_LAYER2] = {"BREATH", UK_KNOBS, 0, PG_LAYER, {L(P_BR_FLT), L(P_BR_SHP), L(P_FTYPE), L(P_FINE)}},
    [PG_MIX] = {"MIX", UK_KNOBS, 0, PG_STAY, {LN(0, P_LEVEL), LN(1, P_LEVEL), LN(2, P_LEVEL), SP(S_VOLUME)}},
    [PG_GLOBAL] = {"GLOBAL", UK_KNOBS, 0, PG_BT, {G(G_BSRC), G(G_BEND), G(G_XPOSE), PR_NONE}},   /* (KNOB 4: Bluetooth
                                                  * until 2026-10-24, now on its own page) */
    [PG_BT] = {"BLUETOOTH", UK_BT, 0, PG_TILT, {G(G_BLE_P), G(G_BLE_C), PR_NONE, G(G_BLE_AUTO)}},   /* (KNOB 3: the list,
                                                  * ui.c; app/ble.c) */
    [PG_TILT] = {"TILT>BEND", UK_KNOBS, 0, PG_G_BREATH, {G(G_TILT_CC), G(G_TILT_DEAD), G(G_TILT_RANGE), G(G_TILT_INV)}},
    [PG_G_BREATH] = {"BREATH IN", UK_KNOBS, 0, PG_G_CURVE, {G(G_BREATH_CC), G(G_BSRC), G(G_BSMOOTH), PR_NONE}},
    [PG_G_CURVE] = {"CURVE", UK_CURVE, 0, PG_LEAP, {SG(S_BC_LO), SG(S_BC_HI), SG(S_BC_SHAPE), PR_NONE}},   /* (the global
                                                  * curve: how breath is read, beside BREATH IN) */
    [PG_LEAP] = {"LEAPS", UK_KNOBS, 0, PG_USB, {G(G_LEAP_MODE), G(G_LEAP_ST), G(G_LEAP_MS), PR_NONE}},   /* (core/leap.c) */
    [PG_USB] = {"USB AUDIO", UK_KNOBS, 0, PG_USBH, {G(G_USB_IN), G(G_USB_IN_LV), G(G_USB_OUT), G(G_USB_OUT_LV)}},
                                                  /* (app/usbaudio.c) */
    [PG_USBH] = {"USB HOST", UK_KNOBS, 0, PG_GLOBAL, {G(G_USB_HOST), PR_NONE, PR_NONE, PR_NONE}},   /* (app/usbhost.c) */
    /* the global effects (what a preset without its own group plays): REC, apart from GLO's settings (2026-10-24) */
    [PG_G_LP] = {"BREATH LP", UK_KNOBS, 0, PG_G_DRIVE, {SG(S_LPF), SG(S_LPRES), SG(S_BR_LP), PR_NONE}},
    [PG_G_DRIVE] = {"DRIVE", UK_KNOBS, 0, PG_G_DETUNE, {SG(S_DRIVE), SG(S_DRV_AT), PR_NONE, PR_NONE}},
    [PG_G_DETUNE] = {"DETUNE", UK_KNOBS, 0, PG_G_DELAY, {SG(S_DT_CENTS), SG(S_DT_DELAY), SG(S_DT_WIDTH), SG(S_DT_MIX)}},
    [PG_G_DELAY] = {"DELAY", UK_KNOBS, 0, PG_G_REVERB, {SG(S_DLY_TIME), SG(S_DLY_FB), SG(S_DLY_TONE), SG(S_DLY_MIX)}},
    [PG_G_REVERB] = {"REVERB", UK_KNOBS, 0, PG_G_REVERB2, {SG(S_REV_SIZE), SG(S_REV_DECAY), SG(S_REV_DAMP), SG(S_REV_MIX)}},
    [PG_G_REVERB2] = {"REVERB +", UK_KNOBS, 0, PG_G_LP, {SG(S_REV_TYPE), SG(S_REV_PRE), PR_NONE, PR_NONE}},
};

/* the encoders that are not KNOB 1..4 (ENC_*) */
enum { UE_PRESET, UE_LAYER, UE_PAR };
static const struct { uint8_t enc, kind; pref_t r; } ENCODERS[] = {
    {ENC_PRESETS, UE_PRESET, PR_NONE},             /* PRESETS: the preset (loads it; in the SAVE prompt: the slot) */
    {ENC_SELECT, UE_LAYER, PR_NONE},               /* SELECT: the layer, 1 2 3 1 ... */
    {ENC_ALGORITHM, UE_PAR, L(P_ENGINE)},          /* ALGORITHM: the layer's engine */
};

/* the buttons (BTN_*): a press, and a hold (~0.6 s). A button with no hold acts at once; one with a hold acts when
 * let go before the hold. Their LEDs say where you are (ui.c ui_leds): a page button is lit on its pages, MUTE
 * (PLAY) while the current layer is muted and blinking while one is soloed, OCT- / OCT+ while transposed down / up,
 * SAVE while the preset is edited.
 *   UA_PAGE n       go to page n; on one of its pages already: the next one (PAGES[].next); back on the button
 *                   later: the page of its cycle it was on last
 *   UA_XPOSE s      transpose by s semitones; SEL held: by s octaves; OCT- and OCT+ together: back to 0 (at once,
 *                   on the second press; neither acts when let go). A change cuts the sounding note (app/app.c)
 *   UA_XPOSE_OCT    (a hold of OCT- / OCT+) transpose by an octave, the button's way
 *   UA_MUTE         mute / unmute the current layer
 *   UA_SOLO         (PLAY's hold) only the current layer (the others muted); again: the mutes as they were
 *   A question (the SAVE prompt, GLOBAL TO FACTORY): UI_YES (OCT+) is yes, UI_NO (OCT-) no; any other button
 *   (or 10 s) no too, except SAVE in the SAVE prompt (yes). While one is asked the OCT buttons do not transpose.
 *   UA_SAVE         the SAVE prompt: PRESETS picks the slot (the one loaded first); OCT+ / SAVE again saves
 *   UA_SAVE_AS      (a hold) SAVE AS: the name, then the slot. The 27 keys type A..Z and a space at the cursor (no
 *                   notes meanwhile), UI_DELETE deletes the character before it, SELECT moves it, ALGORITHM turns
 *                   the character at it (A..Z 0..9 - . / space), PRESETS picks the slot. Only OCT+ saves (no double
 *                   press saves by chance), OCT- cancels; the other buttons do nothing there, and it does not time out
 *   UA_ASK_GLOBAL   (a hold) asks GLOBAL TO FACTORY: every global (G_*, the global sound) back to its default
 *   UA_GLOBAL       (a hold) the page's sound group back to the global one, when the preset has its own
 *   UA_ALL_OFF      panic: every note off */
enum { UA_NONE, UA_PAGE, UA_XPOSE, UA_XPOSE_OCT, UA_MUTE, UA_SOLO, UA_SAVE, UA_SAVE_AS, UA_GLOBAL, UA_ASK_GLOBAL, UA_ALL_OFF };
#define UI_YES BTN_OCT_UP
#define UI_NO BTN_OCT_DOWN
#define UI_DELETE BTN_PLAY                         /* SAVE AS: the character before the cursor */
static const struct { uint8_t btn, act, hold; uint8_t arg; int8_t step; } BUTTONS[] = {
    {BTN_HOME, UA_PAGE, UA_GLOBAL, PG_BREATH, 0},
    {BTN_ENV, UA_PAGE, UA_GLOBAL, PG_CURVE, 0},
    {BTN_EDIT, UA_PAGE, UA_NONE, PG_ENG1, 0},
    {BTN_FX, UA_PAGE, UA_GLOBAL, PG_DRIVE, 0},
    {BTN_LFO, UA_PAGE, UA_NONE, PG_ENV, 0},
    {BTN_ARP, UA_PAGE, UA_NONE, PG_LAYER, 0},
    {BTN_SEQ, UA_PAGE, UA_GLOBAL, PG_MIX, 0},       /* (hold: VOLUME back to the global one) */
    {BTN_GLO, UA_PAGE, UA_ASK_GLOBAL, PG_GLOBAL, 0},
    {BTN_OCT_DOWN, UA_XPOSE, UA_XPOSE_OCT, 0, -1},   /* (a press: a semitone, when let go; a hold: an octave) */
    {BTN_OCT_UP, UA_XPOSE, UA_XPOSE_OCT, 0, 1},
    {BTN_PLAY, UA_MUTE, UA_SOLO, 0, 0},            /* (hold: solo; panic until 2026-10-24) */
    {BTN_REC, UA_PAGE, UA_ALL_OFF, PG_G_LP, 0},     /* the global effects' pages, apart from GLO (hold: panic; SOLO until
                                                     * 2026-10-24) */
    {BTN_SAVE, UA_SAVE, UA_SAVE_AS, 0, 0},
    {BTN_SEL, UA_NONE, UA_NONE, 0, 0},             /* (with OCT- / OCT+: octaves) */
};
#define UI_HOLD_MS 600u

/* MIDI CC -> parameter, over its whole range. CC2 (breath) and pitch bend are the performance's (app/app.c) */
static const struct { uint8_t cc; pref_t r; } CC_MAP[] = {
    {7, LN(0, P_LEVEL)},
    {74, SP(S_LPF)},
    {71, SP(S_LPRES)},
};
