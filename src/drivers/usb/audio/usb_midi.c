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

#define MIDI_STREAM 0x03

#define CIN_NOTE_OFF  0x08
#define CIN_NOTE_ON   0x09
#define CIN_POLY      0x0A
#define CIN_CC        0x0B
#define CIN_PROG      0x0C
#define CIN_PRESSURE  0x0D
#define CIN_PITCH     0x0E
#define CIN_1BYTE     0x0F

#define MIDI_RING 256
#define MIDI_RX   64

typedef struct {
    uint8_t cn, cin;
    uint8_t b[3];
} midi_evt_t;

static midi_evt_t g_ring[MIDI_RING];
static volatile uint16_t g_rh, g_rt;

static void ring_push(const midi_evt_t *e)
{
    uint16_t nx = (uint16_t)((g_rt + 1u) % MIDI_RING);
    if (nx != g_rh) { g_ring[g_rt] = *e; g_rt = nx; }
}

int usb_midi_read_event(midi_evt_t *out)
{
    if (g_rh == g_rt) return 0;
    *out = g_ring[g_rh];
    g_rh = (uint16_t)((g_rh + 1u) % MIDI_RING);
    return 1;
}

typedef struct {
    usb_device_t *dev;
    uint8_t bulk_in, bulk_out;
    uint8_t rx[MIDI_RX];
} midi_t;

static void decode_pkt(const uint8_t *p)
{
    midi_evt_t e;
    e.cn = (p[0] >> 4) & 0x0F;
    e.cin = p[0] & 0x0F;
    e.b[0] = p[1]; e.b[1] = p[2]; e.b[2] = p[3];
    ring_push(&e);
}

int usb_midi_send(usb_device_t *d, uint8_t cable, const uint8_t *msg, uint8_t len)
{
    midi_t *m = d->driver_data;
    if (!m || !m->bulk_out || !len || len > 3) return -1;
    uint8_t cin;
    switch (msg[0] & 0xF0) {
    case 0x80: cin = CIN_NOTE_OFF; break;
    case 0x90: cin = CIN_NOTE_ON;  break;
    case 0xA0: cin = CIN_POLY;     break;
    case 0xB0: cin = CIN_CC;       break;
    case 0xC0: cin = CIN_PROG;     break;
    case 0xD0: cin = CIN_PRESSURE; break;
    case 0xE0: cin = CIN_PITCH;    break;
    case 0xF0: cin = CIN_1BYTE;    break;
    default:   cin = len & 0x0F;   break;
    }
    uint8_t pkt[4] = {
        (uint8_t)(((cable & 0x0F) << 4) | (cin & 0x0F)),
        len > 0 ? msg[0] : 0,
        len > 1 ? msg[1] : 0,
        len > 2 ? msg[2] : 0,
    };
    return usb_bulk_out(d, m->bulk_out, pkt, 4);
}

static int midi_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                       const void *cfg, uint16_t clen)
{
    if (iface->bInterfaceClass != USB_CLASS_AUDIO ||
        iface->bInterfaceSubClass != MIDI_STREAM) return 0;

    midi_t *m = kmalloc(sizeof(*m));
    if (!m) return 0;
    memset(m, 0, sizeof(*m));
    m->dev = d;

    const uint8_t *end = (const uint8_t *)cfg + clen;
    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep || USB_EP_TYPE(ep->bmAttributes) != USB_EP_TYPE_BULK) continue;
        if (USB_EP_IS_IN(ep->bEndpointAddress)) m->bulk_in  = ep->bEndpointAddress;
        else                                     m->bulk_out = ep->bEndpointAddress;
    }
    if (!m->bulk_in) { kfree(m); return 0; }

    d->driver_data = m;
    klog_ok("usb_midi", "USB MIDI streaming ready");
    return 1;
}

static void midi_poll(usb_device_t *d)
{
    midi_t *m = d->driver_data;
    if (!m || !m->bulk_in) return;
    int n = usb_bulk_in(d, m->bulk_in, m->rx, sizeof(m->rx));
    if (n <= 0) return;
    for (int off = 0; off + 3 < n; off += 4)
        if (m->rx[off]) decode_pkt(&m->rx[off]);
}

static void midi_disc(usb_device_t *d)
{
    if (d->driver_data) { kfree(d->driver_data); d->driver_data = NULL; }
}

static usb_driver_t midi_drv = {
    .name = "usb_midi", .probe = midi_probe, .disconnect = midi_disc, .poll = midi_poll,
};

void usb_midi_init(void)
{
    usb_driver_register(&midi_drv);
    klog_ok("usb_midi", "USB MIDI driver registered");
}
