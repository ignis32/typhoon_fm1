/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* Presets and the saved state.
 *
 * A preset is the whole sound: the layers (P_*), the sound groups it has its OWN copy of (S_*, s_own; the
 * groups it does not own play the global copy) and a name. Not in it: the G_* globals and the global copy of
 * the sound groups (sglob): those are the instrument's, the same for every preset.
 *
 * The slots: NPRESET of PS_SLOT bytes in the store (app/app.c store_*: fm1-base's data area at
 * 0x97000, which stock V15 wipes and our own updates keep), 8 to a 4 KiB sector, each with a second copy: slots
 * 1 .. 64 in store sectors 0 .. 7, their copy in 8 .. 15 (PS_COPY on: where they always were); sector 16 is the
 * BT stack's (ble.c); slots 65 .. 96 (2026-10-20: 80, 2026-10-21: 96) in sectors 17 .. 20, their copy in 21 .. 24
 * (ps_sec). A save writes
 * the copy's sector first, then the main one, so a power cut leaves one good record; every record has a CRC, and
 * a read takes the main record, else the copy. (New sectors hold whatever was there: no good record, the slot
 * plays its factory preset until saved.)
 * A slot never saved plays a factory preset (FACTORY[k]) when there is one, else INIT (one sine).
 * A record keeps the counts it was written with (layers, P_COUNT, S_COUNT) and is read by them: its last NEPAR (16)
 * layer values are P_E0..P_E15, the ones before are the commons in order, S_* go by index. (Records from before
 * 2026-10-21 held 8 engine values: not read right, there were none outside the factory.) New parameters therefore go
 * just before P_E0 (layers.h) or at the end of S_*, and take their defaults in older records; the record has room
 * for 4 layers of 40 and 48 S_*.
 *
 * The state (fm1-base's store, OBJ_STATE: app/app.c settings_*): the globals, the global sound, the edit buffer (what
 * plays now, edited or not, and its slot), so a power cycle brings back exactly what was playing. Saved by the
 * main loop 2 s after the last change, when nothing sounds. */

#define NPRESET 96u
#define PS_SLOT 512u
#define PS_PER_SEC (0x1000u / PS_SLOT)
#define PS_LO 64u                                  /* slots 1 .. 64: sectors 0 .. 7 */
#define PS_COPY (PS_LO / PS_PER_SEC * 0x1000u)     /* their copy (32 KiB on: sectors 8 .. 15) */
#define PS_HI_AT (17u * 0x1000u)                   /* slots 65 .. 96: sectors 17 .. 20 (after the BT stack's 16) */
#define PS_HI_COPY ((NPRESET - PS_LO) / PS_PER_SEC * 0x1000u)   /* their copy right after (sectors 21 .. 24) */
#define PS_MAGIC 0x31505254u                       /* "TRP1" */
#define PS_NL 4u
#define PS_NP 40u
#define PS_NS 48u
#define PS_NAME 12u
typedef struct {
    uint32_t magic, crc;                           /* crc: of every byte after it */
    uint8_t ver, nl, np, ns;                       /* 1; NLAYER, P_COUNT, S_COUNT when written */
    uint8_t own, spare[3];                         /* s_own */
    char name[PS_NAME];                            /* ASCII, 0-padded (no 0 when 12 long) */
    int16_t p[PS_NL][PS_NP];
    int16_t s[PS_NS];                              /* sown (every group's, owned or not) */
    uint8_t reserved[PS_SLOT - 28u - PS_NL * PS_NP * 2u - PS_NS * 2u];
} ps_rec_t;
_Static_assert(sizeof(ps_rec_t) == PS_SLOT, "preset record layout");
_Static_assert(NLAYER <= PS_NL && P_COUNT <= PS_NP && S_COUNT <= PS_NS && NGROUP <= 8u, "preset record: room");
_Static_assert(2u * PS_COPY <= 16u * 0x1000u && PS_HI_AT + 2u * PS_HI_COPY <= STORE_LEN && NPRESET % PS_PER_SEC == 0u,
               "presets: the store (sector 16 is the BT stack's)");

