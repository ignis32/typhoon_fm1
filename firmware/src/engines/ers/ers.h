/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* ERS: exciter -> resonator -> shaper, a small physical-modelling pipeline. Breath is the pressure into the
 * exciter (BLOW, + vmod.shape): blown softly it is air, harder a full tone. The resonator follows the note.
 *
 * Page 1:  BLOW       the pressure (what breath moves; 64 + breath at the top is a full tone, more overblows)
 *          EXCITER    NOISE    air (a flute's jet in a bore)
 *                     REED     a single reed (a clarinet's in a bore)
 *                     OSC      a saw .. pulse
 *                     BOW      a bowed string (STK Bowed, Cook & Scavone): the bow sticks and slips on the string,
 *                              breath is the bow's speed. In a bore the string is the sound; the other resonators
 *                              take it in. (It was IMPULSE, clicks: no use under breath)
 *                     LIP      a brass player's lips: a valve that shuts hard, blown hard (a lip model proper, STK
 *                              Brass, would not play over the range); LIPS loose .. tight (brighter, a higher threshold)
 *                     DBL REED an oboe's / bassoon's reed: closes harder (a squarer valve)
 *                     SAX REED a saxophone's reed; BITE: how hard it is bitten (a smaller opening, brighter)
 *                     JET      a flute's jet with turbulence that follows the air flow, and a chiff on a fresh note
 *                     FREE REED a harmonica's / an accordion's reed: it swings at its own pitch through a slot and
 *                              chops the air into pulses; louder and brighter blown harder, a little flat (no bore
 *                              needed: FORMANT is a mouth, CAVITY a reed chamber)
 *                     FM       two operators, the index breath's (soft pure, hard brassy). In a bore the tube's
 *                              returning wave goes into the carrier's phase (feedback FM through a tuned delay):
 *                              DECAY (FEEDBK) low a hollow, woody body, high a growl and chaos
 *          COLOR      the exciter's: AIR, STIFF, WAVE, PRESSURE (the bow's), LIPS, STIFF, BITE, TURBUL, RASP (the
 *                     slot's overlap: round .. rasping), RATIO (FM's modulator: 0.5 .. 7)
 *          RESONATOR  TUBE     a cylinder: a flute (a jet), a clarinet (a reed), a comb (the others)
 *                     MODAL    five band-passes at a free bar's modes (1, 2.76, 5.40, 8.93, 13.3)
 *                     FORMANT  three vowel formants (VOWEL's), not following the note: a voice
 *                     BYPASS   the exciter as it is
 *                     CONE     a conical bore (STK Saxofony: two lines, the reed's POSITN between): all harmonics,
 *                              a saxophone / oboe / bassoon with a reed
 *                     BRASS    a brass bore: the cone's loop with a bell's radiation on it
 *                     HOLE     a cylinder with a register VENT a third along: closed a clarinet, half open it
 *                              wavers and breaks, open it flips to the twelfth
 *                     PIPE     a flue pipe (a recorder, an organ): the jet through a windway, steadier, purer
 *                     CAVITY   one resonance that does not follow the note, over the dry sound: a reed chamber, a
 *                              mouth. BRIGHT is its FREQ (150 Hz .. 6 kHz), DECAY its RES
 *                     SAX      a saxophone's bore: CONE's loop (POSITN), its losses a low-pass that opens with the
 *                              pressure (blown softly little but the fundamental comes back, near a sine; hard, the
 *                              harmonics build up), made up at the note so it starts where CONE does; BRIGHT how far
 *                              it opens. The air at the reed: noise around 1.5 kHz as strong as the pressure, into the
 *                              reed (the tone wavers with it) and out; a short dip of the pressure on a slur
 *          (A jet in CONE, BRASS, HOLE or SAX plays them as TUBE; a reed or lips in PIPE as TUBE; BOW in any bore is
 *          the string.)
 * Page 2:  DECAY      the resonator's: EMBOUCH (the jet's length: shorter overblows), VOICING (PIPE's jet),
 *                     FEEDBK, POSITN (CONE, BRASS, SAX), VENT (HOLE), SUSTAIN (the string), DECAY (MODAL), RES
 *                     (FORMANT)
 *          BRIGHT     the resonator's damping (VOWEL for FORMANT: U O A E I)
 *          SHAPER     SAT, FOLD, FILTER (a low-pass following the note), BYPASS,
 *                     BELL (a bell's radiation: less low end, a presence peak), MUTE (a brass mute: straight 0,
 *                     cup 50 %, harmon 100 %), WAH (a plunger: AMOUNT opens it), BODY (a wooden body's three
 *                     resonances), AIR (breath noise on top, around the note), GROWL (a growl / flutter: the
 *                     pressure flutters ~30 Hz, a half-pitch roughness)
 *          AMOUNT     the shaper's (+ vmod.cutoff: BR>TONE moves it)
 * Page 3:  VIB        vibrato depth (0 .. 50 cents): in above 55 % of a full tone's pressure, as fast as VIB SWELL says
 *          RATE       its rate, 3 .. 8 Hz (64: 5.5)
 *          TONGUE     a kick of the pressure on an articulated note, up to +50 %, gone in 40 ms: a bark, a bite
 *          LAG        how slowly the pressure follows the breath (0: at once .. ~90 ms): a heavier instrument. A lag
 *                     of the engine's own, visible (gone 2026-10-23 .. 28; the presets that had it have it again)
 * Page 5:  LP CUT, LP RES, BR>LP  the breath low-pass (dsp/breathlp.c: the mix's until 2026-10-28)
 *          VIB SWELL  how long the vibrato takes to come in (0: at once; 64: 400 ms, out in 150, as it was fixed
 *                     until 2026-10-28; 127: ~1.6 s)
 * Page 4:  AIR        the breath noise (the jet's, the mouth's, the sax's air, the AIR shaper's): 0 none, 64 as
 *                     modelled, 127 twice
 *          TILT       the tone's tilt around ~800 Hz: darker .. brighter (64: none)
 *          PR>PITCH   the pitch with the pressure: blown harder flat .. sharp, +-30 cents at full (64: none)
 *          DRIFT      a slow wander of the pitch, up to +-8 cents (0: none)
 * (All at their defaults: the sound as before they were there.)
 *
 * Pitch bend (the re.corder's tilt) moves the note's increment, so the bores' lengths follow it smoothly (a slide)
 * too. Partials change with breath (an overblow), never with the bend.
 *
 * The parts (one translation unit, eng_ers.c includes them in this order):
 *   ers.h             this: the choices, a layer's state, a block's context, the small helpers
 *   ers_valve.c       a reed's / the lips' valve and the breath's window onto it
 *   ers_exciters.c    the feed-forward exciters (into a resonator after them)
 *   ers_bores.c       the coupled models: the exciter is part of the loop (flute, flue pipe, cylinder, vented
 *                     cylinder, cone, FM in a bore, bowed string)
 *   ers_resonators.c  the feed-forward resonators: a comb a period long, modal, formant, cavity
 *   ers_shapers.c     the shapers, and the growl's flutter of the pressure; then out
 *   eng_ers.c         the presets, which model plays, the knobs' labels, the render (the stages in order)
 *
 * The physics run in Q14 (1.0 = 16384); the delay lines are int16 at half scale (+-4.0). A layer is one voice, so
 * each layer has its own two lines (ers_st, ~6 KB a layer), shared by whichever bore it plays (HOLE splits the long
 * one in two); cleared when the model changes, and when the layer takes this engine (v->s[7]). The note's period
 * comes from the phase increment: 2^32 / inc samples (no 64-bit division: inc >> 8, ~0.1 cent). */

#define ERS_BORE 2048u                             /* down to ~22 Hz */
#define ERS_JET 1024u
#define ERS_HALF 1024u                             /* (HOLE: the bore line as two) */
#define ERS_NM 5
#define ERS_ONE 16384
#define ERS_MAGIC 0x45525323
/* ERS's fixed numbers that delay the response (notes/03-lag-watch.md; the rest of its numbers still inline, to name) */
#define ERS_FREE_SWING 48                          /* FREE REED: the reed's swing follows the breath 48 / 16384 a
                                                    * sample, ~8 ms: a free reed speaks softly, building up (kept,
                                                    * 2026-10-28) */
#define ERS_FLUE_WW 1200                           /* FLUE: the windway steadies the jet, 1200 / 16384 a sample, ~0.3
                                                    * ms (a pipe's jet, not the player's breath) */

enum { EX_NOISE, EX_REED, EX_OSC, EX_BOW, EX_LIP, EX_DREED, EX_SAX, EX_JET, EX_FREE, EX_FM, ERS_NEXC };
enum { RS_TUBE, RS_MODAL, RS_FORMANT, RS_BYPASS, RS_CONE, RS_BRASS, RS_HOLE, RS_PIPE, RS_CAVITY, RS_SAX, ERS_NRES };
enum { SH_SAT, SH_FOLD, SH_FILTER, SH_BYPASS, SH_BELL, SH_MUTE, SH_WAH, SH_BODY, SH_AIR, SH_GROWL, ERS_NSHP };
enum { M_FF, M_FLUTE, M_FLUE, M_CYL, M_VENT, M_CONE, M_BOW, M_FMB, M_SAX };   /* how exciter and resonator play */
static const char *const ERS_N_EXC[] = {"NOISE", "REED", "OSC", "BOW", "LIP", "DBL REED", "SAX REED", "JET", "FREE REED", "FM"};
static const char *const ERS_N_RES[] = {"TUBE", "MODAL", "FORMANT", "BYPASS", "CONE", "BRASS", "HOLE", "PIPE", "CAVITY",
                                        "SAX"};
static const char *const ERS_N_SHP[] = {"SAT", "FOLD", "FILTER", "BYPASS", "BELL", "MUTE", "WAH", "BODY", "AIR",
                                        "GROWL"};

/* a layer's state */
typedef struct {
    int16_t bore[ERS_BORE], jet[ERS_JET];
    uint32_t w;                                    /* the write index (every line) */
    int32_t model;                                 /* the model the lines hold, + 1 */
    int32_t lp, dcx, dcy;                          /* the loop's damping, its DC blocker */
    int32_t ox, oy;                                /* the output's DC blocker */
    int32_t nlp, pp, gg;                           /* the noise's colour; the last block's pressure, growl gain */
    int32_t vl, bl, ww, ch;                        /* the vent's leak, the bell, the windway; chiff samples left */
    uint32_t gph, sub;                             /* the growl's flutter, its half-pitch phase */
    int32_t ex[2];                                 /* a lone JET's edge tone */
    int32_t fa;                                    /* FREE REED: its swing */
    int32_t sl, sn[2], dip;                        /* SAX: the loop's low-pass, the air's band-pass, the slur's dip */
    uint32_t sinc;                                 /* SAX: the last block's increment (a slur: it jumps) */
    int32_t md[ERS_NM][2], fm[3][2], sh[3][2];     /* modes, formants, the shaper's filters */
    int32_t seed;                                  /* noise */
    int32_t air;                                   /* AIR this block (64: as modelled) */
    int32_t zv, tg, pl, tl;                        /* the vibrato's onset; TONGUE's kick; LAG's pressure; TILT's low-pass */
    int32_t dr, dt, dn;                            /* DRIFT: where the pitch is, where it goes, blocks to the next */
    uint32_t vph;                                  /* the vibrato's phase */
} ers_t;
static ers_t ers_st[NLAYER] PLAT_POOL;                /* (~6 KB a layer: in the pool, zeroed at boot; fast RAM
                                                    * is the platform's and Bluetooth's) */

/* one block: what every stage reads (eng_ers.c ers_render fills it), and the samples between the stages */
typedef struct {
    ers_t *st;
    voice_t *v;
    const vmod_t *m;
    int32_t exc, res, shp, model;                  /* the choices (clamped), the model they make */
    int32_t col, dec, bri, amt;                    /* COLOR, DECAY, BRIGHT; AMOUNT (+ vmod.cutoff), 0 .. 127 << 8 */
    int32_t tilt;                                  /* TILT, Q14 (-1 .. +1; 0: none) */
    int32_t p0, dp;                                /* the pressure over the block, Q14: p0 + dp x i / CTL */
    uint32_t per8, n;                              /* the note's period in samples, Q8; the block's length */
    int32_t x[CTL];
} ers_blk_t;
static inline int32_t ers_p(const ers_blk_t *b, uint32_t i) { return b->p0 + ((b->dp * (int32_t)i) >> CTL_LOG2); }

/* ---- the small helpers */
static inline int32_t ers_tap(const int16_t *b, uint32_t mask, uint32_t w, uint32_t d8)   /* d8 >= 1 << 8 */
{
    uint32_t i = (w - (d8 >> 8)) & mask;
    int32_t a = b[i], c = b[(i - 1u) & mask];
    return (a + (((c - a) * (int32_t)(d8 & 255u)) >> 8)) << 1;   /* (the lines hold half: +-4.0) */
}
/* the breath noise (the jet's, the mouth's, the sax's air, the AIR shaper's): white, >> sh, at AIR (64: x 1, exact) */
static inline int32_t ers_air(ers_t *st, int sh) { return (((int32_t)noise32(&st->seed) >> sh) * st->air) >> 6; }
static inline int16_t ers_s16(int32_t x) { return (int16_t)clamp(x >> 1, -32767, 32767); }
static inline uint32_t ers_len(int32_t d8, uint32_t size) { return (uint32_t)clamp(d8, 256, (int32_t)(size - 2u) << 8); }

/* (the SVF's step, band-pass and low-pass: dsp/dspx.c tsvf_step_cl, shared; ERS's own copy until 2026-10-28) */

static uint32_t ers_isqrt(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}

/* the note in the filters' cutoff scale (<< 8): 440 Hz = 13905, 18.69 a 1/16 semitone */
static inline int32_t ers_cut(int32_t pitch16) { return 13905 + (((pitch16 - 69 * 16) * 1196) >> 6); }

static inline int ers_is_jet(int32_t e) { return e == EX_NOISE || e == EX_JET; }
static inline int ers_is_valve(int32_t e) { return e == EX_REED || e == EX_LIP || e == EX_DREED || e == EX_SAX; }
static inline int ers_is_bore(int32_t r) { return r == RS_TUBE || (r >= RS_CONE && r <= RS_PIPE) || r == RS_SAX; }
