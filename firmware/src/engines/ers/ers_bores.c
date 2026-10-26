/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* ERS: the coupled models (ers.h). The exciter is part of the loop: what comes back down the bore moves the jet, the
 * reed or the bow, which is what makes them play at the bore's pitch and sustain. Each takes a block's context and
 * writes its sound into b->x; the lines and their write index are the layer's (ers_t). */

/* the flute (STK Flute): a jet into an open tube; PIPE (M_FLUE): a flue pipe, the jet through a windway */
static void ers_bore_flute(ers_blk_t *b)
{
    ers_t *st = b->st;
    int32_t flue = b->model == M_FLUE, bri = b->bri, dec = b->dec;
    int32_t a = flue ? 11000 - bri * 40 : 14000 - bri * 48;   /* the loop's one-pole */
    uint32_t comp = (uint32_t)((a << 8) / (ERS_ONE - a));     /* its delay, Q8 */
    uint32_t db = ers_len((int32_t)b->per8 - (int32_t)comp + 256, ERS_BORE);   /* (+1: measured) */
    int32_t ratio = flue ? 6881 + dec * 24                    /* VOICING: 0.42 .. 0.6 of a period */
                         : 8192 + (dec - 50) * 40;            /* EMBOUCH: 0.38 .. 0.5 (50) .. 0.69 */
    uint32_t dj = ers_len((int32_t)(((db >> 4) * (uint32_t)ratio) >> 10), ERS_JET), i, w = st->w;
    int32_t air = b->col * (flue ? 32 : 64);       /* the jet's breath noise (half, 2026-10-18: too loud) */
    for (i = 0; i < b->n; i++) {
        int32_t pi = ers_p(b, i), bo, tmp, y, pd, jo, jt, pj, nz;
        nz = ers_air(st, 18);                      /* +-0.5 */
        bo = ers_tap(st->bore, ERS_BORE - 1u, w, db);
        st->lp += ((bo - st->lp) * (ERS_ONE - a)) >> 14;
        y = st->lp - st->dcx + ((st->dcy * 16302) >> 14);   /* DC blocker */
        st->dcx = st->lp;
        st->dcy = y;
        tmp = y;
        pj = pi ? 13435 + (pi >> 1) : 0;           /* blown from just over the threshold (~0.8: STK 1.1 .. 1.3) */
        if (flue) {                                /* the windway: a steadier jet */
            st->ww += ((pj - st->ww) * 1200) >> 14;
            pj = st->ww;
        }
        if (b->exc == EX_JET) {                    /* turbulence follows the flow; the chiff */
            int32_t fl = st->jet[(w - 1u) & (ERS_JET - 1u)] << 1;
            nz = (nz * ((fl < 0 ? -fl : fl) + 4096)) >> 14;
            if (st->ch > 0) {
                int32_t cv = st->ch * 9;           /* 0 .. ~1.0 */
                pj += (pj * cv) >> 16;
                nz += (((int32_t)noise32(&st->seed) >> 18) * cv) >> 14;
                st->ch--;
            }
        }
        pd = pj + ((pj * ((nz * air) >> 14)) >> 14) - (tmp >> 1);   /* breath (+ noise) - jet reflection */
        st->jet[w & (ERS_JET - 1u)] = ers_s16(pd);
        jo = clamp(ers_tap(st->jet, ERS_JET - 1u, w, dj), -22937, 22937);
        jt = clamp(((((jo * jo) >> 14) * jo) >> 14) - jo, -ERS_ONE, ERS_ONE);   /* the jet: x^3 - x */
        if (flue)
            jt = (jt * 13107) >> 14;               /* (a gentler jet: purer) */
        st->bore[w & (ERS_BORE - 1u)] = ers_s16(jt + (tmp >> 1));   /* + end reflection */
        b->x[i] = (((bo * 3) >> 3) * (pi < ERS_ONE ? pi : ERS_ONE)) >> 14;   /* (as loud as blown) */
        w++;
    }
    st->w = w;
}

/* the mouth's pressure for a valve: none without breath, else the window's (ers_valve_set), a little noise in it */
static inline int32_t ers_mouth(ers_t *st, int32_t pi, int32_t pm0, int32_t pmk)
{
    int32_t nz = ers_air(st, 20), pm;              /* +-0.125 */
    pm = pi ? pm0 + ((pi * pmk) >> 14) : 0;
    return pm + ((pm * nz) >> 14);
}

