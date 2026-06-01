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

#define FTDI_VID    0x0403
#define FTDI_RESET  0x00
#define FTDI_MODEM  0x01
#define FTDI_BAUD   0x03
#define FTDI_DATA   0x04
#define FTDI_LAT    0x09
#define FTDI_DTR_H  0x0101
#define FTDI_RTS_H  0x0202

static const uint8_t frac_enc[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };

static uint16_t ftdi_div(uint32_t baud, int h)
{
    uint32_t base = h ? 12000000u : 3000000u;
    uint32_t d8   = (base * 8u) / baud;
    uint32_t id   = d8 / 8u, fr = d8 % 8u;
    if (!id) id = 1;
    if (id > 0x3FFF) id = 0x3FFF;
    return (uint16_t)(id | ((uint16_t)frac_enc[fr] << 14));
}

#define FTDI_RX 256
#define FTDI_STATUS_BYTES 2
#define FTDI_RING 512

typedef struct {
    usb_device_t *dev;
    uint8_t  iface, bulk_in, bulk_out;
    uint8_t  idx;
    uint32_t baud;
    int      h_series;
    char     name[16];
    uint8_t  rx[FTDI_RX];
    uint8_t  ring[FTDI_RING];
    volatile uint16_t rhead, rtail;
} ftdi_t;

static int g_nftdi;

static int ftdi_out(ftdi_t *f, uint8_t req, uint16_t val)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                   req, val, f->idx, 0);
    return usb_control_out(f->dev, &s, NULL, 0);
}

int usb_ftdi_set_baud(usb_device_t *d, uint32_t baud)
{
    ftdi_t *f = d->driver_data;
    if (!f) return -1;
    f->baud = baud;
    return ftdi_out(f, FTDI_BAUD, ftdi_div(baud, f->h_series));
}

int usb_ftdi_set_line(usb_device_t *d, uint8_t bits, uint8_t parity, uint8_t stop)
{
    ftdi_t *f = d->driver_data;
    if (!f) return -1;
    uint16_t v = bits & 0xFF;
    if (parity == 1)      v |= 1 << 8;
    else if (parity == 2) v |= 2 << 8;
    if (stop == 2)        v |= 2 << 11;
    return ftdi_out(f, FTDI_DATA, v);
}

int usb_ftdi_write(usb_device_t *d, const uint8_t *buf, uint32_t len)
{
    ftdi_t *f = d->driver_data;
    if (!f || !f->bulk_out) return -1;
    return usb_bulk_out(d, f->bulk_out, buf, len);
}

int usb_ftdi_getc(usb_device_t *d)
{
    ftdi_t *f = d->driver_data;
    if (!f || f->rhead == f->rtail) return -1;
    uint8_t c = f->ring[f->rhead];
    f->rhead = (f->rhead + 1u) % FTDI_RING;
    return c;
}

static int ftdi_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                       const void *cfg, uint16_t cfg_len)
{
    (void)cfg_len;
    if (iface->bInterfaceClass != USB_CLASS_VENDOR) return 0;
    if (d->dev_desc.idVendor != FTDI_VID) return 0;
    uint16_t pid = d->dev_desc.idProduct;
    if (pid != 0x6001 && pid != 0x6010 && pid != 0x6011 &&
        pid != 0x6014 && pid != 0x6015) return 0;

    ftdi_t *f = kmalloc(sizeof(*f));
    if (!f) return 0;
    memset(f, 0, sizeof(*f));
    f->dev = d; f->iface = iface->bInterfaceNumber;
    f->idx = (uint8_t)(iface->bInterfaceNumber + 1);
    f->h_series = (pid == 0x6014 || pid == 0x6010 || pid == 0x6011);
    f->baud = 115200;

    const uint8_t *end = (const uint8_t *)cfg + cfg_len;
    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep || USB_EP_TYPE(ep->bmAttributes) != USB_EP_TYPE_BULK) continue;
        if (USB_EP_IS_IN(ep->bEndpointAddress)) f->bulk_in  = ep->bEndpointAddress;
        else                                     f->bulk_out = ep->bEndpointAddress;
    }
    if (!f->bulk_in || !f->bulk_out) { kfree(f); return 0; }

    ftdi_out(f, FTDI_RESET, 0);
    usb_ftdi_set_baud(d, 115200);
    usb_ftdi_set_line(d, 8, 0, 1);
    ftdi_out(f, FTDI_MODEM, FTDI_DTR_H);
    ftdi_out(f, FTDI_MODEM, FTDI_RTS_H);
    ftdi_out(f, FTDI_LAT,   1);

    ksnprintf(f->name, sizeof(f->name), "ttyFTDI%d", g_nftdi++);
    d->driver_data = f;

    char msg[40];
    ksnprintf(msg, sizeof(msg), "FTDI %04x: %s", pid, f->name);
    klog_ok("usb_ftdi", msg);
    return 1;
}

static void ftdi_poll(usb_device_t *d)
{
    ftdi_t *f = d->driver_data;
    if (!f || !f->bulk_in) return;
    int n = usb_bulk_in(d, f->bulk_in, f->rx, FTDI_RX);
    if (n <= FTDI_STATUS_BYTES) return;

    for (int i = FTDI_STATUS_BYTES; i < n; i++) {
        uint16_t nx = (f->rtail + 1u) % FTDI_RING;
        if (nx != f->rhead) { f->ring[f->rtail] = f->rx[i]; f->rtail = nx; }
        uart_putc((char)f->rx[i]);
    }
}

static void ftdi_disc(usb_device_t *d)
{
    if (d->driver_data) { kfree(d->driver_data); d->driver_data = NULL; }
}

static usb_driver_t ftdi_drv = {
    .name = "usb_ftdi", .probe = ftdi_probe, .disconnect = ftdi_disc, .poll = ftdi_poll,
};

void usb_ftdi_init(void)
{
    usb_driver_register(&ftdi_drv);
    klog_ok("usb_ftdi", "FTDI serial driver registered");
}
