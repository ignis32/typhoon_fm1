/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* The presets over USB-MIDI SysEx: a computer lists, reads, writes, renames and deletes preset slots
 * (tools/presets.py). The design is fm1_open's transfer protocol (m-vave/fm1_open app/transfer.c, V12), with our
 * own tag, so neither tool takes the other's firmware for its own:
 *
 *   F0 7D 'T' 'R' 01 cmd seq <data> sum F7
 *     7D        the MIDI ID for non-commercial use
 *     01        this protocol's version (TSX_VER)
 *     cmd       a command below; the reply has cmd | 0x40, the same seq
 *     seq       0..127, the computer's request number: a request sent again as it was (a lost reply) gets the
 *               same reply again, not done twice
 *     data      8-bit bytes as a stream of 7-bit ones, least significant bit first; a reply's first byte is the
 *               status (TS_*)
 *     sum       the bytes from 7D up to and with sum add up to 0 (mod 128)
 *
 *   cmd                data in                        data out (after the status)
 *   0  INFO            -                              TSX_VER, NPRESET, PS_SLOT (2 B), the slot that plays, edited,
 *                                                     NFACTORY, "TYPHOON" (8 B, 0-padded), the firmware's
 *                                                     version (APP_VER, 8 B, 0-padded: "0.2")
 *   1  LIST            first slot, count (<= 4)       per slot: 1 = saved by the user (0: factory / INIT), name (12 B)
 *   2  READ            slot, offset (2 B), n (<= 64)  n bytes of the slot's record (core/presets.c ps_rec_t);
 *                                                     TS_EMPTY: nothing saved there
 *   3  WRITE BEGIN     slot                           -
 *   4  WRITE DATA      offset (2 B), <= 64 bytes      - (the offsets in order, from 0)
 *   5  WRITE COMMIT    CRC-32 of the 512 B (4 B)      - (the record must be good: its magic, version, CRC, counts)
 *   6  DELETE          slot                           - (the slot plays its factory preset, or INIT, again)
 *   7  RENAME          slot, name (12 B, ASCII)       - (TS_EMPTY: nothing saved there)
 *   8  STATS           -                              the audio ISR's load, % x 10 (2 B): the average, the highest
 *                                                     since the last STATS (it starts again); the xruns since
 *                                                     boot (2 B); the worst single block since the last STATS, %
 *                                                     x 10 (2 B; core/cpumeter.c)
 * The editor (a computer edits what plays, as the knobs do; web/presets/tsx.mjs). A parameter is its handle
 * (core/params.c pref_t, 2 B: layer n's i = n << 8 | i, the sound's i as the preset plays it 0x4000 | i, its global
 * copy 0xC000 | i, a global 0x8000 | i); a value 2 B, signed. A layer's engine (P_ENGINE) is its stable id
 * (engines.h ENG_ID_*) both ways.
 *   9  LAYOUT          -                              NLAYER, P_COUNT, S_COUNT, G_COUNT, NGROUP, NENGINES, the first
 *                                                     engine knob (P_E0), the layer the UI is on
 *   10 DESCRIBE        handle                         fmt (felucca_engine.h F_*), min, max, default (2 B each), the
 *                                                     number of names (F_ENUM), the group (S_*: GR_*, else 255),
 *                                                     label (12 B), unit (4 B); TS_BAD: no such parameter. An engine's
 *                                                     knobs are the layer's engine's (describe again after a change)
 *   11 GET             handle .. (<= 16)              the values
 *   12 SET             handle value .. (<= 12)        the values as they are now (clamped to the range)
 *   13 TEXT            handle, value                  the value as the screen shows it (16 B; F_ENUM: its name)
 *   14 ENGINES         first                          the number of engines in this build, then from first on (<= 6)
 *                                                     each one's id and name (8 B); OFF is not in the list (id 0)
 *   15 GROUP           group, own                     own 1: the preset's own copy of the group (from the global
 *                                                     one), 0: back to the global one; the owned groups after (1 B)
 *   16 LOAD            slot                           the slot plays (as PRESETS turned to it)
 *   17 SAVE            slot, name (12 B; 0s: keep)    what plays into the slot (as SAVE); waits for silence: TS_BUSY
 *   18 STATE           -                              a counter that moves with every change (4 B), the slot that
 *                                                     plays, edited, the owned groups, the UI's layer, the page: the
 *                                                     editor polls it and reads again when it moved
 *
 * Writes (COMMIT, DELETE, RENAME) wait for silence, as the settings do (a flash erase stalls the sound): TS_BUSY
 * while something sounds, and the computer asks again. A write of the slot that plays, not edited, loads it.
 * No question on the device: the user wants the computer to just write. Main loop only (the frame comes from
 * fm1-base's app_sysex: tsx_frame; the answers go out with tp_sysex_send: app.c). */

#define TSX_VER 1u
/* the audio ISR's load (app/app.c app_render): its time over the time its frames play, % x 10; an average over ~16
 * calls, and the highest since STATS read it */
static volatile uint32_t audio_load, audio_peak;
enum { TSX_INFO, TSX_LIST, TSX_READ, TSX_WBEGIN, TSX_WDATA, TSX_WCOMMIT, TSX_DELETE, TSX_RENAME, TSX_STATS,
       TSX_LAYOUT, TSX_DESCRIBE, TSX_GET, TSX_SET, TSX_TEXT, TSX_ENGINES, TSX_GROUP, TSX_LOAD, TSX_SAVE, TSX_STATE };
enum { TS_OK, TS_EMPTY, TS_BUSY, TS_BAD, TS_FAIL, TS_CRC };

static struct {
    uint8_t req[200], rep[100];                    /* the last request and its reply (a retry gets it again) */
    uint32_t req_n, rep_n;
    ps_rec_t w;                                    /* WRITE: the record coming in */
    uint32_t w_at, t_w;
    int32_t w_slot;                                /* -1: no write open */
} tsx = {.w_slot = -1};

static uint32_t tsx_unpack(const uint8_t *p, uint32_t n, uint8_t *out, uint32_t max)   /* -> bytes, or ~0u: bad */
{
    uint32_t i, value = 0, bits = 0, k = 0;
    for (i = 0; i < n; i++) {
        value |= (uint32_t)p[i] << bits;
        bits += 7u;
        if (bits >= 8u) {
            if (k == max)
                return ~0u;
            out[k++] = (uint8_t)value;
            value >>= 8;
            bits -= 8u;
        }
    }
    return value || bits >= 7u ? ~0u : k;
}

static uint32_t tsx_pack(const uint8_t *p, uint32_t n, uint8_t *out)
{
    uint32_t i, value = 0, bits = 0, k = 0;
    for (i = 0; i < n; i++) {
        value |= (uint32_t)p[i] << bits;
        bits += 8u;
        while (bits >= 7u) {
            out[k++] = (uint8_t)(value & 127u);
            value >>= 7;
            bits -= 7u;
        }
    }
    if (bits)
        out[k++] = (uint8_t)(value & 127u);
    return k;
}

static uint32_t rd16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
static int32_t rds16(const uint8_t *p) { return (int16_t)rd16(p); }
static void wr16(uint8_t *p, int32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)((uint32_t)v >> 8); }
static void wrname(uint8_t *p, const char *s, uint32_t n)   /* s into n bytes, 0-padded */
{
    uint32_t i;
    for (i = 0; i < n; i++) p[i] = 0;
    for (i = 0; s && i < n && s[i]; i++) p[i] = (uint8_t)s[i];
}

