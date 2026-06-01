/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../inc/usb.h"
#include "../inc/usb_core.h"
#include "../inc/kernel.h"
#include "../inc/kmalloc.h"

#define UAC_CTRL      0x01
#define UAC_STREAM    0x02
#define UAC_MIDI      0x03
#define UAC_CS_IFACE  0x24
#define UAC_AC_HDR    0x01
#define UAC_AC_FU     0x06
#define UAC_AS_FMT    0x02
#define UAC_SET_CUR   0x01
#define UAC_GET_MIN   0x82
#define UAC_GET_MAX   0x83
#define UAC_FU_MUTE   0x01
#define UAC_FU_VOL    0x02
#define UAC_EP_FREQ   0x01

typedef struct __attribute__((packed)) {
    uint8_t bLength, bDescriptorType, bDescriptorSubtype, bFormatType;
    uint8_t bNrChannels, bSubframeSize, bBitResolution, bSamFreqType;
    uint8_t tSamFreq[3];
} fmt_t;

static inline uint32_t dec_freq(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16);
}

#define MAX_EPS   4
#define MAX_FREQS 8

typedef struct {
    uint8_t  ep_addr, dir_in, nchan, bits, subframe_sz;
    uint32_t freqs[MAX_FREQS];
    uint8_t  nfreqs;
    uint32_t freq;
    uint8_t  iface_num, alt;
} aep_t;

typedef struct {
    usb_device_t *dev;
    aep_t   eps[MAX_EPS];
    uint8_t neps, ctrl_if, fu_id;
    int16_t vol_min, vol_max, vol;
} audio_t;

static int set_freq(usb_device_t *d, uint8_t ep_addr, uint32_t f)
{
    uint8_t v[3] = { (uint8_t)f, (uint8_t)(f>>8), (uint8_t)(f>>16) };
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_ENDPOINT,
                   UAC_SET_CUR, (uint16_t)(UAC_EP_FREQ<<8), ep_addr, 3);
    return usb_control_out(d, &s, v, 3);
}

int usb_audio_set_volume(usb_device_t *d, int16_t vol)
{
    audio_t *a = d->driver_data;
    if (!a) return -1;
    uint8_t v[2] = { (uint8_t)(vol & 0xFF), (uint8_t)((vol>>8)&0xFF) };
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   UAC_SET_CUR,
                   (uint16_t)((UAC_FU_VOL<<8)|0x01),
                   (uint16_t)((a->fu_id<<8)|a->ctrl_if), 2);
    int r = usb_control_out(d, &s, v, 2);
    if (r == 0) a->vol = vol;
    return r;
}

int usb_audio_set_mute(usb_device_t *d, int mute)
{
    audio_t *a = d->driver_data;
    if (!a) return -1;
    uint8_t v = mute ? 1 : 0;
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   UAC_SET_CUR,
                   (uint16_t)(UAC_FU_MUTE<<8),
                   (uint16_t)((a->fu_id<<8)|a->ctrl_if), 1);
    return usb_control_out(d, &s, &v, 1);
}

static int find_alt(usb_device_t *d, uint8_t sn, aep_t *ep,
                     const uint8_t *cfg, uint16_t clen)
{
    const uint8_t *p = cfg, *end = cfg + clen;
    uint8_t ci = 0xFF, ca = 0xFF;
    while (p < end && p[0] >= 2) {
        if (p[1] == USB_DESC_INTERFACE) {
            const usb_iface_desc_t *id = (const usb_iface_desc_t *)p;
            ci = id->bInterfaceNumber; ca = id->bAlternateSetting;
        }
        if (ci == sn && ca > 0 && p[1] == UAC_CS_IFACE && p[2] == UAC_AS_FMT &&
            p[0] >= (uint8_t)sizeof(fmt_t)) {
            const fmt_t *f = (const fmt_t *)p;
            if (f->bFormatType != 1) { p += p[0]; continue; }
            ep->nchan = f->bNrChannels; ep->bits = f->bBitResolution;
            ep->subframe_sz = f->bSubframeSize;
            ep->alt = ca; ep->iface_num = sn;
            uint8_t nf = f->bSamFreqType;
            if (!nf) { ep->freqs[0] = 48000; ep->nfreqs = 1; }
            else {
                ep->nfreqs = nf > MAX_FREQS ? MAX_FREQS : nf;
                for (uint8_t i = 0; i < ep->nfreqs; i++)
                    ep->freqs[i] = dec_freq(&f->tSamFreq[0] + i*3);
            }
            ep->freq = ep->freqs[0];
            for (uint8_t i = 0; i < ep->nfreqs; i++)
                if (ep->freqs[i] == 48000) { ep->freq = 48000; break; }
            return 0;
        }
        p += p[0];
    }
    return -1;
}