/* a reed on a closed cylinder (STK Clarinet) */
static void ers_bore_cyl(ers_blk_t *b, const ers_valve_t *k, int32_t lpb, int32_t pm0, int32_t pmk)
{
    ers_t *st = b->st;
    uint32_t db = ers_len((int32_t)(b->per8 >> 1) - (lpb >> 6), ERS_BORE), i, w = st->w;
    int32_t refl = 13107 + b->dec * 25;
    for (i = 0; i < b->n; i++) {
        int32_t pi = ers_p(b, i), bo, f, pm = ers_mouth(st, pi, pm0, pmk);
        bo = ers_tap(st->bore, ERS_BORE - 1u, w, db);
        f = bo + (((st->lp - bo) * lpb) >> 14);
        st->lp = bo;
        st->bore[w & (ERS_BORE - 1u)] = ers_s16(ers_valve(k, pm, -((f * refl) >> 14)));
        b->x[i] = (((bo * 5) >> 3) * (pi < ERS_ONE ? pi : ERS_ONE)) >> 14;   /* (as loud as blown) */
        w++;
    }
    st->w = w;
}

/* the cylinder with a register vent a third along (HOLE) */
static void ers_bore_vent(ers_blk_t *b, const ers_valve_t *k, int32_t lpb, int32_t pm0, int32_t pmk)
{
    ers_t *st = b->st;
    int32_t r = (int32_t)(b->per8 >> 1) - (lpb >> 6), refl = 15565;
    uint32_t d1 = ers_len(r / 6, ERS_HALF), drt = ers_len(r - 2 * (r / 6), ERS_JET), i, w = st->w;
    int32_t v14 = b->dec * 129, kv = clamp((int32_t)(b->m->inc >> 16) * 3, 64, ERS_ONE);   /* a leak below ~2 f */
    int16_t *fwd = st->bore, *back = st->bore + ERS_HALF;
    for (i = 0; i < b->n; i++) {
        int32_t pi = ers_p(b, i), a, e, f, bb, s, pj, pb, pm = ers_mouth(st, pi, pm0, pmk);
        a = ers_tap(fwd, ERS_HALF - 1u, w, d1);    /* at the vent, from the mouth */
        e = ers_tap(st->jet, ERS_JET - 1u, w, drt);   /* back from the open end */
        pb = ers_tap(back, ERS_HALF - 1u, w, d1);  /* at the mouth */
        f = e + (((st->lp - e) * lpb) >> 14);
        st->lp = e;
        bb = -((f * refl) >> 14);
        s = a + bb;
        st->vl += ((s - st->vl) * kv) >> 14;
        pj = s - ((st->vl * v14) >> 14);           /* the vent lets the low pressure out */
        st->jet[w & (ERS_JET - 1u)] = ers_s16(pj - bb);
        back[w & (ERS_HALF - 1u)] = ers_s16(pj - a);
        fwd[w & (ERS_HALF - 1u)] = ers_s16(ers_valve(k, pm, pb));
        b->x[i] = (((e * 5) >> 3) * (pi < ERS_ONE ? pi : ERS_ONE)) >> 14;
        w++;
    }
    st->w = w;
}

/* a reed on a cone (STK Saxofony): two lines, the reed between; on BRASS (LIP or any reed) a bell on what comes out */
static void ers_bore_cone(ers_blk_t *b, const ers_valve_t *k, int32_t lpb, int32_t pm0, int32_t pmk)
{
    ers_t *st = b->st;
    int32_t d = (int32_t)b->per8 - (lpb >> 6), pos = 819 + b->dec * 52;   /* POSITN 0.05 .. 0.45 */
    uint32_t d1 = ers_len((int32_t)((((uint32_t)d >> 4) * (uint32_t)pos) >> 10), ERS_JET);
    uint32_t d0 = ers_len(d - (int32_t)d1, ERS_BORE), i, w = st->w;
    for (i = 0; i < b->n; i++) {
        int32_t pi = ers_p(b, i), o0, f, tmp, pmb, pm = ers_mouth(st, pi, pm0, pmk);
        o0 = ers_tap(st->bore, ERS_BORE - 1u, w, d0);
        f = o0 + (((st->lp - o0) * lpb) >> 14);
        st->lp = o0;
        tmp = -((f * 15565) >> 14);
        pmb = tmp - ers_tap(st->jet, ERS_JET - 1u, w, d1);
        st->bore[w & (ERS_BORE - 1u)] = ers_s16(ers_valve(k, pm, pmb) - tmp);
        st->jet[w & (ERS_JET - 1u)] = ers_s16(tmp);
        if (b->res == RS_BRASS) {                  /* the bell: less below ~300 Hz */
            st->bl += ((pmb - st->bl) * 700) >> 14;
            pmb -= (st->bl * 3) >> 2;
        }
        b->x[i] = (((pmb * 3) >> 2) * (pi < ERS_ONE ? pi : ERS_ONE)) >> 14;
        w++;
    }
    st->w = w;
}

