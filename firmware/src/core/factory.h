/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The factory presets (core/presets.c FACTORY[]): written by tools/factory_from_backup.mjs from the user's own,
 * picked and sorted on the FM-1 (2026-10-29 00:04:56, slots 1-26,f3); a slot the backup did not have kept the factory
 * preset it had. Each: INIT, then per layer its engine and that engine's nearest preset (ENG), the values that
 * differ (PAR), the sound groups it owns (SND). Edit here by hand, or make it again from a new backup. */
/* (the most ops a preset has here: 17; room for FACTORY_OPS - 1) */
static const factory_t FACTORY[] = {
    {"SWEEP LEAD", {ENG(0, ENG_ID_ANALOG, 0), PAR(0, P_TONE, TN_SWEEP), PAR(0, P_TONE_A, 40), PAR(0, P_TONE_B, 100),
                PAR(0, P_TONE_C, 55), SND(S_DLY_MIX, 22), SND(S_REV_MIX, 22), SND(S_VOLUME, 31)}},                  /* 1 */
    {"EVA REEDY", {ENG(0, ENG_ID_EVA, 13), SND(S_REV_MIX, 28), SND(S_REV_TYPE, 1), SND(S_VOLUME, 62)}},             /* 2 */
    {"DOUBLE REEDS", {ENG(0, ENG_ID_ERS, 16), PAR(0, P_PAN, -18), ENG(1, ENG_ID_ERS, 17), PAR(1, P_LEVEL, 55),
                  PAR(1, P_TRANS, -12), PAR(1, P_PAN, 18), SND(S_REV_MIX, 36), SND(S_REV_TYPE, 1),
                  SND(S_REV_PRE, 35), SND(S_VOLUME, 26)}},                                                          /* 3 */
    {"TALK LEAD", {ENG(0, ENG_ID_ANALOG, 0), PAR(0, P_TONE, TN_TALK), PAR(0, P_TONE_A, 10), PAR(0, P_TONE_B, 55),
               PAR(0, P_TONE_C, 75), SND(S_REV_SIZE, 90), SND(S_VOLUME, 65)}},                                      /* 4 */
    {"ACID BREATH", {ENG(0, ENG_ID_ANALOG, 4), PAR(0, P_TRANS, -12), PAR(0, P_TONE, TN_SWEEP), PAR(0, P_TONE_A, 30),
                 PAR(0, P_TONE_B, 120), PAR(0, P_TONE_C, 80), SND(S_DLY_MIX, 25), SND(S_REV_MIX, 14),
                 SND(S_VOLUME, 24)}},                                                                               /* 5 */
    {"EVA SUPERSAW", {ENG(0, ENG_ID_EVA, 16), SND(S_DLY_MIX, 15), SND(S_REV_MIX, 32), SND(S_REV_TYPE, 1),
                  SND(S_VOLUME, 100)}},                                                                             /* 6 */
    {"FM WOOD", {ENG(0, ENG_ID_ERS, 28), PAR(0, P_TRANS, -12), SND(S_REV_MIX, 30), SND(S_REV_TYPE, 1),
             SND(S_VOLUME, 30)}},                                                                                   /* 7 */
    {"FM TUBE", {ENG(0, ENG_ID_ERS, 27), SND(S_REV_MIX, 26), SND(S_VOLUME, 46)}},                                   /* 8 */
    {"EVA BRASS", {ENG(0, ENG_ID_EVA, 0), PAR(0, P_TRANS, -12), PAR(0, P_E6, 26), PAR(0, P_E16, 127),
               SND(S_REV_MIX, 28), SND(S_REV_TYPE, 1), SND(S_VOLUME, 100)}},                                        /* 9 */
    {"FM BRASS", {ENG(0, ENG_ID_ERS, 52), PAR(0, P_TRANS, -12), SND(S_REV_MIX, 30), SND(S_REV_TYPE, 1),
              SND(S_VOLUME, 43)}},                                                                                  /* 10 */
    {"BOWED TUBE", {ENG(0, ENG_ID_ERS, 8), PAR(0, P_TRANS, -24), PAR(0, P_E6, 7), PAR(0, P_E7, 50),
                SND(S_REV_MIX, 36), SND(S_REV_TYPE, 1), SND(S_REV_PRE, 30), SND(S_VOLUME, 40)}},                    /* 11 */
    {"FM REED", {ENG(0, ENG_ID_ERS, 51), PAR(0, P_TRANS, -12), SND(S_REV_MIX, 26), SND(S_VOLUME, 43)}},             /* 12 */
    {"PICCOLO", {ENG(0, ENG_ID_ERS, 30), PAR(0, P_TRANS, 12), SND(S_REV_MIX, 32), SND(S_REV_TYPE, 1),
             SND(S_REV_PRE, 25), SND(S_VOLUME, 86)}},                                                               /* 13 */
    {"BANSURI VIB", {ENG(0, ENG_ID_ERS, 32), PAR(0, P_TRANS, -12), PAR(0, P_E8, 70), PAR(0, P_E9, 50),
                 PAR(0, P_E12, 80), PAR(0, P_E15, 50), SND(S_REV_DECAY, 100), SND(S_REV_MIX, 45), SND(S_REV_TYPE, 1),
                 SND(S_REV_PRE, 40), SND(S_VOLUME, 46)}},                                                           /* 14 */
    {"OBOE", {ENG(0, ENG_ID_ERS, 16), SND(S_REV_MIX, 34), SND(S_REV_TYPE, 1), SND(S_REV_PRE, 30), SND(S_VOLUME, 45)}}, /* 15 */
    {"ALTO FLUTE", {ENG(0, ENG_ID_ERS, 31), PAR(0, P_TRANS, -12), SND(S_REV_MIX, 36), SND(S_REV_TYPE, 1),
                SND(S_REV_PRE, 30), SND(S_VOLUME, 69)}},                                                            /* 16 */
    {"PAN FLUTE", {ENG(0, ENG_ID_ERS, 33), SND(S_DLY_MIX, 12), SND(S_REV_MIX, 42), SND(S_REV_TYPE, 1),
               SND(S_REV_PRE, 35), SND(S_VOLUME, 86)}},                                                             /* 17 */
    {"RECORDER", {ENG(0, ENG_ID_ERS, 19), SND(S_REV_MIX, 30), SND(S_REV_TYPE, 1)}},                                 /* 18 */
    {"VOX FLUTE", {ENG(0, ENG_ID_EVA, 5), PAR(0, P_E9, 13), ENG(1, ENG_ID_FM, 0), PAR(1, P_LEVEL, 43),
               PAR(1, P_BR_SHP, 10), PAR(1, P_E0, 12), PAR(1, P_E2, 0), PAR(1, P_E4, 30), PAR(1, P_E5, 0),
               SND(S_REV_MIX, 40), SND(S_VOLUME, 127)}},                                                            /* 19 */
    {"TIN WHISTLE", {ENG(0, ENG_ID_ERS, 35), SND(S_DLY_MIX, 15), SND(S_REV_MIX, 28), SND(S_VOLUME, 127)}},          /* 20 */
    {"OCARINA", {ENG(0, ENG_ID_ERS, 34), SND(S_REV_MIX, 30), SND(S_VOLUME, 127)}},                                  /* 21 */
    {"FLUE ORGAN", {ENG(0, ENG_ID_ERS, 20), PAR(0, P_TRANS, -12), ENG(1, ENG_ID_ERS, 20), PAR(1, P_LEVEL, 70),
                ENG(2, ENG_ID_ERS, 20), PAR(2, P_LEVEL, 40), PAR(2, P_TRANS, 12), SND(S_REV_SIZE, 127),
                SND(S_REV_DECAY, 115), SND(S_REV_MIX, 50), SND(S_REV_TYPE, 1), SND(S_REV_PRE, 50),
                SND(S_VOLUME, 48)}},                                                                                /* 22 */
    {"BASS CLARINE", {ENG(0, ENG_ID_ERS, 36), PAR(0, P_TRANS, -24), SND(S_REV_MIX, 30), SND(S_REV_TYPE, 1),
                  SND(S_VOLUME, 47)}},                                                                              /* 23 */
    {"EVA DBL", {ENG(0, ENG_ID_EVA, 1), PAR(0, P_TRANS, -12), SND(S_DLY_MIX, 12), SND(S_REV_MIX, 30),
             SND(S_REV_TYPE, 1), SND(S_VOLUME, 127)}},                                                              /* 24 */
    {"EVA HORN", {ENG(0, ENG_ID_EVA, 6), PAR(0, P_TRANS, -12), SND(S_REV_MIX, 34), SND(S_REV_TYPE, 1),
              SND(S_VOLUME, 85)}},                                                                                  /* 25 */
    {"EVA TRANCE", {ENG(0, ENG_ID_EVA, 17), SND(S_DLY_MIX, 22), SND(S_REV_MIX, 30), SND(S_REV_TYPE, 1),
                SND(S_VOLUME, 88)}},                                                                                /* 26 */
    {"CLARINET", {ENG(0, ENG_ID_ERS, 1), PAR(0, P_E2, 90), PAR(0, P_E3, 2), PAR(0, P_E4, 101), PAR(0, P_E5, 65),
              ENG(1, ENG_ID_ERS, 0), PAR(1, P_LEVEL, 46), PAR(1, P_E0, 9), PAR(1, P_E1, 2), PAR(1, P_E4, 38),
              PAR(1, P_E5, 23), PAR(1, P_E6, 0), SND(S_DRIVE, 6), SND(S_DRV_AT, 62), SND(S_REV_MIX, 30),
              SND(S_DT_MIX, 12), SND(S_VOLUME, 90)}},                                                               /* 27 */
    /* 28, 29 by hand (2026-10-29): Denis's pan flute (PANFLUTE). _TEC: as he plays it on a TEControl, his breath
     * markers (PP 4, MF 12, FF 33), his vibrato, the head's wobble (VIB MODE TILT; bend CC 1 its bite, vibrato CC 13 its
     * tilt, his defaults); _ART: the re.corder (Artinoise), the user's breath (PP 34, MF 74, FF 107), its rotation
     * setting an LFO vibrato's depth (VIB MODE LFO = 1, VIB CC 52, CENTRE 64) */
    {"PANFLUTE_TEC", {ENG(0, ENG_ID_PAN, 0), PAR(0, P_E0, 4), PAR(0, P_E1, 12), PAR(0, P_E2, 33),
                  PAR(0, P_E8, 1), PAR(0, P_E10, 13), SND(S_REV_MIX, 42), SND(S_REV_TYPE, 1), SND(S_REV_PRE, 35)}},  /* 28 */
    {"PANFLUTE_ART", {ENG(0, ENG_ID_PAN, 0), PAR(0, P_E0, 34), PAR(0, P_E1, 74), PAR(0, P_E2, 107),
                  PAR(0, P_E9, 1), PAR(0, P_E10, 52), SND(S_REV_MIX, 42), SND(S_REV_TYPE, 1), SND(S_REV_PRE, 35)}},  /* 29 */
};
