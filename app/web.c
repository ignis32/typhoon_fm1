/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* typhoon's own exports for fm1-base's browser emulator (base/web/emu/emu_web.c includes this with APP_WEB, at its
 * end: plat_host.c and the whole app are above). The page keeps fm1-base's store objects (the state: what plays); the
 * presets are typhoon's own, in the data area (core/presets.c), so app/web/emu/ keeps those: app_worklet.js reads
 * them out when they change and writes them back after boot, then web_presets_rescan. */

/* the presets' store in the simulated data area (STORE_LEN bytes at STORE_AT) */
__attribute__((used, visibility("default"))) uint8_t *web_presets(void)
{
    data_init();
    return &data_area[STORE_AT - DATA_BASE];
}
__attribute__((used, visibility("default"))) uint32_t web_presets_len(void) { return STORE_LEN; }
/* changes when a save, a rename or a delete does (FNV-1a over the store) */
__attribute__((used, visibility("default"))) uint32_t web_presets_hash(void)
{
    const uint8_t *s = web_presets();
    uint32_t h = 2166136261u, i;
    for (i = 0; i < STORE_LEN; i++)
        h = (h ^ s[i]) * 16777619u;
    return h;
}
/* GLOBAL > BREATH CC: what the page's breath slider sends on (1..127 a CC, 128 channel aftertouch) */
__attribute__((used, visibility("default"))) uint32_t web_breath_cc(void) { return (uint32_t)gpar[G_BREATH_CC]; }
/* after the page wrote saved presets back: the slots' index again, as at boot (core/presets.c state_load) */
__attribute__((used, visibility("default"))) void web_presets_rescan(void)
{
    uint32_t i;
    for (i = 0; i < NPRESET; i++)
        ps_index(i);
    ui.full = 1;
}