/* FM in a bore: feedback FM through a tuned delay. The carrier goes into a comb a period long (ringing 0.9, BRIGHT
 * its damping), and what comes back goes into the carrier's phase, DECAY (FEEDBK) of half a cycle at most: low it
 * pulls the carrier towards the tube's harmonics (hollow, woody), high it growls and breaks up. Stays at the note */
static void ers_bore_fm(ers_blk_t *b)
{
    ers_t *st = b->st;
    int32_t lpb = (128 - b->bri) * 64, ring = 14746, gin = ERS_ONE - ((ring * 15) >> 4);
    uint32_t db = ers_len((int32_t)b->per8 - (lpb >> 6), ERS_BORE), pc = b->v->ph[0], pm = b->v->ph[1];
    uint32_t im = ers_fm_inc(b), i, w = st->w;
    for (i = 0; i < b->n; i++) {
        int32_t pi = ers_p(b, i), bo = ers_tap(st->bore, ERS_BORE - 1u, w, db), f, e, y;
        f = bo + (((st->lp - bo) * lpb) >> 14);
        st->lp = bo;
        e = (ers_fm_sample(pc, pm, pi, (uint32_t)(f * b->dec) << 10) * pi) >> 14;
        y = ((e * gin) >> 14) + ((f * ring) >> 14);
        st->bore[w & (ERS_BORE - 1u)] = ers_s16(y);
        b->x[i] = y;
        pc += b->m->inc;
        pm += im;
        w++;
    }
    st->w = w;
    b->v->ph[0] = pc;
    b->v->ph[1] = pm;
}

/* SAX (ers.h): CONE's loop, its losses a one-pole low-pass at K x the note, K with the pressure: SAX_K0 at none, at
 * full SAX_K1LO .. SAX_K1HI (BRIGHT 0 .. 127). Its delay at the note comes off the line and its gain there is made
 * up (G / |H(f0)|), so the pitch and where the note starts stay as it opens. The air: white noise through a wide
 * band-pass (SAX_NZC), as strong as the pressure, into the reed (SAX_NZI) and out (SAX_NZO). A slur (the increment
 * jumps by more than 1/32 in a block): the pressure dips by SAX_DIP, back in ~45 ms. (The constants: an automated
 * search against measured saxophones, 2026-10-20, then the air out cut ~10 dB by ear: at the measured level it was
 * heard as hiss. Behind #ifndef for SIM_CFLAGS tuning runs) */
#ifndef SAX_K0
#define SAX_K0 608                                 /* x the note, Q8: 2.4 */
#endif
#ifndef SAX_K1LO
#define SAX_K1LO 1920                              /* 7.5 */
#endif
#ifndef SAX_K1HI
#define SAX_K1HI 13312                             /* 52 */
#endif
#ifndef SAX_NZI
#define SAX_NZI 3000                               /* Q14 */
#endif
#ifndef SAX_NZO
#define SAX_NZO 1300
#endif
#ifndef SAX_NZC
#define SAX_NZC 20390                              /* ~1.5 kHz (the cutoff scale, ers_cut) */
#endif
#ifndef SAX_NZQ
#define SAX_NZQ 30                                 /* (tsvf reso: wide) */
#endif
#ifndef SAX_DIP
#define SAX_DIP 6000                               /* Q14 */
#endif

static int32_t ers_atan14(int32_t t)               /* atan of t >= 0 (Q14), radians Q14 (to ~0.005) */
{
    int32_t r;
    if (t <= ERS_ONE)
        return (t * (12868 + ((4473 * (ERS_ONE - t)) >> 14))) >> 14;   /* x (pi/4 + .273 (1 - x)) (< 2^29) */
    t = (int32_t)((1u << 28) / (uint32_t)t);
    r = (t * (12868 + ((4473 * (ERS_ONE - t)) >> 14))) >> 14;
    return 25736 - r;                              /* pi/2 - atan(1/t) */
}