/* a parameter the editor may name: a layer's (P_*), the sound's (either copy), a global */
static int tsx_par_ok(pref_t r)
{
    uint32_t i = r & 0xFFu;
    if (r & PR_SND) return i < S_COUNT && !(r & 0x3F00u);
    if (r & PR_GLOBAL) return i < G_COUNT && !(r & 0x3F00u);
    return ((r >> 8) & 0x0Fu) < NLAYER && i < P_COUNT && !(r & 0x3000u);   /* (an explicit layer, not PR_SEL) */
}
static int32_t tsx_get(pref_t r)
{
    return par_get(r);                             /* (P_ENGINE: the id) */
}
static void tsx_set(pref_t r, int32_t v)
{
    if (pr_layer(r) && (r & 0xFFu) == P_ENGINE) {  /* an engine by its id: par_set wants its place (NENGINES: OFF) */
        uint8_t miss;
        uint32_t idx = v == ENG_ID_OFF ? NENGINES : eng_index((uint32_t)v, &miss);
        if (v != ENG_ID_OFF && miss)
            return;                                /* (not in this build: nothing changes) */
        par_set(r, (int32_t)idx);
        return;
    }
    par_set(r, v);
}
static uint32_t rd32(const uint8_t *p) { return rd16(p) | rd16(p + 2) << 16; }

static void tsx_loaded(uint32_t k)                 /* slot k changed: what plays follows it when it is k, unedited */
{
    if (k == ps.cur && !ps_edited)
        ps_load(k);
}

