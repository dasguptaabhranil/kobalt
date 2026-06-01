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

#define CP210X_VID      0x10C4
#define CP_IFC_ENABLE   0x00
#define CP_SET_LC       0x03
#define CP_SET_MHS      0x07
#define CP_SET_BAUD     0x1E
#define CP_MHS_DTR      0x0101
#define CP_MHS_RTS      0x0202

#define CP_RX   256
#define CP_RING 512

typedef struct {
    usb_device_t *dev;
    uint8_t  iface, bulk_in, bulk_out;
    uint32_t baud;
    char     name[16];
    uint8_t  rx[CP_RX];
    uint8_t  ring[CP_RING];
    volatile uint16_t rhead, rtail;
} cp_t;

static int g_ncp;

static int cp_out(cp_t *c, uint8_t req, uint16_t val, const void *buf, uint16_t len)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_IFACE,
                   req, val, c->iface, len);
    return usb_control_out(c->dev, &s, buf, len);
}

int usb_cp210x_set_baud(usb_device_t *d, uint32_t baud)
{
    cp_t *c = d->driver_data;
    if (!c) return -1;
    c->baud = baud;
    return cp_out(c, CP_SET_BAUD, 0, &baud, 4);
}

int usb_cp210x_set_line(usb_device_t *d, uint8_t bits, uint8_t par, uint8_t stop)
{
    cp_t *c = d->driver_data;
    if (!c) return -1;
    uint16_t lc = (uint16_t)(bits << 8);
    if (par == 1)      lc |= 0x0010;
    else if (par == 2) lc |= 0x0020;
    if (stop == 2)     lc |= 0x0200;
    return cp_out(c, CP_SET_LC, lc, NULL, 0);
}

int usb_cp210x_write(usb_device_t *d, const uint8_t *buf, uint32_t len)
{
    cp_t *c = d->driver_data;
    if (!c || !c->bulk_out) return -1;
    return usb_bulk_out(d, c->bulk_out, buf, len);
}

int usb_cp210x_getc(usb_device_t *d)
{
    cp_t *c = d->driver_data;
    if (!c || c->rhead == c->rtail) return -1;
    uint8_t b = c->ring[c->rhead];
    c->rhead = (c->rhead + 1u) % CP_RING;
    return b;
}

static int cp_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                     const void *cfg, uint16_t cfg_len)
{
    if (iface->bInterfaceClass != USB_CLASS_VENDOR) return 0;
    if (d->dev_desc.idVendor != CP210X_VID) return 0;

    cp_t *c = kmalloc(sizeof(*c));
    if (!c) return 0;
    memset(c, 0, sizeof(*c));
    c->dev = d; c->iface = iface->bInterfaceNumber; c->baud = 115200;

    const uint8_t *end = (const uint8_t *)cfg + cfg_len;
    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep || USB_EP_TYPE(ep->bmAttributes) != USB_EP_TYPE_BULK) continue;
        if (USB_EP_IS_IN(ep->bEndpointAddress)) c->bulk_in  = ep->bEndpointAddress;
        else                                     c->bulk_out = ep->bEndpointAddress;
    }
    if (!c->bulk_in || !c->bulk_out) { kfree(c); return 0; }

    cp_out(c, CP_IFC_ENABLE, 1, NULL, 0);
    usb_cp210x_set_baud(d, 115200);
    usb_cp210x_set_line(d, 8, 0, 1);
    cp_out(c, CP_SET_MHS, CP_MHS_DTR, NULL, 0);
    cp_out(c, CP_SET_MHS, CP_MHS_RTS, NULL, 0);

    ksnprintf(c->name, sizeof(c->name), "ttyCPL%d", g_ncp++);
    d->driver_data = c;

    char msg[40];
    ksnprintf(msg, sizeof(msg), "CP210x %04x: %s", d->dev_desc.idProduct, c->name);
    klog_ok("usb_cp210x", msg);
    return 1;
}

static void cp_poll(usb_device_t *d)
{
    cp_t *c = d->driver_data;
    if (!c || !c->bulk_in) return;
    int n = usb_bulk_in(d, c->bulk_in, c->rx, CP_RX);
    if (n <= 0) return;
    for (int i = 0; i < n; i++) {
        uint16_t nx = (c->rtail + 1u) % CP_RING;
        if (nx != c->rhead) { c->ring[c->rtail] = c->rx[i]; c->rtail = nx; }
        uart_putc((char)c->rx[i]);
    }
}

static void cp_disc(usb_device_t *d)
{
    cp_t *c = d->driver_data;
    if (!c) return;
    cp_out(c, CP_IFC_ENABLE, 0, NULL, 0);
    kfree(c);
    d->driver_data = NULL;
}

static usb_driver_t cp_drv = {
    .name = "usb_cp210x", .probe = cp_probe, .disconnect = cp_disc, .poll = cp_poll,
};

void usb_cp210x_init(void)
{
    usb_driver_register(&cp_drv);
    klog_ok("usb_cp210x", "CP210x serial driver registered");
}