static void ers_bore_sax(ers_blk_t *b, const ers_valve_t *k, int32_t pm0, int32_t pmk)
{
    ers_t *st = b->st;
    uint32_t inc = b->m->inc, d1, d0, i, w = st->w;
    int32_t w16 = (int32_t)(umul64(inc, 411775u) >> 32);   /* the note's step, radians Q16 (2 pi x 2^16) */
    int32_t pe = clamp(b->p0 + b->dp, 0, ERS_ONE), k1 = SAX_K1LO + ((SAX_K1HI - SAX_K1LO) * b->bri) / 127;
    int32_t kk = SAX_K0 + (((k1 - SAX_K0) * pe) >> 14);         /* the low-pass, x the note, Q8 */
    int32_t x = clamp((int32_t)(mul64(w16, kk) >> 8), 1, 2 << 16);   /* 2 pi fc / fs, Q16 */
    int32_t x2 = (int32_t)(mul64(x, x) >> 16), x3 = (int32_t)(mul64(x2, x) >> 16);   /* (< 2^18, 2^19) */
    int32_t ex = 65536 + x + (x2 >> 1) + x3 / 6;                       /* e^x, Q16 */
    int32_t a = ERS_ONE - (int32_t)((1u << 30) / (uint32_t)ex);         /* the low-pass: 1 - e^-x, Q14 */
    int32_t pl = ERS_ONE - a, sn = sine_i(inc) >> 1, cs = sine_i(inc + 0x40000000u) >> 1;   /* the pole; sin, cos Q14 */
    int32_t den = ERS_ONE - ((pl * cs) >> 14), tq, tau8, gn, d, pos = 819 + b->dec * 52, g;
    uint32_t D;
    tsvf_t cn;
    /* (no 64-bit division on the device: every quotient below fits 32 bits, sin and cos >= 0 below fs / 4) */
    tq = den > 64 ? (int32_t)(((uint32_t)((pl * sn) >> 14) << 14) / (uint32_t)den) : 8 * ERS_ONE;   /* (< 2^28) */
    tau8 = w16 > 0 ? (int32_t)(((uint32_t)ers_atan14(tq) << 10) / (uint32_t)w16) : 0;   /* its delay at the note, Q8 */
    D = (uint32_t)((1 << 28) - 2 * pl * cs + pl * pl);                 /* |H(f0)| = a / sqrt(D) (<= 2^29) */
    gn = clamp((int32_t)((15565u * ers_isqrt(D)) / (uint32_t)a), 0, 16220);   /* (isqrt < 2^15) */
    d = (int32_t)b->per8 - tau8;
    d1 = ers_len((int32_t)((((uint32_t)clamp(d, 0, 0x7FFFFFFF) >> 4) * (uint32_t)pos) >> 10), ERS_JET);
    d0 = ers_len(d - (int32_t)d1, ERS_BORE);
    if (st->sinc && (inc > st->sinc + (st->sinc >> 5) || inc < st->sinc - (st->sinc >> 5)))
        st->dip = ERS_ONE;                         /* a slur */
    st->sinc = inc;
    g = ERS_ONE - ((st->dip * SAX_DIP) >> 14);
    st->dip -= st->dip >> 6;
    tsvf_coef(&cn, SAX_NZC, SAX_NZQ);
    for (i = 0; i < b->n; i++) {
        int32_t pi = (ers_p(b, i) * g) >> 14, pc = pi < ERS_ONE ? pi : ERS_ONE, o0, tmp, pmb, pm, bp, lp, air;
        ers_svf(&cn, ers_air(st, 18), st->sn, &bp, &lp);
        air = (bp * pc) >> 14;
        pm = pi ? pm0 + ((pi * pmk) >> 14) + ((air * SAX_NZI) >> 14) : 0;
        o0 = ers_tap(st->bore, ERS_BORE - 1u, w, d0);
        st->sl += ((o0 - st->sl) * a) >> 14;
        tmp = -((st->sl * gn) >> 14);
        pmb = tmp - ers_tap(st->jet, ERS_JET - 1u, w, d1);
        st->bore[w & (ERS_BORE - 1u)] = ers_s16(ers_valve(k, pm, pmb) - tmp);
        st->jet[w & (ERS_JET - 1u)] = ers_s16(tmp);
        b->x[i] = ((((pmb * 3) >> 2) * pc) >> 14) + ((air * SAX_NZO) >> 14);
        w++;
    }
    st->w = w;
}