static void tsx_note(const char *what, uint32_t k) /* the header says it a moment: "USB > 23 CLARINET" */
{
    char s[24];
    put_str(s, what);
    slot_text(k, ps_ix[k].name, s + par_strlen(s));
    ui_msg(s);
}

static int tsx_rec_good(const ps_rec_t *r)
{
    return ps_ok(r) && r->nl >= 1u && r->nl <= PS_NL && r->np > ps_nepar(r) && r->np <= ps_np_room(r) && r->ns <= PS_NS;
}

/* one command: data d (n bytes) -> o (*on bytes); the status */
static int tsx_command(uint32_t cmd, const uint8_t *d, uint32_t n, uint8_t *o, uint32_t *on)
{
    uint32_t k, i;
    *on = 0;
    switch (cmd) {
    case TSX_INFO: {
        static const char NAME[8] = "TYPHOON", VER[] = APP_VER;
        if (n) return TS_BAD;
        o[0] = TSX_VER; o[1] = NPRESET; o[2] = PS_SLOT & 255u; o[3] = PS_SLOT >> 8;
        o[4] = ps.cur; o[5] = ps_edited; o[6] = (uint8_t)NFACTORY;
        for (i = 0; i < 8u; i++) o[7 + i] = (uint8_t)NAME[i];
        for (i = 0; i < 8u; i++) o[15 + i] = i < sizeof VER - 1u ? (uint8_t)VER[i] : 0;
        *on = 23;
        return TS_OK;
    }
    case TSX_LIST:
        if (n != 2u || d[0] >= NPRESET || !d[1] || d[1] > 4u || d[0] + d[1] > NPRESET) return TS_BAD;
        for (k = 0; k < d[1]; k++) {
            const char *nm = ps_ix[d[0] + k].name;
            uint8_t *q = o + k * 13u;
            q[0] = ps_ix[d[0] + k].user;
            for (i = 0; i < PS_NAME; i++) q[1 + i] = 0;
            for (i = 0; i < PS_NAME && nm[i]; i++) q[1 + i] = (uint8_t)nm[i];
        }
        *on = d[1] * 13u;
        return TS_OK;
    case TSX_READ: {
        uint32_t at = n == 4u ? rd16(d + 1) : 0, len = n == 4u ? d[3] : 0;
        if (n != 4u || d[0] >= NPRESET || !len || len > 64u || at + len > PS_SLOT) return TS_BAD;
        if (ps_read(d[0], &ps_tmp)) return TS_EMPTY;
        memcpy(o, (const uint8_t *)&ps_tmp + at, len);
        *on = len;
        return TS_OK;
    }
    case TSX_WBEGIN:
        if (n != 1u || d[0] >= NPRESET) return TS_BAD;
        tsx.w_slot = d[0];
        tsx.w_at = 0;
        tsx.t_w = fm1_ms | 1u;
        return TS_OK;
    case TSX_WDATA:
        if (tsx.w_slot < 0 || n < 3u || n > 66u || rd16(d) != tsx.w_at || tsx.w_at + (n - 2u) > PS_SLOT) return TS_BAD;
        memcpy((uint8_t *)&tsx.w + tsx.w_at, d + 2, n - 2u);
        tsx.w_at += n - 2u;
        tsx.t_w = fm1_ms | 1u;
        return TS_OK;
    case TSX_WCOMMIT:
        if (tsx.w_slot < 0 || n != 4u || tsx.w_at != PS_SLOT) return TS_BAD;
        if (rd32(d) != ps_crc((const uint8_t *)&tsx.w, PS_SLOT)) return TS_CRC;
        if (!tsx_rec_good(&tsx.w)) return TS_BAD;
        if (!layers_quiet()) return TS_BUSY;
        k = (uint32_t)tsx.w_slot;
        tsx.w_slot = -1;
        if (ps_put(k, &tsx.w)) return TS_FAIL;
        tsx_loaded(k);
        tsx_note("USB > ", k);
        return TS_OK;
    case TSX_DELETE:
        if (n != 1u || d[0] >= NPRESET) return TS_BAD;
        if (!ps_ix[d[0]].user) return TS_OK;      /* (nothing saved there: already so) */
        if (!layers_quiet()) return TS_BUSY;
        if (ps_put(d[0], 0)) return TS_FAIL;
        tsx_loaded(d[0]);
        tsx_note("USB X ", d[0]);
        return TS_OK;
    case TSX_RENAME:
        if (n != 1u + PS_NAME || d[0] >= NPRESET) return TS_BAD;
        for (i = 0; i < PS_NAME; i++)
            if (d[1 + i] && (d[1 + i] < 32u || d[1 + i] > 126u)) return TS_BAD;
        if (ps_read(d[0], &ps_tmp)) return TS_EMPTY;
        if (!layers_quiet()) return TS_BUSY;
        memcpy(&tsx.w, &ps_tmp, PS_SLOT);
        for (i = 0; i < PS_NAME; i++) tsx.w.name[i] = (char)d[1 + i];
        for (i = PS_NAME; i && (tsx.w.name[i - 1u] == ' ' || !tsx.w.name[i - 1u]); i--) tsx.w.name[i - 1u] = 0;
        tsx.w.crc = ps_crc((const uint8_t *)&tsx.w + 8, PS_SLOT - 8u);
        tsx.w_slot = -1;
        if (ps_put(d[0], &tsx.w)) return TS_FAIL;
        if (d[0] == ps.cur)
            name_set(ps.name, ps_ix[d[0]].name, PS_NAME);
        tsx_note("USB > ", d[0]);
        return TS_OK;
    case TSX_STATS: {
        uint32_t a = audio_load, p = audio_peak, x = plat_xruns(), w = cpum.stat;
        if (n) return TS_BAD;
        audio_peak = 0;
        cpum.stat = 0;
        if (x > 0xFFFFu) x = 0xFFFFu;
        o[0] = (uint8_t)a; o[1] = (uint8_t)(a >> 8); o[2] = (uint8_t)p; o[3] = (uint8_t)(p >> 8);
        o[4] = (uint8_t)x; o[5] = (uint8_t)(x >> 8); o[6] = (uint8_t)w; o[7] = (uint8_t)(w >> 8);
        *on = 8;
        return TS_OK;
    }
    case TSX_LAYOUT:
        if (n) return TS_BAD;
        o[0] = NLAYER; o[1] = P_COUNT; o[2] = S_COUNT; o[3] = G_COUNT; o[4] = NGROUP; o[5] = (uint8_t)NENGINES;
        o[6] = P_E0; o[7] = (uint8_t)gpar[G_LAYER];
        *on = 8;
        return TS_OK;
    case TSX_DESCRIBE: {
        pref_t r = n == 2u ? (pref_t)rd16(d) : PR_NONE;
        const param_desc_t *pd = r != PR_NONE && tsx_par_ok(r) ? par_desc(r) : 0;
        if (!pd) return TS_BAD;
        o[0] = pd->fmt;
        wr16(o + 1, pd->min); wr16(o + 3, pd->max); wr16(o + 5, pd->def);
        if (pr_layer(r) && (r & 0xFFu) == P_ENGINE) {   /* (the engine: ids, any known one; ENGINES lists this build's) */
            wr16(o + 1, 0); wr16(o + 3, ENG_ID_ERS); wr16(o + 5, ENGINES[0].id);
        }
        o[7] = pd->fmt == F_ENUM && pd->names ? (uint8_t)(pd->max - pd->min + 1) : 0;
        o[8] = (r & PR_SND) ? S_GROUP[(r & 0xFFu) % S_COUNT] : 255u;
        wrname(o + 9, pd->label, 12);
        wrname(o + 21, pd->unit, 4);
        *on = 25;
        return TS_OK;
    }
    case TSX_GET:
        if (!n || n > 32u || (n & 1u)) return TS_BAD;
        for (i = 0; i < n; i += 2u) {
            pref_t r = (pref_t)rd16(d + i);
            if (!tsx_par_ok(r)) return TS_BAD;
            wr16(o + i, tsx_get(r));
        }
        *on = n;
        return TS_OK;
    case TSX_SET:
        if (!n || n > 48u || (n & 3u)) return TS_BAD;
        for (i = 0; i < n; i += 4u)
            if (!tsx_par_ok((pref_t)rd16(d + i))) return TS_BAD;
        for (i = 0; i < n; i += 4u) {
            pref_t r = (pref_t)rd16(d + i);
            tsx_set(r, rds16(d + i + 2u));
            wr16(o + i / 2u, tsx_get(r));
        }
        *on = n / 2u;
        return TS_OK;
    case TSX_TEXT: {
        char s[24];
        pref_t r = n == 4u ? (pref_t)rd16(d) : PR_NONE;
        if (r == PR_NONE || !tsx_par_ok(r) || !par_desc(r)) return TS_BAD;
        par_value_text(r, rds16(d + 2), s);
        wrname(o, s, 16);
        *on = 16;
        return TS_OK;
    }
    case TSX_ENGINES:
        if (n != 1u) return TS_BAD;
        o[0] = (uint8_t)NENGINES;
        for (k = d[0], i = 0; k < NENGINES && i < 6u; k++, i++) {
            o[1 + i * 9u] = (uint8_t)ENGINES[k].id;
            wrname(o + 2 + i * 9u, ENGINES[k].e->name, 8);
        }
        *on = 1 + i * 9u;
        return TS_OK;
    case TSX_GROUP:
        if (n != 2u || d[0] >= NGROUP) return TS_BAD;
        if (d[1])
            group_own(d[0]);
        else
            group_global(d[0]);
        o[0] = s_own;
        *on = 1;
        return TS_OK;
    case TSX_LOAD:
        if (n != 1u || d[0] >= NPRESET) return TS_BAD;
        ps_load(d[0]);
        return TS_OK;
    case TSX_SAVE:
        if (n != 1u + PS_NAME || d[0] >= NPRESET) return TS_BAD;
        for (i = 0; i < PS_NAME; i++)
            if (d[1 + i] && (d[1 + i] < 32u || d[1 + i] > 126u)) return TS_BAD;
        if (!layers_quiet()) return TS_BUSY;
        if (d[1]) {
            char nm[PS_NAME + 1];
            for (i = 0; i < PS_NAME; i++) nm[i] = (char)d[1 + i];
            nm[PS_NAME] = 0;
            for (i = PS_NAME; i && (nm[i - 1u] == ' ' || !nm[i - 1u]); i--) nm[i - 1u] = 0;
            name_set(ps.name, nm, PS_NAME);
        }
        if (ps_save(d[0])) return TS_FAIL;
        tsx_note("USB > ", d[0]);
        return TS_OK;
    case TSX_STATE:
        if (n) return TS_BAD;
        o[0] = (uint8_t)par_dirty; o[1] = (uint8_t)(par_dirty >> 8); o[2] = (uint8_t)(par_dirty >> 16);
        o[3] = (uint8_t)(par_dirty >> 24);
        o[4] = ps.cur; o[5] = ps_edited; o[6] = s_own; o[7] = (uint8_t)gpar[G_LAYER]; o[8] = (uint8_t)ui.page;
        *on = 9;
        return TS_OK;
    }
    return TS_BAD;
}

