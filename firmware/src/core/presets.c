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
 * A record keeps the counts it was written with (layers, P_COUNT, S_COUNT) and is read by them: its last NEPAR values
 * are P_E0.., the ones before are the commons in order, S_* go by index. New parameters therefore go just before P_E0
 * (layers.h) or at the end of S_*, and take their defaults in records written before them; the record has room for
 * 3 layers of 48, the routes (PS_NCELL) and 48 S_*. Only version PS_VER is read: older records (from before
 * 2026-10-30, the sources) are not (no user presets to keep then: the user, 2026-10-30), their slots play their
 * factory presets until saved again.
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
#define PS_VER 5u                                  /* 5 (2026-10-30): the sources (no breath curve, no LP group in S_*),
                                                    * the routes; 4 and older are not read */
#define PS_NL 3u
#define PS_NP 48u
#define PS_NS 48u
#define PS_NAME 12u
#define PS_NCELL 32u                               /* the routes a preset has room for (core/routes.c), 3 bytes each */
typedef struct {
    uint32_t magic, crc;                           /* crc: of every byte after it */
    uint8_t ver, nl, np, ns;                       /* PS_VER; NLAYER, P_COUNT, S_COUNT when written */
    uint8_t own, spare[3];                         /* s_own */
    char name[PS_NAME];                            /* ASCII, 0-padded (no 0 when 12 long) */
    int16_t p[PS_NL][PS_NP];
    uint8_t cell[PS_NCELL][3];                     /* the routes (where version 4 had its fourth layer's room) */
    int16_t s[PS_NS];                              /* sown (every group's, owned or not) */
    uint8_t reserved[PS_SLOT - 28u - PS_NL * PS_NP * 2u - PS_NCELL * 3u - PS_NS * 2u];
} ps_rec_t;
_Static_assert(sizeof(ps_rec_t) == PS_SLOT, "preset record layout");
_Static_assert(NLAYER <= PS_NL && P_COUNT <= PS_NP && S_COUNT <= PS_NS && NGROUP <= 8u && NCELL == PS_NCELL &&
               NTARGET == T_KNOB + NEPAR && NTARGET <= 64u && NLAYER <= 3u, "preset record: room (a cell: 2 bits of layer, 6 of target)");
_Static_assert(2u * PS_COPY <= 16u * 0x1000u && PS_HI_AT + 2u * PS_HI_COPY <= STORE_LEN && NPRESET % PS_PER_SEC == 0u,
               "presets: the store (sector 16 is the BT stack's)");

/* ---- the factory presets: changes to the INIT sound, in order (an engine before its layer's parameters) */
enum { FP_END, FP_ENG, FP_PAR, FP_SND, FP_CELL };
typedef struct { uint8_t op, l, i; int16_t v; } fp_t;
#define FACTORY_OPS 48                             /* a preset's changes, its end among them (tools/factory_from_backup.mjs
                                                    * keeps to it; 24 until 2026-10-29) */
typedef struct { const char *name; fp_t f[FACTORY_OPS]; } factory_t;
#define ENG(l, id, pre) {FP_ENG, (l), (id), (pre)}   /* layer l plays engine id, with that engine's preset pre */
#define PAR(l, i, v) {FP_PAR, (l), (i), (v)}
#define SND(i, v) {FP_SND, 0, (i), (v)}              /* the preset owns i's group, i = v */
#define CEL(l, s, t, a) {FP_CELL, (l), (t), (int16_t)((s) << 8 | ((a) & 0xFF))}   /* a route: source s -> layer l's
                                                    * target t at a % (0: none; core/routes.c) */
/* the table itself: core/factory.h, made from the user's own presets (2026-10-29: the 26 kept of 92; the others, and
 * their comments, in git history before it) */
#include "factory.h"
#undef ENG
#undef PAR
#undef SND
#undef CEL
#define NFACTORY ((uint32_t)NELEM(FACTORY))

static struct { char name[PS_NAME + 1]; uint8_t user; } ps_ix[NPRESET];   /* every slot's name (the browser) */
static struct {
    uint8_t cur;                                   /* the slot loaded / saved last */
    char name[PS_NAME + 1];                        /* the name of what plays */
} ps;
static ps_rec_t ps_tmp __attribute__((section(".pool"))), ps_new __attribute__((section(".pool")));   /* (the pool:
                                                    * main loop only, RAM kept for the audio) */
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
    return r->magic == PS_MAGIC && r->ver == PS_VER && r->crc == ps_crc((const uint8_t *)r + 8, PS_SLOT - 8u);
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
            if (f[j].i == P_TONE)                  /* (a TONE's routes: core/routes.c routes_tone) */
                routes_tone(l, f[j].v);
            break;
        case FP_SND:
            group_own(S_GROUP[f[j].i]);
            sown[f[j].i] = f[j].v;
            break;
        case FP_CELL:
            cell_set(l, f[j].i, (uint32_t)f[j].v >> 8 & 15u, (int8_t)(f[j].v & 0xFF));
            break;
        }
    }
}