/* a jet or a valve in a bore: the valve set onto its window first */
static void ers_bore(ers_blk_t *b)
{
    ers_valve_t k = {b->exc, 11469, -(2458 + b->col * 25)};   /* REED: offset 0.7, STIFF 0.15 .. 0.34 */
    int32_t lpb = (128 - b->bri) * 64;             /* the loop's two-tap low-pass, Q14 (0 .. 0.5) */
    int32_t pm0 = 9011, pmk = 7373;                /* the mouth: blown from near the threshold (0.55 .. 1.0) */
    if (ers_is_valve(b->exc)) {                    /* the breath onto the valve's window (ers_valve_set) */
        int32_t R = b->model == M_CYL ? 13107 + b->dec * 25 : b->model == M_VENT ? 14000 : 15565;   /* its return */
        ers_valve_set(&k, b->col, R, &pm0, &pmk);
    }
    switch (b->model) {
    case M_FMB: ers_bore_fm(b); break;
    case M_FLUTE:
    case M_FLUE: ers_bore_flute(b); break;
    case M_CYL: ers_bore_cyl(b, &k, lpb, pm0, pmk); break;
    case M_VENT: ers_bore_vent(b, &k, lpb, pm0, pmk); break;
    case M_SAX: ers_bore_sax(b, &k, pm0, pmk); break;
    default: ers_bore_cone(b, &k, lpb, pm0, pmk); break;
    }
}

/* the bowed string (STK Bowed): two lines, the bow between them. The neck side (1 - 0.127 of the string, bore) and
 * the bridge side (0.127, jet); the bridge's reflection through the string's loss (DECAY) and a one-pole (BRIGHT);
 * the bow table (STK BowTable): the friction (|dv x slope| + 0.75)^-4, slope 5 .. 1 as PRESSURE rises. Breath is the
 * bow's speed (0 .. 0.46). In a bore the string is the sound; MODAL, FORMANT, BYPASS take it in after */
static void ers_bow(ers_blk_t *b)
{
    ers_t *st = b->st;
    int32_t pole = 14000 - b->bri * 72, g = 14746 + b->dec * 12, slope = 20480 - b->col * 129;   /* Q14, Q14, Q12 */
    uint32_t comp = (uint32_t)((pole << 8) / (ERS_ONE - pole)) - 76u;   /* the one-pole's delay, Q8 (-0.3: measured) */
    uint32_t dnk = ers_len((int32_t)((b->per8 * 14303u) >> 14) - (int32_t)comp, ERS_BORE);
    uint32_t dbr = ers_len((int32_t)((b->per8 * 2081u) >> 14), ERS_JET), i, w = st->w;
    for (i = 0; i < b->n; i++) {
        int32_t pi = ers_p(b, i), vel = (pi * 3769) >> 14;
        int32_t br = ers_tap(st->jet, ERS_JET - 1u, w, dbr), nk = ers_tap(st->bore, ERS_BORE - 1u, w, dnk);
        int32_t brefl, nrefl, dv, s, s2, s4, f, nv;
        st->lp += ((br - st->lp) * (ERS_ONE - pole)) >> 14;
        brefl = -((st->lp * g) >> 14);
        nrefl = -nk;
        dv = clamp(vel - (brefl + nrefl), -2 * ERS_ONE, 2 * ERS_ONE);
        s = ((dv * slope) >> 12);
        s = (s < 0 ? -s : s) + 12288;
        if (s > 40960)
            s = 40960;                             /* (2.5: the friction is ~0 by then) */
        s2 = (s * s) >> 14;
        s4 = (int32_t)(mul64(s2, s2) >> 14);
        f = clamp((int32_t)((1u << 28) / (uint32_t)s4), 164, ERS_ONE);   /* 0.01 .. 1 */
        nv = (dv * f) >> 14;
        st->jet[w & (ERS_JET - 1u)] = ers_s16(nrefl + nv);     /* into the bridge side */
        st->bore[w & (ERS_BORE - 1u)] = ers_s16(brefl + nv);   /* into the neck side */
        b->x[i] = br * 3;
        w++;
    }
    st->w = w;
}