/* ---- the factory presets: changes to the INIT sound, in order (an engine before its layer's parameters) */
enum { FP_END, FP_ENG, FP_PAR, FP_SND };
typedef struct { uint8_t op, l, i; int16_t v; } fp_t;
typedef struct { const char *name; fp_t f[24]; } factory_t;
#define ENG(l, id, pre) {FP_ENG, (l), (id), (pre)}   /* layer l plays engine id, with that engine's preset pre */
#define PAR(l, i, v) {FP_PAR, (l), (i), (v)}
#define SND(i, v) {FP_SND, 0, (i), (v)}              /* the preset owns i's group, i = v */
static const factory_t FACTORY[] = {
    /* 1 .. 14: the main ones */
    {"RECORDER", {ENG(0, ENG_ID_ERS, 19), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30), SND(S_REV_PRE, 20)}},
    {"PIPE FLUTE", {ENG(0, ENG_ID_ERS, 0), ENG(1, ENG_ID_SINE, 1), PAR(1, P_LEVEL, 20), SND(S_REV_MIX, 40)}},
    /* CLARINET (made on the device, then taken in here): ERS CLARINET through FORMANT,
     * an ERS OSC layer under it.
     * Every value of the groups it owns is given: owning a group starts from the global copy, whatever that is */
    {"CLARINET", {ENG(0, ENG_ID_ERS, 1), PAR(0, P_E2, 90), PAR(0, P_E3, 2), PAR(0, P_E4, 101), PAR(0, P_E5, 65),
                  ENG(1, ENG_ID_ERS, 7), PAR(1, P_LEVEL, 46), PAR(1, P_E0, 9), PAR(1, P_E2, 30), PAR(1, P_E4, 38),
                  PAR(1, P_E5, 23), PAR(1, P_E6, 0), PAR(1, P_E7, 0),
                  SND(S_DRIVE, 6), SND(S_DRV_AT, 62),
                  SND(S_REV_SIZE, 90), SND(S_REV_DECAY, 70), SND(S_REV_DAMP, 50), SND(S_REV_MIX, 30),
                  SND(S_DT_CENTS, 9), SND(S_DT_DELAY, 10), SND(S_DT_WIDTH, 127), SND(S_DT_MIX, 12),
                  SND(S_VOLUME, 90)}},   /* (VOLUME 90 since the clean PLATE, 2026-10-25: at 100 it touched the clip) */
    {"OBOE", {ENG(0, ENG_ID_ERS, 16), SND(S_VOLUME, 45), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 34), SND(S_REV_PRE, 30)}},
    {"DOUBLE REEDS", {ENG(0, ENG_ID_ERS, 16), SND(S_VOLUME, 26), PAR(0, P_PAN, -18), ENG(1, ENG_ID_ERS, 17), PAR(1, P_TRANS, -12),
                      PAR(1, P_LEVEL, 55), PAR(1, P_PAN, 18), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 36), SND(S_REV_PRE, 35)}},
    {"BOWED CELLO", {ENG(0, ENG_ID_ERS, 8), SND(S_VOLUME, 40), PAR(0, P_TRANS, -24), PAR(0, P_E6, 7), PAR(0, P_E7, 50),
                     SND(S_REV_TYPE, 1), SND(S_REV_MIX, 36), SND(S_REV_PRE, 30)}},
    {"VOX FLUTE", {ENG(0, ENG_ID_VOWEL, 0), ENG(1, ENG_ID_SINE, 1), PAR(1, P_LEVEL, 60), ENG(2, ENG_ID_OFF, 0),
                   PAR(2, P_LEVEL, 12), SND(S_REV_MIX, 40)}},
    {"FM TUBE", {ENG(0, ENG_ID_ERS, 27), SND(S_VOLUME, 46), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 26)}},
    {"FM WOOD", {ENG(0, ENG_ID_ERS, 28), SND(S_VOLUME, 30), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30)}},
    {"FM GROWL", {ENG(0, ENG_ID_ERS, 29), SND(S_VOLUME, 57), SND(S_DRIVE, 20), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 20)}},
    {"TALK LEAD", {ENG(0, ENG_ID_ANALOG, 0), SND(S_VOLUME, 65), PAR(0, P_TONE, TN_TALK), PAR(0, P_TONE_A, 10), PAR(0, P_TONE_B, 55),
                   PAR(0, P_TONE_C, 75), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 24)}},
    {"SWEEP LEAD", {ENG(0, ENG_ID_ANALOG, 0), SND(S_VOLUME, 31), PAR(0, P_TONE, TN_SWEEP), PAR(0, P_TONE_A, 40), PAR(0, P_TONE_B, 100),
                    PAR(0, P_TONE_C, 55), SND(S_DLY_MIX, 22), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 22)}},
    {"FILTER SAX", {ENG(0, ENG_ID_ERS, 14), PAR(0, P_TRANS, -12), PAR(0, P_TONE, TN_FILTER), PAR(0, P_TONE_A, 55),
                    PAR(0, P_TONE_B, 85), PAR(0, P_TONE_C, 0), PAR(0, P_BR_FLT, 50), SND(S_REV_TYPE, 0),
                    SND(S_REV_MIX, 26)}},
    {"BLUES HARP", {ENG(0, ENG_ID_ERS, 24), SND(S_VOLUME, 59), SND(S_DRIVE, 30), SND(S_DRV_AT, 70), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 18),
                    SND(S_DLY_MIX, 14)}},

    /* 15 .. 64: more: the physical ones, FM in a tube, the TONE leads (ERS engine presets 30 .. 53: eng_ers.c) */
    /* flutes */
    {"PICCOLO", {ENG(0, ENG_ID_ERS, 30), SND(S_VOLUME, 86), PAR(0, P_TRANS, 12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 32), SND(S_REV_PRE, 25)}},
    {"ALTO FLUTE", {ENG(0, ENG_ID_ERS, 31), SND(S_VOLUME, 69), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 36), SND(S_REV_PRE, 30)}},
    {"BANSURI", {ENG(0, ENG_ID_ERS, 32), SND(S_VOLUME, 46), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 45), SND(S_REV_DECAY, 100),
                 SND(S_REV_PRE, 40)}},
    {"SHAKUHACHI", {ENG(0, ENG_ID_ERS, 21), SND(S_VOLUME, 61), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 50), SND(S_REV_DECAY, 110),
                    SND(S_REV_PRE, 50)}},
    {"PAN FLUTE", {ENG(0, ENG_ID_ERS, 33), SND(S_VOLUME, 86), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 42), SND(S_REV_PRE, 35), SND(S_DLY_MIX, 12)}},
    {"OCARINA", {ENG(0, ENG_ID_ERS, 34), SND(S_VOLUME, 127), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 30)}},
    {"TIN WHISTLE", {ENG(0, ENG_ID_ERS, 35), SND(S_VOLUME, 127), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 28), SND(S_DLY_MIX, 15)}},
    /* an organ's flue pipes: 8' and 4', a 2' quietly over them */
    {"FLUE ORGAN", {ENG(0, ENG_ID_ERS, 20), SND(S_VOLUME, 48), PAR(0, P_TRANS, -12), ENG(1, ENG_ID_ERS, 20), PAR(1, P_LEVEL, 70),
                    ENG(2, ENG_ID_ERS, 20), PAR(2, P_TRANS, 12), PAR(2, P_LEVEL, 40), SND(S_REV_TYPE, 1),
                    SND(S_REV_MIX, 50), SND(S_REV_DECAY, 115), SND(S_REV_SIZE, 127), SND(S_REV_PRE, 50)}},
    /* single reeds */
    {"BASS CLARINET", {ENG(0, ENG_ID_ERS, 36), SND(S_VOLUME, 47), PAR(0, P_TRANS, -24), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30)}},
    {"CHALUMEAU", {ENG(0, ENG_ID_ERS, 22), SND(S_VOLUME, 127), PAR(0, P_E4, 0), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 32), SND(S_REV_PRE, 25)}},
    {"SOPRANO SAX", {ENG(0, ENG_ID_ERS, 37), SND(S_VOLUME, 127), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 28)}},
    {"ALTO SAX", {ENG(0, ENG_ID_ERS, 38), SND(S_VOLUME, 119), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 26), SND(S_DLY_MIX, 10)}},
    {"BARI SAX", {ENG(0, ENG_ID_ERS, 39), SND(S_VOLUME, 56), PAR(0, P_TRANS, -24), SND(S_DRIVE, 15), SND(S_DRV_AT, 80), SND(S_REV_TYPE, 0),
                  SND(S_REV_MIX, 22)}},
    /* the clarinet with an octave under it (TONE SUB OCT, more of it blown harder) */
    {"SUB CLARINET", {ENG(0, ENG_ID_ERS, 1), SND(S_VOLUME, 40), PAR(0, P_TRANS, -12), PAR(0, P_TONE, TN_SUB), PAR(0, P_TONE_A, 60),
                      PAR(0, P_TONE_B, 70), PAR(0, P_TONE_C, 10), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 28)}},
    /* double reeds */
    {"ENGLISH HORN", {ENG(0, ENG_ID_ERS, 40), SND(S_VOLUME, 43), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 36), SND(S_REV_PRE, 30)}},
    {"BASSOON", {ENG(0, ENG_ID_ERS, 17), SND(S_VOLUME, 36), PAR(0, P_TRANS, -24), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 32), SND(S_REV_PRE, 25)}},
    {"CRUMHORN", {ENG(0, ENG_ID_ERS, 18), SND(S_VOLUME, 33), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 38), SND(S_REV_PRE, 30)}},
    /* a shawm (a loud outdoor oboe), a second a fifth under it, as a pair would play */
    {"SHAWM", {ENG(0, ENG_ID_ERS, 41), SND(S_VOLUME, 25), PAR(0, P_PAN, -15), ENG(1, ENG_ID_ERS, 41), PAR(1, P_TRANS, -7), PAR(1, P_LEVEL, 55),
               PAR(1, P_PAN, 15), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 34)}},
    /* free reeds */
    {"MELODICA", {ENG(0, ENG_ID_ERS, 42), SND(S_VOLUME, 71), PAR(0, P_FINE, 12), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 22), SND(S_DLY_MIX, 12)}},
    /* a mouth organ of pipes: the note, its fifth and octave above, quieter */
    {"SHENG", {ENG(0, ENG_ID_ERS, 43), PAR(0, P_FINE, 12), SND(S_VOLUME, 35), ENG(1, ENG_ID_ERS, 43), PAR(1, P_FINE, 12), PAR(1, P_TRANS, 7), PAR(1, P_LEVEL, 50),
               ENG(2, ENG_ID_ERS, 43), PAR(2, P_FINE, 12), PAR(2, P_TRANS, 12), PAR(2, P_LEVEL, 40), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 36)}},
    /* a tremolo harmonica: two reed plates, one a little sharp (the beating) */
    {"TREMOLO HARP", {ENG(0, ENG_ID_ERS, 23), SND(S_VOLUME, 24), PAR(0, P_PAN, -20), ENG(1, ENG_ID_ERS, 23), PAR(1, P_FINE, 8), PAR(1, P_PAN, 20),
                      SND(S_REV_TYPE, 1), SND(S_REV_MIX, 26)}},
    {"BASS HARP", {ENG(0, ENG_ID_ERS, 24), SND(S_VOLUME, 53), PAR(0, P_TRANS, -12), PAR(0, P_TONE, TN_SUB), PAR(0, P_TONE_A, 50),
                   PAR(0, P_TONE_B, 50), PAR(0, P_TONE_C, 40), SND(S_DRIVE, 25), SND(S_DRV_AT, 70), SND(S_REV_TYPE, 0),
                   SND(S_REV_MIX, 18)}},
    /* strings (the bow: breath is its speed) */
    {"VIOLIN", {ENG(0, ENG_ID_ERS, 44), SND(S_VOLUME, 46), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 34), SND(S_REV_PRE, 25)}},
    {"VIOLA", {ENG(0, ENG_ID_ERS, 45), SND(S_VOLUME, 36), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 34), SND(S_REV_PRE, 25)}},
    {"DOUBLE BASS", {ENG(0, ENG_ID_ERS, 46), SND(S_VOLUME, 41), PAR(0, P_TRANS, -24), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 26)}},
    {"ERHU", {ENG(0, ENG_ID_ERS, 47), SND(S_VOLUME, 38), PAR(0, P_FINE, 12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 40), SND(S_REV_PRE, 35)}},
    /* violin, viola and cello on one line, a little apart */
    {"STRING TRIO", {ENG(0, ENG_ID_ERS, 44), SND(S_VOLUME, 19), PAR(0, P_PAN, -25), ENG(1, ENG_ID_ERS, 45), PAR(1, P_TRANS, -12),
                     PAR(1, P_FINE, -5), PAR(1, P_LEVEL, 75), ENG(2, ENG_ID_ERS, 8), PAR(2, P_TRANS, -24), PAR(2, P_FINE, 4),
                     PAR(2, P_LEVEL, 70), PAR(2, P_PAN, 25), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 40), SND(S_REV_DECAY, 100),
                     SND(S_REV_PRE, 35)}},
    {"GLASS HARMONICA", {ENG(0, ENG_ID_ERS, 48), SND(S_VOLUME, 18), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 50), SND(S_REV_DECAY, 95),
                         SND(S_REV_SIZE, 120), SND(S_REV_PRE, 40)}},
    /* brass */
    {"FLUGELHORN", {ENG(0, ENG_ID_ERS, 12), SND(S_VOLUME, 40), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 34), SND(S_REV_PRE, 25)}},
    {"FRENCH HORN", {ENG(0, ENG_ID_ERS, 13), SND(S_VOLUME, 42), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 48), SND(S_REV_DECAY, 100),
                     SND(S_REV_PRE, 40)}},
    {"TROMBONE", {ENG(0, ENG_ID_ERS, 49), SND(S_VOLUME, 47), PAR(0, P_TRANS, -24), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30)}},
    {"HARMON TPT", {ENG(0, ENG_ID_ERS, 10), SND(S_VOLUME, 108), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 26), SND(S_DLY_MIX, 14)}},
    /* FM in a tube */
    {"FM FLUTE", {ENG(0, ENG_ID_ERS, 50), SND(S_VOLUME, 29), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 38), SND(S_REV_PRE, 30)}},
    {"FM REED", {ENG(0, ENG_ID_ERS, 51), SND(S_VOLUME, 43), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 26)}},
    {"FM BRASS", {ENG(0, ENG_ID_ERS, 52), SND(S_VOLUME, 43), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30)}},
    /* an organ of FM tubes: 8' and 4', ratio 1 and the wood's */
    {"FM ORGAN", {ENG(0, ENG_ID_ERS, 27), SND(S_VOLUME, 21), PAR(0, P_TRANS, -12), ENG(1, ENG_ID_ERS, 28), PAR(1, P_LEVEL, 60), SND(S_REV_TYPE, 1),
                  SND(S_REV_MIX, 42), SND(S_REV_DECAY, 105), SND(S_REV_PRE, 40)}},
    {"FM BASS", {ENG(0, ENG_ID_ERS, 28), SND(S_VOLUME, 43), PAR(0, P_TRANS, -24), PAR(0, P_TONE, TN_SUB), PAR(0, P_TONE_A, 55),
                 PAR(0, P_TONE_B, 40), PAR(0, P_TONE_C, 0), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 12)}},
    {"FM CHAOS", {ENG(0, ENG_ID_ERS, 53), SND(S_VOLUME, 44), PAR(0, P_TRANS, -12), SND(S_DLY_MIX, 20), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 26)}},
    /* TONE: basses */
    {"TALK BASS", {ENG(0, ENG_ID_ANALOG, 2), SND(S_VOLUME, 43), PAR(0, P_TRANS, -24), PAR(0, P_TONE, TN_TALK), PAR(0, P_TONE_A, 20),
                   PAR(0, P_TONE_B, 60), PAR(0, P_TONE_C, 70), SND(S_REV_MIX, 10)}},
    {"SWEEP BASS", {ENG(0, ENG_ID_ANALOG, 0), SND(S_VOLUME, 29), PAR(0, P_TRANS, -24), PAR(0, P_TONE, TN_SWEEP), PAR(0, P_TONE_A, 25),
                    PAR(0, P_TONE_B, 110), PAR(0, P_TONE_C, 45), SND(S_REV_MIX, 10)}},
    {"ACID BREATH", {ENG(0, ENG_ID_ANALOG, 4), SND(S_VOLUME, 24), PAR(0, P_TRANS, -12), PAR(0, P_TONE, TN_SWEEP), PAR(0, P_TONE_A, 30),
                     PAR(0, P_TONE_B, 120), PAR(0, P_TONE_C, 80), SND(S_DLY_MIX, 25), SND(S_REV_MIX, 14)}},
    /* TONE over the physical ones */
    {"TALK SAX", {ENG(0, ENG_ID_ERS, 38), SND(S_VOLUME, 127), PAR(0, P_TRANS, -12), PAR(0, P_TONE, TN_TALK), PAR(0, P_TONE_A, 15),
                  PAR(0, P_TONE_B, 50), PAR(0, P_TONE_C, 65), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 24)}},
    {"SWEEP SAX", {ENG(0, ENG_ID_ERS, 14), SND(S_VOLUME, 105), PAR(0, P_TRANS, -12), PAR(0, P_TONE, TN_SWEEP), PAR(0, P_TONE_A, 45),
                   PAR(0, P_TONE_B, 95), PAR(0, P_TONE_C, 60), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 24), SND(S_DLY_MIX, 14)}},
    /* a clarinet through a resonant band-pass that breath moves: a wah */
    {"WAH CLARINET", {ENG(0, ENG_ID_ERS, 1), SND(S_VOLUME, 107), PAR(0, P_TONE, TN_FILTER), PAR(0, P_TONE_A, 45), PAR(0, P_TONE_B, 90),
                      PAR(0, P_TONE_C, 1), PAR(0, P_BR_FLT, 60), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 24)}},
    {"TALK FM", {ENG(0, ENG_ID_ERS, 27), SND(S_VOLUME, 56), PAR(0, P_TONE, TN_TALK), PAR(0, P_TONE_A, 5), PAR(0, P_TONE_B, 63),
                 PAR(0, P_TONE_C, 80), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 26)}},
    {"SWEEP BRASS", {ENG(0, ENG_ID_ANALOG, 9), SND(S_VOLUME, 18), PAR(0, P_TONE, TN_SWEEP), PAR(0, P_TONE_A, 40), PAR(0, P_TONE_B, 90),
                     PAR(0, P_TONE_C, 40), ENG(1, ENG_ID_ANALOG, 9), PAR(1, P_FINE, 7), PAR(1, P_LEVEL, 70),
                     PAR(1, P_TONE, TN_SWEEP), PAR(1, P_TONE_A, 40), PAR(1, P_TONE_B, 90), PAR(1, P_TONE_C, 40),
                     SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30)}},
    {"SUB FLUTE", {ENG(0, ENG_ID_ERS, 19), SND(S_VOLUME, 75), PAR(0, P_TONE, TN_SUB), PAR(0, P_TONE_A, 55), PAR(0, P_TONE_B, 30),
                   PAR(0, P_TONE_C, 0), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 34)}},
    {"TALK FOLD", {ENG(0, ENG_ID_FOLD, 1), SND(S_VOLUME, 127), PAR(0, P_TONE, TN_TALK), PAR(0, P_TONE_A, 30), PAR(0, P_TONE_B, -50),
                   PAR(0, P_TONE_C, 85), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 26), SND(S_DLY_MIX, 16)}},
    /* the oboe with a singer on it */
    {"VOX OBOE", {ENG(0, ENG_ID_ERS, 16), SND(S_VOLUME, 32), ENG(1, ENG_ID_VOWEL, 0), PAR(1, P_LEVEL, 45), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 40),
                  SND(S_REV_PRE, 35)}},
    /* a talking lead in two voices: the note and a sixth under, each with its own mouth */
    {"TALK DUO", {ENG(0, ENG_ID_ANALOG, 0), SND(S_VOLUME, 29), PAR(0, P_TONE, TN_TALK), PAR(0, P_TONE_A, 10), PAR(0, P_TONE_B, 55),
                  PAR(0, P_TONE_C, 75), PAR(0, P_PAN, -20), ENG(1, ENG_ID_ANALOG, 0), PAR(1, P_TRANS, -9),
                  PAR(1, P_LEVEL, 60), PAR(1, P_TONE, TN_TALK), PAR(1, P_TONE_A, 60), PAR(1, P_TONE_B, -40),
                  PAR(1, P_TONE_C, 75), PAR(1, P_PAN, 20), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 26)}},
    /* 65 .. (2026-10-21): the EVA engine (breath shaped per destination: level, cutoff, vibrato) */
    {"EVA BRASS", {ENG(0, ENG_ID_EVA, 0), SND(S_VOLUME, 100), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 28)}},
    {"EVA DBL", {ENG(0, ENG_ID_EVA, 1), SND(S_VOLUME, 127), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30),
                 SND(S_DLY_MIX, 12)}},
    {"EVA LEAD", {ENG(0, ENG_ID_EVA, 2), SND(S_VOLUME, 115), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 22), SND(S_DLY_MIX, 18)}},
    {"EVA FLUTE", {ENG(0, ENG_ID_EVA, 3), SND(S_VOLUME, 108), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 35)}},
    {"EVA MULTI", {ENG(0, ENG_ID_EVA, 4), SND(S_VOLUME, 82), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30)}},
    /* 70 .. (2026-10-21): EVA's EDIT 3 / 4 (filter types, formant, tremolo, chiff, response) */
    {"EVA VOX", {ENG(0, ENG_ID_EVA, 5), SND(S_VOLUME, 127), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 32)}},
    {"EVA HORN", {ENG(0, ENG_ID_EVA, 6), SND(S_VOLUME, 85), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 34)}},
    {"EVA REED", {ENG(0, ENG_ID_EVA, 7), SND(S_VOLUME, 127), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 24)}},
    {"EVA TREMOLO", {ENG(0, ENG_ID_EVA, 8), SND(S_VOLUME, 82), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30), SND(S_DLY_MIX, 15)}},
    {"EVA BASS", {ENG(0, ENG_ID_EVA, 9), SND(S_VOLUME, 127), PAR(0, P_TRANS, -24), SND(S_REV_MIX, 10)}},
    {"EVA PIPE", {ENG(0, ENG_ID_EVA, 10), SND(S_VOLUME, 127), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 40)}},
    {"EVA SECTION", {ENG(0, ENG_ID_EVA, 0), PAR(0, P_TRANS, -12), ENG(1, ENG_ID_EVA, 6), PAR(1, P_LEVEL, 70),
                     PAR(1, P_TRANS, -12), PAR(1, P_FINE, 8), PAR(1, P_PAN, -24), PAR(0, P_PAN, 24), SND(S_VOLUME, 58),
                     SND(S_REV_TYPE, 1), SND(S_REV_MIX, 34)}},
    {"EVA CHOIR", {ENG(0, ENG_ID_EVA, 5), PAR(0, P_TRANS, -12), ENG(1, ENG_ID_EVA, 11), PAR(1, P_LEVEL, 55),
                   PAR(1, P_E9, 60), PAR(1, P_E10, 80), PAR(1, P_FINE, -7), SND(S_VOLUME, 90), SND(S_REV_TYPE, 1),
                   SND(S_REV_MIX, 45)}},
    /* 78 .. 80 (2026-10-21): ERS's EDIT 3 / 4 (VIB RATE TONGUE LAG, AIR TILT PR>PITCH DRIFT) */
    {"SAX EXPRESS", {ENG(0, ENG_ID_ERS, 38), PAR(0, P_E8, 55), PAR(0, P_E9, 58), PAR(0, P_E10, 45), PAR(0, P_E12, 28),
                     PAR(0, P_E13, 58), PAR(0, P_E14, 72), PAR(0, P_E15, 35), SND(S_VOLUME, 119), PAR(0, P_TRANS, -12),
                     SND(S_REV_TYPE, 0), SND(S_REV_MIX, 26), SND(S_DLY_MIX, 10)}},
    {"BANSURI VIB", {ENG(0, ENG_ID_ERS, 32), PAR(0, P_E8, 70), PAR(0, P_E9, 50), PAR(0, P_E12, 80),
                     PAR(0, P_E15, 50), SND(S_VOLUME, 46), PAR(0, P_TRANS, -12), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 45),
                     SND(S_REV_DECAY, 100), SND(S_REV_PRE, 40)}},
    {"BARK TRUMPET", {ENG(0, ENG_ID_ERS, 9), PAR(0, P_E8, 40), PAR(0, P_E9, 70), PAR(0, P_E10, 110), PAR(0, P_E13, 76),
                      PAR(0, P_E14, 72), SND(S_VOLUME, 51), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 24)}},
    /* 81 .. (2026-10-21): EVA's waves that breath plays */
    {"EVA SYNC LEAD", {ENG(0, ENG_ID_EVA, 12), SND(S_VOLUME, 127), SND(S_REV_TYPE, 0), SND(S_REV_MIX, 22), SND(S_DLY_MIX, 18)}},
    {"EVA REEDY", {ENG(0, ENG_ID_EVA, 13), SND(S_VOLUME, 62), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 28)}},
    {"EVA HARMONIC", {ENG(0, ENG_ID_EVA, 14), SND(S_VOLUME, 100), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 38)}},
    {"EVA SUB BASS", {ENG(0, ENG_ID_EVA, 15), SND(S_VOLUME, 127), PAR(0, P_TRANS, -12), SND(S_REV_MIX, 8)}},
    {"EVA SUPERSAW", {ENG(0, ENG_ID_EVA, 16), SND(S_VOLUME, 100), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 32), SND(S_DLY_MIX, 15)}},
    {"EVA TRANCE", {ENG(0, ENG_ID_EVA, 17), SND(S_VOLUME, 88), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30), SND(S_DLY_MIX, 22)}},
    {"EVA SUPER STACK", {ENG(0, ENG_ID_EVA, 16), ENG(1, ENG_ID_EVA, 15), PAR(1, P_LEVEL, 60), PAR(1, P_TRANS, -12),
                         SND(S_VOLUME, 74), SND(S_REV_TYPE, 1), SND(S_REV_MIX, 30), SND(S_DLY_MIX, 15)}},
};
#undef ENG
#undef PAR
#undef SND
#define NFACTORY ((uint32_t)NELEM(FACTORY))

