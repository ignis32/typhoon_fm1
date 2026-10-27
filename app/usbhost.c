/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 ignis32 */
/* GLOBAL > USB HOST (G_USB_HOST; fm1-base plat_usb_host, fm1.toml usb_host): the USB port plays host to a USB MIDI
 * device, the re.corder by cable, directly or behind one hub. The FM-1 gives the port no power: a powered hub or a
 * powered OTG adapter. Meanwhile no computer: no USB audio, no editor or installer over USB (OFF, or REC held at
 * power-on: update mode). It plays as MIDI in does from anywhere (app_midi_poll). Saved: at power-on the FM-1 starts
 * as a device and turns host on its first frame. A change once the knob has rested 0.8 s (as USB AUDIO's OFF). */
static struct {
    int8_t on;                                     /* as applied (-1: not yet, at boot) */
    uint32_t t_moved;
    int16_t knob;
} uhost = {-1, 0, 0};

static void uhost_poll(void)                       /* app_frame (the main loop) */
{
    static const char *const SAY[] = {
        [USBH_MSG_ON] = "USB HOST: LOOKING", [USBH_MSG_OFF] = "USB HOST OFF",
        [USBH_MSG_MIDI] = "USB MIDI DEVICE ON", [USBH_MSG_NOMIDI] = "USB: NOT A MIDI DEVICE",
        [USBH_MSG_LOW] = "USB: LOW SPEED, NOT SUPPORTED", [USBH_MSG_LOST] = "USB MIDI DEVICE GONE",
    };
    int m;
    if (gpar[G_USB_HOST] != uhost.knob) {
        uhost.knob = gpar[G_USB_HOST];
        uhost.t_moved = fm1_ms;
    }
    if (uhost.on < 0 || (uhost.knob != uhost.on && fm1_ms - uhost.t_moved > 800u)) {
        if (uhost.on >= 0 || uhost.knob)
            plat_usb_host(uhost.knob);
        uhost.on = (int8_t)uhost.knob;
    }
    m = plat_usb_host_msg();
    if (m > USBH_MSG_NONE && m <= USBH_MSG_LOST && SAY[m])
        app_say(SAY[m], 0);
    if (plat_usb_lost())                           /* a USB session ended: what its notes hold, let go */
        perf_cut();
}
