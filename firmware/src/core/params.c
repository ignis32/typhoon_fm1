/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* Every parameter by one handle, described once: the UI (ui/), MIDI CC (ui/layout.h CC_MAP) and storage
 * all go through here, so a parameter added in layers.h / an engine shows up everywhere by its descriptor.
 *
 * A handle (pref_t) names:
 *   L(P_x)       a parameter of the layer the UI is on; LN(n, P_x) of layer n. P_E0..P_E15 are the engine's
 *                sixteen (engine_t.edit, or its desc() hook)
 *   SP(S_x)      a sound parameter as the preset plays it (core/layers.h sv()). Setting one makes its whole
 *                group the preset's OWN (copied from the global one first, so nothing jumps)
 *   SG(S_x)      a sound parameter's GLOBAL copy (the GLO pages): every preset that uses the group follows it
 *   G(G_x)       a global parameter
 * Descriptors are Felucca-Engines' param_desc_t: label, format (F_*), min, max, default, value names, unit. */

typedef uint16_t pref_t;
#define PR_GLOBAL 0x8000u
#define PR_SND 0x4000u
#define PR_SEL 0x0Fu                                /* "the layer the UI is on" */
#define G(i) ((pref_t)(PR_GLOBAL | (i)))
#define SP(i) ((pref_t)(PR_SND | (i)))
#define SG(i) ((pref_t)(PR_GLOBAL | PR_SND | (i)))
#define L(i) ((pref_t)(PR_SEL << 8 | (i)))
#define LN(n, i) ((pref_t)((n) << 8 | (i)))
#define PR_NONE ((pref_t)0xFFFFu)
static int pr_layer(pref_t r) { return r != PR_NONE && !(r & (PR_GLOBAL | PR_SND)); }
static int pr_snd(pref_t r) { return r != PR_NONE && (r & PR_SND); }   /* SP or SG */

static const char *const N_FTYPE[] = {"LP", "BP", "HP"};
static const char *const N_BSRC[] = {"AUTO", "BREATH", "VEL"};   /* (BREATH: the BREATH CC only) */
static const char *const N_REVT[] = {"PLATE", "HALL"};   /* (OLD dropped 2026-10-23) */
static const char *const N_LAYER[] = {"1", "2", "3", "4"};
static const char *const N_BLE_C[] = {"OFF", "FIRST", "CHOOSE"};   /* (app/ble.c BC_*; until 2026-10-24 one knob,
                                                                    * G_BT: OFF / PERIPH / BOTH) */
static const char *const N_LEAP[] = {"OFF", "HOLD"};          /* (core/leap.c LEAP_*) */
static const char *const N_USB_IN[] = {"OFF", "MUTE", "ON"};      /* (app/usbaudio.c) */
static const char *const N_USB_OUT[] = {"OFF", "MUTE", "MIX", "SYNTH"};

