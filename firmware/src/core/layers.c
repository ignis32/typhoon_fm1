/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The performance and the layers (the audio ISR's side).
 *
 * The main loop posts what happens (perf_note_on / _off, perf_breath, perf_bend, perf_all_off) into a ring;
 * the audio ISR takes it at the start of each control block (CTL = 32 frames) and owns everything below:
 *   - the note: mono, last-note priority over the held notes. A note while another sounds is LEGATO: the
 *     voices only change pitch (no note_on, no reset: phases and filters run on). The re.corder sends the new
 *     note-on before the old note-off, so a slur is always legato.
 *   - the breath: CC2 (0..127) through the curve (S_BC_LO / HI / SHAPE: breath_curve, the UI draws it),
 *     smoothed (G_BSMOOTH: global, the only smoothing breath gets). G_BSRC AUTO uses CC2 once it comes, else the note-on velocity (a keyboard, the keys).
 *   - transpose (G_XPOSE): taken when a note starts; the main loop cuts the sounding note when it changes
 *     (perf_cut: a short fade), so a note is never moved under the player. Notes are kept by the number that
 *     came in, so the note-off of a note held across a change still finds it.
 *   - per layer: the loudness follows breath (rising at P_ATK's speed) while the note is held, fades at P_REL
 *     after it; P_BR_AMP says how much of it is breath. Breath opens the engine's own filter (P_BR_FLT,
 *     vmod.cutoff) and moves its shape (P_BR_SHP, vmod.shape), so every engine answers breath without code of
 *     its own.
 *   - then the mix: drive, the breath low-pass, delay, reverb (core/fx.c fx_block).
 * The UI reads perf_ui (note, breath, levels): written here, read there, no lock needed for a meter. */

enum { EV_ON, EV_OFF, EV_BREATH, EV_BEND, EV_ALLOFF, EV_CUT };
typedef struct { uint8_t t, a, b; } pev_t;
#define PQ 64u
static pev_t pq[PQ];
static volatile uint32_t pq_w, pq_r;

static void perf_post(uint32_t t, uint32_t a, uint32_t b)   /* main loop only */
{
    if (pq_w - pq_r >= PQ)
        return;                                    /* (64 behind: the ISR stalled; drop rather than overwrite) */
    pq[pq_w % PQ].t = (uint8_t)t;
    pq[pq_w % PQ].a = (uint8_t)a;
    pq[pq_w % PQ].b = (uint8_t)b;
    RING_PUBLISH();
    pq_w++;
}
static void perf_note_on(uint32_t note, uint32_t vel) { perf_post(vel ? EV_ON : EV_OFF, note & 127u, vel & 127u); }
static void perf_note_off(uint32_t note) { perf_post(EV_OFF, note & 127u, 0); }
static void perf_breath(uint32_t cc2) { perf_post(EV_BREATH, cc2 & 127u, 0); }
static void perf_bend(uint32_t lsb, uint32_t msb) { perf_post(EV_BEND, lsb & 127u, msb & 127u); }
static void perf_all_off(void) { perf_post(EV_ALLOFF, 0, 0); }
static void perf_cut(void) { perf_post(EV_CUT, 0, 0); }   /* every note off, the sound out in a few ms */

static volatile struct {
    uint8_t note, gate;                            /* the note (255: none yet), held */
    int32_t breath;                                /* Q15, after the curve and the smoothing */
    int32_t breath_in;                             /* Q15, before the curve (the dot's x on the curve page) */
    int32_t amp[NLAYER];                           /* each layer's loudness, Q15 */
    uint8_t missing[NLAYER];                       /* the layer's engine id is not in this build */
} perf_ui = {255};

#define BLOCKS_PER_S (FS / CTL)                    /* control blocks a second (1378) */

static struct {
    uint8_t stack[8], n;                           /* held notes, the last on top */
    uint8_t note, vel, gate;
    int8_t xpose;                                  /* G_XPOSE as the note started */
    int32_t braw;                                 /* the last CC2, Q15 */
    int32_t breath;                                /* smoothed, Q15 */
    int32_t bend16;                                /* 1/16 semitone */
    int32_t bend_fine;                             /* and the rest, as m.fine (1/4096 of the increment, ~0.42 cent) */
    uint32_t blocks, t_cc2;                        /* block counter, when CC2 last came (0: never) */
} pf = {.note = 60};

enum { ENV_OFF, ENV_DELAY, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
typedef struct { uint8_t st; uint32_t n; int32_t lev; } env_t;   /* a volume envelope: stage, blocks, level Q15 */

static struct {
    uint8_t id;                                    /* the engine id it plays now (0: none yet) */
    uint8_t idx;                                   /* its position in ENGINES[] */
    uint8_t cut;                                   /* fading out fast (perf_cut) */
    env_t env;                                     /* the volume envelope */
    int32_t amp;                                   /* loudness now, Q15 */
} lay[NLAYER];

/* a breath curve: CC2 as Q15 in, Q15 out, for LOW, HIGH (0..127), SHAPE (-64..63) (also the UI's picture of it) */
static int32_t breath_curve_of(int32_t x, int32_t lo7, int32_t hi7, int32_t s)
{
    int32_t lo = lo7 * 32767 / 127, hi = hi7 * 32767 / 127, c, y;
    if (hi <= lo + 256)
        hi = lo + 256;
    x = x <= lo ? 0 : clamp(((x - lo) << 15) / (hi - lo), 0, 32767);   /* (< 2^31, LOW down to -64: no 64-bit division) */
    if (s > 0) {                                   /* toward x^3: slow at first, the top end opens up */
        c = (int32_t)(mul64(mul64(x, x) >> 15, x) >> 15);
        y = x + (c - x) * s / 63;
    } else {                                       /* toward 1 - (1 - x)^3: fast at first */
        int32_t u = 32767 - x;
        c = 32767 - (int32_t)(mul64(mul64(u, u) >> 15, u) >> 15);
        y = x + (c - x) * -s / 64;
    }
    return clamp(y, 0, 32767);
}
static int32_t breath_curve(int32_t x) { return breath_curve_of(x, sv(S_BC_LO), sv(S_BC_HI), sv(S_BC_SHAPE)); }

/* the time of a 0..127 time parameter in ms: 1 ms .. ~4 s, exponential (P_ATK, P_REL; ui shows the same) */
static uint32_t time_ms(int32_t v)
{
    static const uint16_t T[9] = {1, 2, 5, 12, 30, 75, 180, 450, 1100};   /* every 16 steps; x3.6 past the last */
    uint32_t i = (uint32_t)clamp(v, 0, 127), k = i >> 4, f = i & 15u;
    uint32_t a = T[k], b = k < 8u ? T[k + 1] : 4000u;
    return a + (((b - a) * f) >> 4);
}
/* the volume envelope's times (P_DELAY, P_ATK, P_DEC, P_REL) in ms: 0 .. ~3.8 s, exponential; 0 is none */
static uint32_t env_ms(int32_t v)
{
    static const uint16_t T[9] = {0, 5, 15, 40, 100, 250, 600, 1500, 4000};   /* every 16 steps */
    uint32_t i = (uint32_t)clamp(v, 0, 127), k = i >> 4, f = i & 15u;
    return T[k] + (((uint32_t)(T[k + 1] - T[k]) * f) >> 4);
}
/* a one-pole's step per control block (Q15) for a time constant in ms */
static int32_t pole_k(uint32_t ms)
{
    uint32_t den = ms * (BLOCKS_PER_S / 10u) / 100u + 1u;   /* blocks in ms */
    return den <= 1u ? 32768 : (int32_t)(32768u / den);
}

/* one control block of a layer's volume envelope (the audio, and the LFO page's graph: the same steps). DELAY, then
 * ATTACK (linear to full), DECAY (toward SUSTAIN), SUSTAIN while held; RELEASE once the gate is off (cut: 6 ms).
 * Started by env_start on a fresh attack only, from the level it has (a retrigger while it fades: no click) */
static uint32_t env_blocks(uint32_t ms) { return ms * (BLOCKS_PER_S / 10u) / 100u; }
static void env_start(env_t *e, const int16_t *p)
{
    e->st = p[P_DELAY] ? ENV_DELAY : ENV_ATTACK;
    e->n = 0;
}
static void env_step(env_t *e, const int16_t *p, int gate, int cut)
{
    int32_t sus = p[P_SUS] * 32767 / 127;
    uint32_t nb;
    if (!gate && e->st && e->st != ENV_RELEASE)
        e->st = ENV_RELEASE;
    switch (e->st) {
    case ENV_DELAY:
        if (++e->n >= env_blocks(env_ms(p[P_DELAY])))
            e->st = ENV_ATTACK;
        break;
    case ENV_ATTACK:
        nb = env_blocks(env_ms(p[P_ATK]));
        e->lev = nb ? e->lev + (int32_t)((32767u + nb - 1u) / nb) : 32767;
        if (e->lev >= 32767) {
            e->lev = 32767;
            e->st = ENV_DECAY;
        }
        break;
    case ENV_DECAY:
        e->lev += (int32_t)(mul64(sus - e->lev, pole_k(env_ms(p[P_DEC]))) >> 15);
        if (e->lev - sus < 64 && sus - e->lev < 64)
            e->st = ENV_SUSTAIN;
        break;
    case ENV_SUSTAIN:
        e->lev = sus;                              /* (SUSTAIN turned while held: at once) */
        break;
    case ENV_RELEASE:
        e->lev += (int32_t)(mul64(0 - e->lev, pole_k(cut ? 6u : env_ms(p[P_REL]))) >> 15);
        if (e->lev < 16) {
            e->lev = 0;
            e->st = ENV_OFF;
        }
        break;
    }
}

/* where the layer's engine id is in ENGINES[]; a missing one plays ENGINES[0] */
static uint32_t eng_index(uint32_t id, uint8_t *missing)
{
    uint32_t i;
    for (i = 0; i < NENGINES; i++)
        if (ENGINES[i].id == id) {
            *missing = 0;
            return i;
        }
    *missing = 1;
    return 0;
}
/* an engine id's place on the engine knob: this build's engines in order, then OFF (NENGINES) */
static uint32_t eng_pos(uint32_t id)
{
    uint8_t miss;
    return id == ENG_ID_OFF ? NENGINES : eng_index(id, &miss);
}

static void layer_pitch(track_t *t, voice_t *v)
{
    v->note = pf.note;
    v->pitch16 = v->pitch_cur = clamp(((int32_t)pf.note + pf.xpose + t->p[P_TRANS]) * 16, 0, 2047);
}

static uint32_t vage;
static void layer_start(uint32_t l)                /* a new note (not legato) */
{
    track_t *t = &trk[l];
    voice_t *v = &t->v[0];
    const engine_t *e = ENGINES[lay[l].idx].e;
    v->vel = pf.vel;
    v->gate = 1;
    v->active = 1;
    v->stage = 2;                                  /* (LOFI / WHEEL read it: "held") */
    lay[l].cut = 0;
    env_start(&lay[l].env, t->p);                 /* (a fresh attack: legato never gets here) */
    v->age = ++vage;
    layer_pitch(t, v);
    e->note_on(t, v);
}

static void notes_event(const pev_t *ev)
{
    uint32_t i, j, l;
    switch (ev->t) {
    case EV_ON:
        for (i = j = 0; i < pf.n; i++)             /* (again: to the top) */
            if (pf.stack[i] != ev->a) pf.stack[j++] = pf.stack[i];
        pf.n = (uint8_t)j;
        if (pf.n == sizeof pf.stack) {             /* full: the oldest goes */
            for (i = 1; i < pf.n; i++)
                pf.stack[i - 1] = pf.stack[i];
            pf.n--;
        }
        pf.stack[pf.n++] = ev->a;
        pf.note = ev->a;
        pf.vel = ev->b;
        if (pf.gate) {                             /* legato: the voices change pitch only */
            for (l = 0; l < NLAYER; l++)
                layer_pitch(&trk[l], &trk[l].v[0]);
        } else {
            pf.gate = 1;
            pf.xpose = (int8_t)gpar[G_XPOSE];
            for (l = 0; l < NLAYER; l++)
                if (trk[l].p[P_LEVEL] > 0 && !trk[l].p[P_MUTE] && trk[l].p[P_ENGINE] != ENG_ID_OFF)
                    layer_start(l);
        }
        break;
    case EV_OFF:
        for (i = j = 0; i < pf.n; i++)
            if (pf.stack[i] != ev->a) pf.stack[j++] = pf.stack[i];
        pf.n = (uint8_t)j;
        if (ev->a != pf.note)
            break;                                 /* a note under the top let go: nothing to hear */
        if (pf.n) {                                /* back to the note still held, legato */
            pf.note = pf.stack[pf.n - 1];
            for (l = 0; l < NLAYER; l++)
                layer_pitch(&trk[l], &trk[l].v[0]);
            break;
        }
        /* fall through: the last note let go */
    case EV_ALLOFF:
    case EV_CUT:
        if (ev->t != EV_OFF)
            pf.n = 0;
        pf.gate = 0;
        for (l = 0; l < NLAYER; l++) {
            trk[l].v[0].gate = 0;
            trk[l].v[0].stage = 3;
            if (ev->t == EV_CUT)
                lay[l].cut = 1;
        }
        break;
    case EV_BREATH:
        pf.braw = (int32_t)ev->a * 32767 / 127;
        pf.t_cc2 = pf.blocks | 1u;
        break;
    case EV_BEND:
        {   /* in 1/1600 semitone (1/16 cent; 8192 x 24 x 1600 fits): 1/16 semitones, the rest as fine tuning, so a
             * small bend (the tilt's: 10 % of 2 st) moves smoothly, not in 6.25-cent steps */
            int32_t c16 = ((((int32_t)ev->b << 7 | ev->a) - 8192) * gpar[G_BEND] * 1600) / 8192;
            pf.bend16 = c16 / 100;
            pf.bend_fine = (c16 - pf.bend16 * 100) * 2367 / 16000;
        }
        break;
    }
}

/* the breath, Q15, once a block */
static void breath_tick(void)
{
    int32_t src = gpar[G_BSRC], x;
    int use_cc2 = src == 1 || (src == 0 && pf.t_cc2 && pf.blocks - pf.t_cc2 < 3u * BLOCKS_PER_S);
    if (use_cc2)
        x = pf.braw;
    else
        x = pf.gate ? (int32_t)pf.vel * 32767 / 127 : 0;
    perf_ui.breath_in = x;
    if (use_cc2)
        x = breath_curve(x);                       /* (velocity is already what the player meant) */
    pf.breath += (int32_t)(mul64(x - pf.breath, pole_k((uint32_t)gpar[G_BSMOOTH])) >> 15);
}

/* one control block of every layer, added to l[] and r[] (CTL frames, Q15) */
static void layers_block(int32_t *outl, int32_t *outr)
{
    static int32_t buf[CTL];
    uint32_t l, i;
    while (pq_r != pq_w) {
        notes_event(&pq[pq_r % PQ]);
        pq_r++;
    }
    pf.blocks++;
    breath_tick();
    perf_ui.note = pf.note;
    perf_ui.gate = pf.gate;
    perf_ui.breath = pf.breath;
    for (l = 0; l < NLAYER; l++) {
        track_t *t = &trk[l];
        voice_t *v = &t->v[0];
        const int16_t *p = t->p;
        const engine_t *e;
        vmod_t m;
        int32_t target, a0, lev = p[P_LEVEL], pan = p[P_PAN];
        uint32_t base;
        if (p[P_ENGINE] == ENG_ID_OFF) {           /* no engine: silent, nothing rendered */
            if (lay[l].id != ENG_ID_OFF) {
                memset(v, 0, sizeof *v);
                lay[l].id = ENG_ID_OFF;
            }
            lay[l].amp = 0;
            lay[l].env.st = ENV_OFF;
            lay[l].env.lev = 0;
            perf_ui.amp[l] = 0;
            perf_ui.missing[l] = 0;
            v->active = 0;
            continue;
        }
        if ((uint32_t)p[P_ENGINE] != lay[l].id) {  /* another engine: silence, a clean voice */
            uint8_t miss;
            lay[l].id = (uint8_t)p[P_ENGINE];
            lay[l].idx = (uint8_t)eng_index(lay[l].id, &miss);
            perf_ui.missing[l] = miss;
            memset(v, 0, sizeof *v);
            lay[l].amp = 0;
            lay[l].env.st = ENV_OFF;
            lay[l].env.lev = 0;
            if (pf.gate && lev > 0)
                layer_start(l);
        }
        e = ENGINES[lay[l].idx].e;
        if (e->block)
            e->block(t);                           /* (also with no voice sounding: the interface says so) */
        if (p[P_MUTE] && v->gate) {                /* muted: it fades out as after a note (P_REL), no click */
            v->gate = 0;
            v->stage = 3;
        }
        if (lev > 0 && !p[P_MUTE] && pf.gate && (!v->active || !v->gate))
            layer_start(l);                        /* unmuted (or turned up) during a held note: in at once */
        if (lev <= 0 || !v->active) {
            lay[l].amp = 0;
            perf_ui.amp[l] = 0;
            v->active = 0;
            continue;
        }
        /* the volume: the envelope, times how much of it is breath (P_BR_AMP: 0 none, the default) */
        env_step(&lay[l].env, p, v->gate, lay[l].cut);
        target = 32767 - ((32767 - pf.breath) * p[P_BR_AMP]) / 127;
        a0 = lay[l].amp;
        lay[l].amp = (int32_t)(mul64(lay[l].env.lev, target) >> 15);
        if (!v->gate && lay[l].env.st == ENV_OFF) { /* faded out */
            lay[l].amp = 0;
            v->active = 0;
            v->stage = 0;
        }
        v->env = lay[l].amp << 9;                  /* Q24, for engines that read it */
        v->env_out = lay[l].amp;
        layer_pitch(t, v);                         /* (TRANS may have moved) */
        base = pitch_inc((uint32_t)clamp(v->pitch16 + pf.bend16, 0, 2047));
        memset(&m, 0, sizeof m);
        m.pitch16 = v->pitch16 + pf.bend16;
        m.fine = p[P_FINE] * 2367 / 1000 + pf.bend_fine;   /* cents -> 1/4096 of the increment; the bend's rest */
        m.inc = base + (uint32_t)((int32_t)(base >> 12) * m.fine);
        m.amp0 = a0;
        m.amp1 = lay[l].amp;
        if (e->amp)
            m.amp1 = e->amp(t, v, m.amp1);         /* (LOFI's stepped envelope) */
        m.envq15 = m.amp1;
        m.cutoff = (pf.breath * p[P_BR_FLT]) >> 7;
        m.shape = (64 << 8) + ((pf.breath * p[P_BR_SHP]) >> 7);
        perf_ui.amp[l] = m.amp1;
        memset(buf, 0, sizeof buf);
        e->render(t, v, buf, CTL, &m);
        tone_block(l, buf, CTL, p, &m, pf.breath);   /* the TONE stage (core/tone.c) */
        {                                          /* level and pan (centre: both sides at the level) */
            int32_t gl = (64 - pan) * lev, gr = (64 + pan) * lev;     /* / (64 * 127) */
            for (i = 0; i < CTL; i++) {
                outl[i] += (buf[i] * (gl >> 4)) >> 9;
                outr[i] += (buf[i] * (gr >> 4)) >> 9;
            }
        }
    }
}

static void fx_block(int32_t *l, int32_t *r, int32_t breath);   /* core/fx.c */

/* the audio ISR's whole job: n frames (a multiple of CTL), L R L R, Q15, soft above ~-2 dBFS */
static void layers_audio(int32_t *out, uint32_t n)
{
    int32_t l[CTL], r[CTL];
    uint32_t b, i;
    for (b = 0; b + CTL <= n; b += CTL) {
        memset(l, 0, sizeof l);
        memset(r, 0, sizeof r);
        layers_block(l, r);
        fx_block(l, r, pf.breath);
        for (i = 0; i < CTL; i++) {
            out[2u * (b + i)] += soft_knee(l[i], 26000);
            out[2u * (b + i) + 1u] += soft_knee(r[i], 26000);
        }
    }
}

static int layers_quiet(void)                      /* nothing sounds, not even a fade (flash erases stall the audio) */
{
    uint32_t l;
    for (l = 0; l < NLAYER; l++)
        if (trk[l].v[0].active) return 0;
    return 1;
}