/* the routes as a record or the state keep them (3 bytes each: layer << 6 | target, source, amount), and back */
static void cells_put(uint8_t (*c)[3])
{
    uint32_t i;
    for (i = 0; i < NCELL; i++) {
        c[i][0] = cells[i].lt;
        c[i][1] = cells[i].src;
        c[i][2] = (uint8_t)cells[i].amt;
    }
}
static void cells_get(const uint8_t (*c)[3])
{
    uint32_t i;
    for (i = 0; i < NCELL; i++) {
        int8_t a = (int8_t)c[i][2];
        cells[i].amt = 0;
        if (!a || (c[i][0] >> 6) >= NLAYER || (c[i][0] & 63u) >= NTARGET || c[i][1] >= NSRC || a < -100 || a > 100)
            continue;                              /* (none, or nothing this build knows) */
        cells[i].lt = c[i][0];
        cells[i].src = c[i][1];
        RING_PUBLISH();
        cells[i].amt = a;
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
    cells_get(r->cell);
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
    ps_new.ver = PS_VER;
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
    cells_put(ps_new.cell);
    memcpy(ps_new.s, sown, sizeof sown);
    ps_new.crc = ps_crc((const uint8_t *)&ps_new + 8, PS_SLOT - 8u);
    if ((rc = ps_put(k, &ps_new)) != 0)
        return rc;
    ps.cur = (uint8_t)k;
    ps_edited = 0;
    par_dirty++;
    return 0;
}

/* ---- the state in the settings object (fm1-base's OBJ_STATE, up to PLAT_STORE_MAX). With room to grow: the counts it
 * was saved with say what is there, and are read by them as the presets are (layers: the commons, then the NEPAR
 * engine values; S_* and G_* by index): a new parameter (just before P_E0, or at the end of S_* / G_*) takes its
 * default, nothing is lost. Another layout is a new STATE_MAGIC; an older state is not read: the globals start from
 * their defaults once (0x7A0B and older, before 2026-10-30, the sources: the user's call, no conversions) */
#define STATE_MAGIC 0x7A0Cu                        /* change it only when the layout below changes (0x7A0C: the
                                                    * sources' globals, the routes) */
#define P_SAVED 48u
#define S_SAVED 24u
typedef struct {
    uint16_t magic;
    uint8_t ng, ns, np, nl;                        /* G_COUNT, S_COUNT, P_COUNT, NLAYER when saved */
    uint8_t cur, edited, own, spare;
    int16_t g[G_SAVED];
    int16_t sg[S_SAVED];                           /* the global sound */
    int16_t so[S_SAVED];                           /* the edit buffer: its own sound, its layers, its routes, its name */
    int16_t p[NLAYER][P_SAVED];
    uint8_t cell[PS_NCELL][3];
    char name[PS_NAME];
} state_t;
_Static_assert(sizeof(state_t) <= PLAT_STORE_MAX, "the settings object: room");
_Static_assert(G_COUNT <= G_SAVED && S_COUNT <= S_SAVED && P_COUNT <= P_SAVED, "the saved state: room");
static state_t state_buf __attribute__((section(".pool")));   /* (loading and saving: the main loop's, in the pool) */
static void state_load(void)
{
    state_t *s = &state_buf;
    uint32_t i, n, nc;
    params_defaults();
    for (i = 0; i < NPRESET; i++)
        ps_index(i);
    if (settings_load(s, sizeof *s) || s->magic != STATE_MAGIC || s->ng > G_SAVED || s->ns > S_SAVED ||
        s->np < NEPAR || s->np > P_SAVED || s->nl != NLAYER) {
        ps_load(0);                                /* nothing saved, or a layout we do not know: the first preset */
        return;
    }
    for (i = 0; i < G_COUNT && i < s->ng; i++)
        gpar[i] = (int16_t)clamp(s->g[i], GLOBAL[i].min, GLOBAL[i].max);
    for (i = 0; i < S_COUNT && i < s->ns; i++) {
        sglob[i] = (int16_t)clamp(s->sg[i], SOUND[i].min, SOUND[i].max);
        sown[i] = (int16_t)clamp(s->so[i], SOUND[i].min, SOUND[i].max);
    }
    nc = s->np - NEPAR;                            /* the commons saved; then the engine's NEPAR */
    for (n = 0; n < NLAYER; n++) {
        for (i = 0; i < NCOMMON && i < nc; i++)
            trk[n].p[i] = s->p[n][i];              /* (an engine this build lacks: layers.c plays ENGINES[0]) */
        for (i = 0; i < NEPAR; i++)
            trk[n].p[P_E0 + i] = s->p[n][nc + i];
    }
    cells_get(s->cell);
    s_own = (uint8_t)(s->own & ((1u << NGROUP) - 1u));
    ps.cur = (uint8_t)(s->cur % NPRESET);
    ps_edited = s->edited ? 1u : 0u;
    name_set(ps.name, s->name, PS_NAME);
    par_dirty++;
}

static void state_save(void)
{
    state_t *s = &state_buf;
    uint32_t n, j;
    memset(s, 0, sizeof *s);
    s->magic = STATE_MAGIC;
    s->ng = G_COUNT;
    s->ns = S_COUNT;
    s->np = P_COUNT;
    s->nl = NLAYER;
    s->cur = ps.cur;
    s->edited = ps_edited;
    s->own = s_own;
    memcpy(s->g, gpar, sizeof gpar);
    memcpy(s->sg, sglob, sizeof sglob);
    memcpy(s->so, sown, sizeof sown);
    for (n = 0; n < NLAYER; n++)
        memcpy(s->p[n], trk[n].p, sizeof trk[n].p);
    cells_put(s->cell);
    for (j = 0; j < PS_NAME && ps.name[j]; j++)
        s->name[j] = ps.name[j];
    settings_save(s, sizeof *s);
}

#if defined(TYPHOON_SIM)
static const char *host_factory_name(uint32_t k) { return k < NFACTORY ? FACTORY[k].name : 0; }   /* the simulator (sim_factory_name) */
#endif