/* the layer's own parameters (P_ENGINE's names come from engines.h: par_text) */
static const param_desc_t COMMON[NCOMMON] = {
    [P_ENGINE] = {"ENGINE", F_ENUM, 0, 255, 0, 0, 0},
    [P_LEVEL] = {"LEVEL", F_PCT, 0, 127, 100, 0, 0},
    [P_MUTE] = {"MUTE", F_ONOFF, 0, 1, 0, 0, 0},
    [P_TRANS] = {"TRANSP", F_SEMI, -24, 24, 0, 0, 0},
    [P_FINE] = {"FINE", F_INT, -50, 50, 0, 0, "ct"},
    [P_PAN] = {"PAN", F_BIPCT, -64, 63, 0, 0, 0},
    [P_ATK] = {"ATTACK", F_TIME, 0, 127, 0, 0, "E"},     /* (0 and BR>AMP 0: no lag in
                                                           * staccato; the loudness follower is for extra layers) */
    [P_REL] = {"RELEASE", F_TIME, 0, 127, 50, 0, "E"},   /* (unit "E": an envelope time, env_ms) */
    [P_FTYPE] = {"F.TYPE", F_ENUM, 0, 2, 0, N_FTYPE, 0},
    [P_BR_AMP] = {"BR>AMP", F_PCT, 0, 127, 0, 0, 0},
    [P_BR_FLT] = {"BR>TONE", F_BIPCT, -64, 63, 40, 0, 0},
    [P_BR_SHP] = {"BR>SHP", F_BIPCT, -64, 63, 0, 0, 0},
    [P_DELAY] = {"DELAY", F_TIME, 0, 127, 0, 0, "E"},
    [P_DEC] = {"DECAY", F_TIME, 0, 127, 64, 0, "E"},
    [P_SUS] = {"SUSTAIN", F_PCT, 0, 127, 127, 0, 0},
    [P_TONE] = {"TONE", F_ENUM, 0, TN_COUNT - 1, TN_OFF, N_TONE, 0},   /* (core/tone.c; A .. C: tone_desc) */
    [P_TONE_A] = {"-", F_INT, 0, 0, 0, 0, 0},
    [P_TONE_B] = {"-", F_INT, 0, 0, 0, 0, 0},
    [P_TONE_C] = {"-", F_INT, 0, 0, 0, 0, 0},
    [P_QUIRKS] = {"QUIRKS", F_INT, 0, 100, 0, 0, "%"},   /* (core/layers.c layer_quirk; 0: every note true) */
    [P_QSET] = {"QUIRK SET", F_INT, 0, 127, 0, 0, 0},
};
/* the sound's (the defaults are the global copy's at first) */
static const param_desc_t SOUND[S_COUNT] = {
    [S_BC_LO] = {"LOW", F_INT, -64, 126, 0, 0, 0},     /* (the breath curve's: its page says so); below 0: a head
                                                           * start: a held note sounds before any breath */
    [S_BC_HI] = {"HIGH", F_INT, 1, 127, 120, 0, 0},
    [S_BC_SHAPE] = {"SHAPE", F_INT, -64, 63, 0, 0, 0},
    [S_LPF] = {"-", F_INT, 0, 0, 0, 0, 0},             /* (S_LPF, S_LPRES, S_BR_LP: the mix's breath LP until 2026-10-28,
                                                         * now ERS's and FM's own, EDIT 5: nothing, kept in place) */
    [S_LPRES] = {"-", F_INT, 0, 0, 0, 0, 0},
    [S_BR_LP] = {"-", F_INT, 0, 0, 0, 0, 0},
    [S_DRIVE] = {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
    [S_DRV_AT] = {"DRV AT", F_PCT, 0, 127, 96, 0, 0},
    [S_DLY_TIME] = {"TIME", F_TIME, 64, 125, 100, 0, 0},   /* 30 ms .. ~1 s */
    [S_DLY_FB] = {"FEEDBK", F_PCT, 0, 127, 40, 0, 0},
    [S_DLY_TONE] = {"TONE", F_PCT, 0, 127, 80, 0, 0},
    [S_DLY_MIX] = {"MIX", F_PCT, 0, 127, 0, 0, 0},
    [S_REV_SIZE] = {"SIZE", F_PCT, 0, 127, 90, 0, 0},
    [S_REV_DECAY] = {"DECAY", F_PCT, 0, 127, 70, 0, 0},
    [S_REV_DAMP] = {"DAMP", F_PCT, 0, 127, 50, 0, 0},
    [S_REV_MIX] = {"MIX", F_PCT, 0, 127, 24, 0, 0},
    [S_DT_CENTS] = {"DETUNE", F_INT, 1, 50, 9, 0, "ct"},
    [S_DT_DELAY] = {"DELAY", F_INT, 0, 30, 10, 0, "ms"},
    [S_DT_WIDTH] = {"WIDTH", F_PCT, 0, 127, 127, 0, 0},
    [S_DT_MIX] = {"MIX", F_PCT, 0, 127, 0, 0, 0},
    [S_REV_TYPE] = {"TYPE", F_ENUM, 0, 1, 0, N_REVT, 0},   /* (PLATE the default) */
    [S_REV_PRE] = {"PRE-DLY", F_INT, 0, 200, 20, 0, "ms"},
    [S_VOLUME] = {"VOLUME", F_INT, 0, 127, 100, 0, 0},   /* (100: unity; 127 = +2 dB) */
};
static const char *const GROUP_NAME[NGROUP] = {"CURVE", "LP", "DRIVE", "DELAY", "REVERB", "DETUNE", "VOLUME"};
static const param_desc_t GLOBAL[G_COUNT] = {
    [G_BSRC] = {"LOUDNESS", F_ENUM, 0, 2, 0, N_BSRC, 0},
    [G_BEND] = {"BEND", F_INT, 0, 24, 4, 0, "st"},   /* (4 semitones) */
    [G_XPOSE] = {"TRANSP", F_SEMI, -24, 24, 0, 0, 0},
#if TYPHOON_BLE
    [G_BLE_P] = {"BLE-P", F_ONOFF, 0, 1, 1, 0, 0},
#else
    [G_BLE_P] = {"BLE-P", F_ONOFF, 0, 0, 0, 0, 0},       /* (not in this build: the knob is empty) */
#endif
    [G_LAYER] = {"LAYER", F_ENUM, 0, NLAYER - 1, 0, N_LAYER, 0},
    [G_TILT_CC] = {"CC", F_INT, 0, 127, 52, 0, 0},       /* (0: OFF, par_text) */
    [G_TILT_DEAD] = {"DEAD", F_INT, 0, 30, 6, 0, 0},
    [G_TILT_RANGE] = {"RANGE", F_INT, 0, 100, 10, 0, "%"},
    [G_TILT_INV] = {"INVERT", F_ONOFF, 0, 1, 0, 0, 0},
    [G_BREATH_CC] = {"BREATH CC", F_INT, 1, 128, 11, 0, 0},   /* (128: AT, channel aftertouch; par_text) */
    [G_BSMOOTH] = {"SMOOTH", F_INT, 0, 100, 0, 0, "ms"},    /* (0: none, the default since 2026-10-24; 5 until then,
                                                              * 20 until 2026-10-23: ~46 ms to 90 %, felt as lag) */
    [G_LEAP_MODE] = {"LEAPS", F_ENUM, 0, LEAP_NMODE - 1, LEAP_HOLD, N_LEAP, 0},
    [G_LEAP_ST] = {"OVER", F_INT, 1, 24, 13, 0, "st"},   /* (the thumb's passing notes: +17 / +19 from D6) */
    [G_LEAP_MS] = {"WAIT", F_INT, 0, 60, 30, 0, "ms"},   /* (95 % of them end within 27 ms, in a recorded take) */
    [G_USB_IN] = {"USB IN", F_ENUM, 0, 2, 0, N_USB_IN, 0},       /* (OFF by default: the computer keeps its own audio
                                                                  * devices unless wanted) */
    [G_USB_IN_LV] = {"IN LEVEL", F_INT, 0, 200, 100, 0, "%"},
    [G_USB_OUT] = {"USB OUT", F_ENUM, 0, 3, 0, N_USB_OUT, 0},
    [G_USB_OUT_LV] = {"OUT LEVEL", F_INT, 0, 200, 100, 0, "%"},
#if TYPHOON_BLE
    [G_BLE_C] = {"BLE-C", F_ENUM, 0, 2, 1, N_BLE_C, 0},          /* (FIRST: the first Bluetooth MIDI device found) */
    [G_BLE_AUTO] = {"AUTO_RECONNECT", F_ONOFF, 0, 1, 1, 0, 0},
#else
    [G_BLE_C] = {"BLE-C", F_ENUM, 0, 0, 0, N_BLE_C, 0},
    [G_BLE_AUTO] = {"AUTO_RECONNECT", F_ONOFF, 0, 0, 0, 0, 0},
#endif
    [G_USB_HOST] = {"USB HOST", F_ONOFF, 0, 1, 0, 0, 0},     /* (OFF by default: a computer, the editor, the installer) */
};

static uint32_t par_dirty;                         /* bumped by every change: the UI redraws, storage saves */
static uint8_t ps_edited;                          /* the preset changed since it was loaded / saved (the "*") */

static uint32_t par_layer(pref_t r)
{
    uint32_t n = (r >> 8) & 0x0Fu;
    return n == PR_SEL ? (uint32_t)gpar[G_LAYER] : n % NLAYER;
}

static const engine_t *layer_engine(uint32_t n)
{
    uint8_t miss;
    return ENGINES[eng_index((uint32_t)trk[n].p[P_ENGINE], &miss)].e;
}

static const param_desc_t *par_desc(pref_t r)
{
    uint32_t i = r & 0xFFu;
    if (r == PR_NONE)
        return 0;
    if (r & PR_SND)
        return i < S_COUNT ? &SOUND[i] : 0;
    if (r & PR_GLOBAL)
        return i < G_COUNT ? &GLOBAL[i] : 0;
    if (i >= P_TONE_A && i <= P_TONE_C)            /* (TONE's knobs mean what its type says) */
        return tone_desc(trk[par_layer(r)].p[P_TONE], i - P_TONE_A);
    if (i < NCOMMON)
        return &COMMON[i];
    if (i < P_COUNT) {
        static const param_desc_t NO_KNOB = {"-", F_INT, 0, 0, 0, 0, 0};   /* (an OFF layer: no engine knobs) */
        uint32_t n = par_layer(r), k = i - P_E0;
        if (trk[n].p[P_ENGINE] == ENG_ID_OFF)
            return &NO_KNOB;
        const engine_t *e = layer_engine(n);
        const param_desc_t *d = e->desc ? e->desc(&trk[n], k) : 0;
        if (!d && !e->edit[k].label)
            return &NO_KNOB;                       /* (a knob the engine does not use) */
        return d ? d : &e->edit[k];
    }
    return 0;
}

static int32_t par_get(pref_t r)
{
    uint32_t i = r & 0xFFu;
    if (r == PR_NONE)
        return 0;
    if (r & PR_SND)
        return i < S_COUNT ? ((r & PR_GLOBAL) ? sglob[i] : sv(i)) : 0;
    if (r & PR_GLOBAL)
        return i < G_COUNT ? gpar[i] : 0;
    return i < P_COUNT ? trk[par_layer(r)].p[i] : 0;
}

/* does the preset have its own copy of the group of S_x? */
static int snd_own(uint32_t i) { return (s_own >> S_GROUP[i]) & 1u; }
/* the preset takes its own copy of group g (from the global one: it sounds the same until a value changes) */
static void group_own(uint32_t g)
{
    uint32_t i;
    if ((s_own >> g) & 1u)
        return;
    for (i = 0; i < S_COUNT; i++)
        if (S_GROUP[i] == g)
            sown[i] = sglob[i];
    s_own |= (uint8_t)(1u << g);
    ps_edited = 1;
    par_dirty++;
}
/* the preset uses the global group g again (its own values dropped) */
static void group_global(uint32_t g)
{
    if (!((s_own >> g) & 1u))
        return;
    s_own &= (uint8_t)~(1u << g);
    ps_edited = 1;
    par_dirty++;
}

/* a layer's engine: the id, its sixteen parameters back to that engine's defaults, and how much breath moves its
 * first knob (engines.h br_shp) */
static void layer_set_engine(uint32_t n, uint32_t idx)
{
    const engine_t *e;
    uint32_t k;
    if (idx >= NENGINES) {                         /* OFF (the knob's last place): the rest kept for the next engine */
        trk[n].p[P_ENGINE] = ENG_ID_OFF;
        return;
    }
    e = ENGINES[idx].e;
    for (k = 0; k < NEPAR; k++)
        trk[n].p[P_E0 + k] = e->edit[k].def;
    trk[n].p[P_BR_SHP] = ENGINES[idx % NENGINES].br_shp;
    trk[n].p[P_ENGINE] = ENGINES[idx % NENGINES].id;
}

static void par_set(pref_t r, int32_t v)
{
    const param_desc_t *d = par_desc(r);
    uint32_t i = r & 0xFFu;
    int16_t *p;
    if (!d)
        return;
    if (pr_layer(r) && i == P_ENGINE) {            /* by position in this build's ENGINES[] */
        layer_set_engine(par_layer(r), (uint32_t)clamp(v, 0, (int32_t)NENGINES));   /* (NENGINES: OFF) */
        ps_edited = 1;
        par_dirty++;
        return;
    }
    v = clamp(v, d->min, d->max);
    if (r & PR_SND) {
        if (!(r & PR_GLOBAL)) {
            if (v == sv(i))
                return;
            group_own(S_GROUP[i]);
            p = &sown[i];
        } else
            p = &sglob[i];
    } else if (r & PR_GLOBAL)
        p = &gpar[i];
    else
        p = &trk[par_layer(r)].p[i];
    if (*p != v) {
        *p = (int16_t)v;
        if (pr_layer(r) && i == P_TONE)            /* another TONE: its knobs at its defaults */
            tone_defaults(trk[par_layer(r)].p);
        if (!(r & PR_GLOBAL))
            ps_edited = 1;
        par_dirty++;
    }
}

/* an encoder turned: steps of 1, faster for wide ranges when turned quickly (|steps| > 1 per poll) */
static void par_step(pref_t r, int32_t steps)
{
    const param_desc_t *d = par_desc(r);
    if (!d || !steps)
        return;
    if (pr_layer(r) && (r & 0xFFu) == P_ENGINE) {
        int32_t idx = (int32_t)eng_pos((uint32_t)par_get(r)) + (steps > 0 ? 1 : -1), np = (int32_t)NENGINES + 1;
        par_set(r, (idx + np) % np);               /* (the engines, then OFF, round) */
        return;
    }
    if (d->max - d->min > 40 && (steps > 1 || steps < -1))
        steps *= 3;
    par_set(r, par_get(r) + steps);
}

/* MIDI CC 0..127 onto the parameter's whole range */
static void par_cc(pref_t r, uint32_t v)
{
    const param_desc_t *d = par_desc(r);
    if (d)
        par_set(r, d->min + (int32_t)((d->max - d->min) * v + 63) / 127);
}

/* ---- text */
static uint32_t par_strlen(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }
static char *put_int(char *s, int32_t v, int plus)
{
    char t[12];
    uint32_t n = 0, u = (uint32_t)(v < 0 ? -v : v);
    if (v < 0) *s++ = '-';
    else if (plus && v > 0) *s++ = '+';
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    while (n) *s++ = t[--n];
    *s = 0;
    return s;
}
static void put_str(char *s, const char *a)
{
    while (*a) *s++ = *a++;
    *s = 0;
}

/* value v of parameter r as the screen shows it (buf: 16 bytes); par_text: its value now */
static void par_value_text(pref_t r, int32_t v, char *buf)
{
    const param_desc_t *d = par_desc(r);
    char *e;
    buf[0] = 0;
    if (!d)
        return;
    if (r == G(G_TILT_CC) && !v) {                 /* (tilt -> bend: off) */
        put_str(buf, "OFF");
        return;
    }
    if (r == G(G_BREATH_CC) && v == 128) {        /* (breath from channel aftertouch) */
        put_str(buf, "AT");
        return;
    }
    if (pr_layer(r) && (r & 0xFFu) == P_ENGINE) {
        uint8_t miss;
        eng_index((uint32_t)v, &miss);
        put_str(buf, eng_id_name((uint32_t)v));
        if (miss && v != ENG_ID_OFF) put_str(buf + par_strlen(buf), "?");   /* saved with an engine this build lacks */
        return;
    }
    switch (d->fmt) {
    case F_ENUM:
        if (d->names) { put_str(buf, d->names[clamp(v, d->min, d->max) - d->min]); break; }
        put_int(buf, v, 0);
        break;
    case F_ONOFF:
        put_str(buf, v ? "ON" : "OFF");
        break;
    case F_PCT:
        e = put_int(buf, (v * 100 + 63) / 127, 0);
        put_str(e, "%");
        break;
    case F_BIPCT:
        e = put_int(buf, v * 100 / 64, 1);
        put_str(e, "%");
        break;
    case F_SEMI:
        e = put_int(buf, v, 1);
        put_str(e, " st");
        break;
    case F_OCT:
        put_int(buf, v, 1);
        break;
    case F_TIME: {
        uint32_t ms = d->unit && d->unit[0] == 'E' ? env_ms(v) : time_ms(v);   /* (an envelope time: its own curve) */
        if (ms < 1000u) { e = put_int(buf, (int32_t)ms, 0); put_str(e, " ms"); }
        else { e = put_int(buf, (int32_t)(ms / 1000u), 0); *e++ = '.'; e = put_int(e, (int32_t)(ms / 100u % 10u), 0); put_str(e, " s"); }
        break;
    }
    default:
        e = put_int(buf, v, 0);
        if (d->unit) { *e++ = ' '; put_str(e, d->unit); }
        break;
    }
}
static void par_text(pref_t r, char *buf) { par_value_text(r, par_get(r), buf); }

/* ---- defaults (storage: core/presets.c) */
static void layers_defaults(void)                  /* the layers of a new preset: one sine plays, the others OFF */
{
    uint32_t i, n;
    for (n = 0; n < NLAYER; n++) {
        for (i = 0; i < NCOMMON; i++)
            trk[n].p[i] = COMMON[i].def;
        layer_set_engine(n, 0);                    /* (the first engine's knobs, for whatever comes) */
        if (n)
            layer_set_engine(n, NENGINES);         /* OFF, at full level: an engine picked there plays at once */
    }
    /* the sine: ENGINES[0] is FM (engines.h); no index, no air, breath not on the index (SINE's, until 2026-10-28) */
    trk[0].p[P_E0 + 0] = 0;                        /* INDEX */
    trk[0].p[P_E0 + 4] = 0;                        /* AIR */
    trk[0].p[P_BR_SHP] = 0;
    s_own = 0;
}

static void params_defaults(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        gpar[i] = GLOBAL[i].def;
    for (i = 0; i < S_COUNT; i++)
        sglob[i] = sown[i] = SOUND[i].def;
    layers_defaults();
}

static void globals_defaults(void)                 /* the GLO pages' "GLOBAL TO FACTORY": the globals, the global sound */
{
    uint32_t i, layer = (uint32_t)gpar[G_LAYER];
    for (i = 0; i < G_COUNT; i++)
        gpar[i] = GLOBAL[i].def;
    gpar[G_LAYER] = (int16_t)layer;                /* (where the UI is: not a setting) */
    for (i = 0; i < S_COUNT; i++)
        sglob[i] = SOUND[i].def;
    par_dirty++;
}