static struct { char name[PS_NAME + 1]; uint8_t user; } ps_ix[NPRESET];   /* every slot's name (the browser) */
static struct {
    uint8_t cur;                                   /* the slot loaded / saved last */
    char name[PS_NAME + 1];                        /* the name of what plays */
} ps;
static ps_rec_t ps_tmp, ps_new;
static uint32_t ps_sbuf[0x1000u / 4u] __attribute__((section(".pool")));   /* a sector, while saving */

/* CRC-32 as zlib's (IEEE 802.3, reflected, polynomial 0xEDB88320, ~0 in and out), a bit at a time: the low bit
 * decides, as a mask, whether the polynomial goes in */
static uint32_t ps_crc(const uint8_t *p, uint32_t n)
{
    uint32_t crc = ~0u, bit;
    for (; n; n--, p++) {
        crc ^= *p;
        for (bit = 8u; bit; bit--)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
static int ps_ok(const ps_rec_t *r)
{
    return r->magic == PS_MAGIC && r->ver == 1u && r->crc == ps_crc((const uint8_t *)r + 8, PS_SLOT - 8u);
}
/* slot k's sector in the store (copy 0: the main one, 1: its copy) */
static uint32_t ps_sec(uint32_t k, uint32_t copy)
{
    if (k < PS_LO)
        return k / PS_PER_SEC * 0x1000u + (copy ? PS_COPY : 0u);
    return PS_HI_AT + (k - PS_LO) / PS_PER_SEC * 0x1000u + (copy ? PS_HI_COPY : 0u);
}
static int ps_read(uint32_t k, ps_rec_t *r)        /* 0: a good record (the main one, else the copy) */
{
    uint32_t at = k % PS_PER_SEC * PS_SLOT;
    if (!store_read(ps_sec(k, 0) + at, r, PS_SLOT) && ps_ok(r))
        return 0;
    if (!store_read(ps_sec(k, 1) + at, r, PS_SLOT) && ps_ok(r))
        return 0;
    return -1;
}

static void name_set(char *dst, const char *src, uint32_t n)   /* up to n chars, 0-terminated */
{
    uint32_t i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
}
static void slot_default_name(uint32_t k, char *dst)
{
    if (k < NFACTORY)
        name_set(dst, FACTORY[k].name, PS_NAME);
    else
        name_set(dst, "INIT", PS_NAME);
}
static void ps_index(uint32_t k)
{
    ps_ix[k].user = !ps_read(k, &ps_tmp);
    if (ps_ix[k].user)
        name_set(ps_ix[k].name, ps_tmp.name, PS_NAME);
    else
        slot_default_name(k, ps_ix[k].name);
}

static void factory_apply(uint32_t k)
{
    const fp_t *f = FACTORY[k].f;
    uint32_t j;
    layers_defaults();
    for (j = 0; j < NELEM(FACTORY[k].f) && f[j].op != FP_END; j++) {
        uint32_t l = f[j].l % NLAYER;
        switch (f[j].op) {
        case FP_ENG: {
            uint8_t miss;
            uint32_t idx = eng_index(f[j].i, &miss), e;
            const engine_t *en = ENGINES[idx].e;
            if (f[j].i == ENG_ID_OFF) {            /* a layer kept, no engine (its level, envelope ... ready) */
                layer_set_engine(l, NENGINES);
                break;
            }
            layer_set_engine(l, idx);
            if (miss) {
                trk[l].p[P_ENGINE] = f[j].i;      /* (shows "?": this build lacks it) */
                break;
            }
            if ((uint32_t)f[j].v < en->npresets)
                for (e = 0; e < NEPAR; e++)
                    trk[l].p[P_E0 + e] = en->presets[f[j].v].e[e];
            break;
        }
        case FP_PAR:
            trk[l].p[f[j].i] = f[j].v;
            break;
        case FP_SND:
            group_own(S_GROUP[f[j].i]);
            sown[f[j].i] = f[j].v;
            break;
        }
    }
}

static void ps_apply(const ps_rec_t *r)
{
    uint32_t l, i, nc = r->np > NEPAR ? r->np - NEPAR : 0;
    layers_defaults();
    for (l = 0; l < NLAYER && l < r->nl && l < PS_NL; l++) {
        for (i = 0; i < NCOMMON && i < nc; i++)
            trk[l].p[i] = r->p[l][i];
        for (i = 0; i < NEPAR && nc + i < r->np && nc + i < PS_NP; i++)
            trk[l].p[P_E0 + i] = r->p[l][nc + i];
    }
    for (i = 0; i < S_COUNT; i++)
        sown[i] = i < r->ns && i < PS_NS ? r->s[i] : sglob[i];
    s_own = (uint8_t)(r->own & ((1u << NGROUP) - 1u));
}

/* slot k plays: what is saved there, else its factory preset, else INIT */
static void ps_load(uint32_t k)
{
    k %= NPRESET;
    if (!ps_read(k, &ps_tmp)) {
        ps_apply(&ps_tmp);
        name_set(ps.name, ps_tmp.name, PS_NAME);
    } else if (k < NFACTORY) {
        factory_apply(k);
        slot_default_name(k, ps.name);
    } else {
        layers_defaults();
        slot_default_name(k, ps.name);
    }
    ps.cur = (uint8_t)k;
    ps_edited = 0;
    par_dirty++;
}

#if defined(TYPHOON_SIM)
/* the INIT sound (one sine) in the last slot, whatever is there: the tests' and renders' clean start, now that the
 * factory presets fill every slot (the sim's sim_preset(255), a script's "preset 255") */
static void ps_load_init(void)
{
    layers_defaults();
    name_set(ps.name, "INIT", PS_NAME);
    ps.cur = (uint8_t)(NPRESET - 1u);
    ps_edited = 0;
    par_dirty++;
}
#endif

/* record r into slot k (r 0: none, the slot plays its factory preset again), the copy first, each other slot's good
 * record kept; then the browser's index. 0: done. Two sector erases: ~100 ms without sound. (r is not ps_tmp) */
static int ps_put(uint32_t k, const ps_rec_t *r)
{
    uint32_t sec, cpy, j;
    uint8_t *b = (uint8_t *)ps_sbuf;
    int rc;
    k %= NPRESET;
    sec = ps_sec(k, 0);                            /* the sector as it is, each slot's good record kept */
    cpy = ps_sec(k, 1);
    if ((rc = store_read(sec, b, 0x1000u)) != 0)
        return rc;
    for (j = 0; j < PS_PER_SEC; j++) {
        ps_rec_t *s = (ps_rec_t *)(b + j * PS_SLOT);
        if (!ps_ok(s) && !store_read(cpy + j * PS_SLOT, &ps_tmp, PS_SLOT) && ps_ok(&ps_tmp))
            memcpy(s, &ps_tmp, PS_SLOT);
    }
    if (r)
        memcpy(b + k % PS_PER_SEC * PS_SLOT, r, PS_SLOT);
    else
        memset(b + k % PS_PER_SEC * PS_SLOT, 0xFF, PS_SLOT);
    if ((rc = store_erase(cpy)) != 0 || (rc = store_write(cpy, b, 0x1000u)) != 0)
        return rc;
    if ((rc = store_erase(sec)) != 0 || (rc = store_write(sec, b, 0x1000u)) != 0)
        return rc;
    ps_index(k);
    return ps_ix[k].user == (r != 0) ? 0 : -1;    /* (read back) */
}

/* what plays now into slot k (the name: what plays, "PRESET nn" for INIT). 0: saved */
static int ps_save(uint32_t k)
{
    uint32_t l, j;
    int rc;
    k %= NPRESET;
    memset(&ps_new, 0, sizeof ps_new);
    ps_new.magic = PS_MAGIC;
    ps_new.ver = 1;
    ps_new.nl = NLAYER;
    ps_new.np = P_COUNT;
    ps_new.ns = S_COUNT;
    ps_new.own = s_own;
    if (ps.name[0] == 'I' && ps.name[1] == 'N' && ps.name[2] == 'I' && ps.name[3] == 'T' && !ps.name[4]) {
        name_set(ps.name, "PRESET ", PS_NAME);
        put_int(ps.name + 7, (int32_t)k + 1, 0);
    }
    for (j = 0; j < PS_NAME && ps.name[j]; j++)
        ps_new.name[j] = ps.name[j];
    for (l = 0; l < NLAYER; l++)
        memcpy(ps_new.p[l], trk[l].p, sizeof trk[l].p);
    memcpy(ps_new.s, sown, sizeof sown);
    ps_new.crc = ps_crc((const uint8_t *)&ps_new + 8, PS_SLOT - 8u);
    if ((rc = ps_put(k, &ps_new)) != 0)
        return rc;
    ps.cur = (uint8_t)k;
    ps_edited = 0;
    par_dirty++;
    return 0;
}

/* ---- the state in the settings sector (<= 512 bytes). With room to grow: the counts it was saved with say what is
 * there, and are read by them as the presets are (layers: the commons, then the NEPAR engine values; S_* and G_* by
 * index): a new parameter (just before P_E0, or at the end of S_* / G_*) takes its default, nothing is lost */
#define STATE_MAGIC 0x7A08u                        /* change it only when the layout below changes */
#define P_SAVED 40u
#define S_SAVED 32u
typedef struct {
    uint16_t magic;
    uint8_t ng, ns, np, nl;                        /* G_COUNT, S_COUNT, P_COUNT, NLAYER when saved */
    uint8_t cur, edited, own, spare;
    int16_t g[G_SAVED];
    int16_t sg[S_SAVED];                           /* the global sound */
    int16_t so[S_SAVED];                           /* the edit buffer: its own sound, its layers, its name */
    int16_t p[NLAYER][P_SAVED];
    char name[PS_NAME];
} state_t;
_Static_assert(sizeof(state_t) <= 512u, "the settings sector holds 512 bytes");
_Static_assert(G_COUNT <= G_SAVED && S_COUNT <= S_SAVED && P_COUNT <= P_SAVED, "the saved state: room");
static void state_load(void)
{
    static state_t s;
    uint32_t i, n, nc;
    params_defaults();
    for (i = 0; i < NPRESET; i++)
        ps_index(i);
    if (settings_load(&s, sizeof s) || s.magic != STATE_MAGIC || s.ng > G_SAVED || s.ns > S_SAVED || s.np < NEPAR ||
        s.np > P_SAVED || s.nl != NLAYER) {
        ps_load(0);                                /* nothing saved, or a layout we do not know: the first preset */
        return;
    }
    for (i = 0; i < G_COUNT && i < s.ng; i++)
        gpar[i] = (int16_t)clamp(s.g[i], GLOBAL[i].min, GLOBAL[i].max);
    for (i = 0; i < S_COUNT && i < s.ns; i++) {
        sglob[i] = (int16_t)clamp(s.sg[i], SOUND[i].min, SOUND[i].max);
        sown[i] = (int16_t)clamp(s.so[i], SOUND[i].min, SOUND[i].max);
    }
    nc = s.np - NEPAR;                             /* the commons saved; then the engine's NEPAR */
    for (n = 0; n < NLAYER; n++) {
        for (i = 0; i < NCOMMON && i < nc; i++)
            trk[n].p[i] = s.p[n][i];               /* (an engine this build lacks: layers.c plays ENGINES[0]) */
        for (i = 0; i < NEPAR; i++)
            trk[n].p[P_E0 + i] = s.p[n][nc + i];
    }
    s_own = (uint8_t)(s.own & ((1u << NGROUP) - 1u));
    ps.cur = (uint8_t)(s.cur % NPRESET);
    ps_edited = s.edited ? 1u : 0u;
    name_set(ps.name, s.name, PS_NAME);
    par_dirty++;
}

static void state_save(void)
{
    static state_t s;
    uint32_t n, j;
    memset(&s, 0, sizeof s);
    s.magic = STATE_MAGIC;
    s.ng = G_COUNT;
    s.ns = S_COUNT;
    s.np = P_COUNT;
    s.nl = NLAYER;
    s.cur = ps.cur;
    s.edited = ps_edited;
    s.own = s_own;
    memcpy(s.g, gpar, sizeof gpar);
    memcpy(s.sg, sglob, sizeof sglob);
    memcpy(s.so, sown, sizeof sown);
    for (n = 0; n < NLAYER; n++)
        memcpy(s.p[n], trk[n].p, sizeof trk[n].p);
    for (j = 0; j < PS_NAME && ps.name[j]; j++)
        s.name[j] = ps.name[j];
    settings_save(&s, sizeof s);
}

#if defined(TYPHOON_SIM)
static const char *host_factory_name(uint32_t k) { return k < NFACTORY ? FACTORY[k].name : 0; }   /* the simulator (sim_factory_name) */
#endif