static int audio_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                        const void *cfg, uint16_t clen)
{
    if (iface->bInterfaceClass != USB_CLASS_AUDIO || iface->bInterfaceSubClass != UAC_CTRL)
        return 0;

    audio_t *a = kmalloc(sizeof(*a));
    if (!a) return 0;
    memset(a, 0, sizeof(*a));
    a->dev = d; a->ctrl_if = iface->bInterfaceNumber;

    const uint8_t *p = (const uint8_t *)iface + iface->bLength;
    const uint8_t *end = (const uint8_t *)cfg + clen;
    uint8_t si[4], nsi = 0;
    while (p < end && p[0] >= 2 && p[1] != USB_DESC_INTERFACE) {
        if (p[1] == UAC_CS_IFACE) {
            if (p[2] == UAC_AC_HDR && p[0] >= 9) {
                uint8_t n = p[8];
                for (uint8_t i = 0; i < n && nsi < 4; i++) si[nsi++] = p[9+i];
            }
            if (p[2] == UAC_AC_FU && p[0] >= 6) a->fu_id = p[3];
        }
        p += p[0];
    }

    if (a->fu_id) {
        int16_t v;
        usb_setup_t vs;
        usb_fill_setup(&vs, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_IFACE,
                        UAC_GET_MIN, (uint16_t)(UAC_FU_VOL<<8),
                        (uint16_t)((a->fu_id<<8)|a->ctrl_if), 2);
        if (usb_control_in(d, &vs, &v, 2) == 0) a->vol_min = v;
        usb_fill_setup(&vs, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_IFACE,
                        UAC_GET_MAX, (uint16_t)(UAC_FU_VOL<<8),
                        (uint16_t)((a->fu_id<<8)|a->ctrl_if), 2);
        if (usb_control_in(d, &vs, &v, 2) == 0) a->vol_max = v;
    }

    for (uint8_t i = 0; i < nsi && a->neps < MAX_EPS; i++) {
        aep_t *ep = &a->eps[a->neps];
        if (find_alt(d, si[i], ep, (const uint8_t *)cfg, clen) < 0) continue;

        const usb_iface_desc_t *sid = usb_find_iface(cfg, clen, si[i], ep->alt);
        if (!sid) continue;
        const usb_ep_desc_t *ed = (const usb_ep_desc_t *)sid;
        for (uint8_t j = 0; j < sid->bNumEndpoints; j++) {
            ed = usb_next_ep(ed, end);
            if (!ed) break;
            if (USB_EP_TYPE(ed->bmAttributes) == USB_EP_TYPE_ISOCH) {
                ep->ep_addr = ed->bEndpointAddress;
                ep->dir_in  = USB_EP_IS_IN(ed->bEndpointAddress);
                break;
            }
        }
        if (!ep->ep_addr) continue;
        usb_set_interface(d, si[i], ep->alt);
        set_freq(d, ep->ep_addr, ep->freq);
        a->neps++;
    }

    d->driver_data = a;
    char msg[48];
    ksnprintf(msg, sizeof(msg), "UAC: %u stream ep(s) FU=%u", a->neps, a->fu_id);
    klog_ok("usb_audio", msg);
    return 1;
}

static void audio_disc(usb_device_t *d)
{
    if (d->driver_data) { kfree(d->driver_data); d->driver_data = NULL; }
}

static usb_driver_t audio_drv = {
    .name = "usb_audio", .probe = audio_probe, .disconnect = audio_disc, .poll = NULL,
};

void usb_audio_init(void)
{
    usb_driver_register(&audio_drv);
    klog_ok("usb_audio", "USB Audio class driver registered");
}
