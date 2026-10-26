/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* GLOBAL > USB AUDIO (G_USB_*): the FM-1's audio over USB, each way on its own (fm1-base plat_usb_audio_set:
 * patches/fm1-base/0001 and patches/felucca-bsp/0001).
 *   USB IN   the computer's audio through the FM-1: OFF (the computer has no such audio device: it keeps its own
 *            default), MUTE (there, silent), ON at IN LEVEL (0 .. 200 %)
 *   USB OUT  the FM-1 recorded on the computer: OFF, MUTE, MIX (the computer's audio in it, as the speaker has it),
 *            SYNTH (typhoon alone), at OUT LEVEL
 * MASTER is the speaker's and the headphones' only. Levels and MUTE apply at once; a change to or from OFF once the
 * knob has rested 0.8 s (the FM-1 drops off USB for about a second, MIDI too, and comes back with the other audio
 * devices). At power-on (app_init, before USB starts) as saved, so the computer never sees what is OFF. */
static struct {
    int16_t knobs[4];                              /* G_USB_IN .. G_USB_OUT_LV as last seen */
    uint32_t t_moved;                              /* when they last moved */
    uint8_t in_on, out_on;                         /* the devices presented (not OFF) */
} ua;

static void ua_send(void)
{
    const int in = gpar[G_USB_IN], out = gpar[G_USB_OUT];
    /* (a device still presented while its OFF waits: silent; one not presented yet: OFF until then) */
    plat_usb_audio_set(!ua.out_on ? USB_AUDIO_OFF : out >= 2 ? USB_AUDIO_ON : USB_AUDIO_MUTE,
                       (uint32_t)gpar[G_USB_OUT_LV] * 4096u / 100u,
                       !ua.in_on ? USB_AUDIO_OFF : in == 2 ? USB_AUDIO_ON : USB_AUDIO_MUTE,
                       (uint32_t)gpar[G_USB_IN_LV] * 4096u / 100u, out == 3 ? USB_REC_APP : USB_REC_MIX);
}

static void ua_init(void)                          /* app_init, after the state */
{
    ua.in_on = gpar[G_USB_IN] != 0;
    ua.out_on = gpar[G_USB_OUT] != 0;
    memcpy(ua.knobs, &gpar[G_USB_IN], sizeof ua.knobs);
    ua_send();
}

static void ua_poll(void)                          /* app_frame */
{
    if (memcmp(ua.knobs, &gpar[G_USB_IN], sizeof ua.knobs)) {   /* (G_USB_IN .. G_USB_OUT_LV: in a row) */
        memcpy(ua.knobs, &gpar[G_USB_IN], sizeof ua.knobs);
        ua.t_moved = fm1_ms;
        ua_send();                                 /* levels, MUTE, the source: at once */
    }
    if (((gpar[G_USB_IN] != 0) != ua.in_on || (gpar[G_USB_OUT] != 0) != ua.out_on) && fm1_ms - ua.t_moved > 800u) {
        ua.in_on = gpar[G_USB_IN] != 0;
        ua.out_on = gpar[G_USB_OUT] != 0;
        ua_send();                                 /* to or from OFF: the FM-1 presents itself again */
    }
}
_Static_assert(G_USB_IN_LV == G_USB_IN + 1 && G_USB_OUT == G_USB_IN + 2 && G_USB_OUT_LV == G_USB_IN + 3,
               "USB AUDIO's knobs in a row (ua.knobs)");