static void tsx_tick(void)                          /* main loop */
{
    if (tsx.w_slot >= 0 && ms_since(tsx.t_w) > 10000u)
        tsx.w_slot = -1;                           /* a write that stopped half way */
}

/* a SysEx frame from USB (the bytes between F0 and F7: fm1-base's app_sysex): 1 = ours, answered */
static int tsx_frame(const uint8_t *p, uint32_t n)
{
    uint8_t d[140], o[1 + 64], *r = tsx.rep;
    uint32_t i, s = 0, dn, on = 0, at;
    int st;
    if (n < 7u || p[0] != 0x7Du || p[1] != 'T' || p[2] != 'R')
        return 0;                                  /* (not ours: the platform's, or another app's) */
    if (n > sizeof tsx.req || p[3] != TSX_VER || p[4] >= 64u)
        return 1;                                  /* (ours, not this version: dropped) */
    if (n == tsx.req_n && tsx.rep_n && !memcmp(p, tsx.req, n)) {
        tp_sysex_send(tsx.rep, tsx.rep_n);       /* the same request again: the same reply */
        return 1;
    }
    memcpy(tsx.req, p, n);
    tsx.req_n = n;
    tsx.rep_n = 0;
    for (i = 0; i < n; i++)
        s += tsx.req[i];
    if (s & 127u)
        return 1;                                  /* (a bad sum: no reply, the computer asks again) */
    dn = tsx_unpack(tsx.req + 6, n - 7u, d, sizeof d);
    if (dn == ~0u)
        return 1;
    st = tsx_command(tsx.req[4], d, dn, o + 1, &on);
    o[0] = (uint8_t)st;
    r[0] = 0xF0;
    for (i = 0; i < 6u; i++)
        r[1 + i] = tsx.req[i];
    r[5] |= 0x40u;
    at = 7u + tsx_pack(o, 1u + on, r + 7);
    for (i = 1, s = 0; i < at; i++)
        s += r[i];
    r[at++] = (uint8_t)((0u - s) & 127u);
    r[at++] = 0xF7;
    tsx.rep_n = st == TS_BUSY ? 0 : at;            /* (BUSY: a retry asks again, it is not answered from the cache) */
    tp_sysex_send(r, at);
    return 1;
}
