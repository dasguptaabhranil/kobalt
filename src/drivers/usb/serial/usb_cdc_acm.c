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

#define ACM_SET_LINE  0x20
#define ACM_SET_CTRL  0x22
#define ACM_DTR       (1u<<0)
#define ACM_RTS       (1u<<1)
#define CDC_UNION     0x06

typedef struct __attribute__((packed)) {
    uint32_t dwDTERate;
    uint8_t  bCharFormat, bParityType, bDataBits;
} line_coding_t;

#define ACM_RX 256
#define ACM_RING 512

typedef struct {
    usb_device_t  *dev;
    uint8_t        ctrl_if, data_if;
    uint8_t        intr_ep, bulk_in, bulk_out;
    line_coding_t  lc;
    char           name[16];
    uint8_t        rx[ACM_RX];

    uint8_t        ring[ACM_RING];
    volatile uint16_t rhead, rtail;
} acm_t;

static int g_nacm;

static int set_line(acm_t *a)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   ACM_SET_LINE, 0, a->ctrl_if, sizeof(a->lc));
    return usb_control_out(a->dev, &s, &a->lc, sizeof(a->lc));
}

static int set_ctrl(acm_t *a, uint16_t state)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   ACM_SET_CTRL, state, a->ctrl_if, 0);
    return usb_control_out(a->dev, &s, NULL, 0);
}

int usb_acm_set_baud(usb_device_t *d, uint32_t baud)
{
    acm_t *a = d->driver_data;
    if (!a) return -1;
    a->lc.dwDTERate = baud;
    return set_line(a);
}

int usb_acm_write(usb_device_t *d, const uint8_t *buf, uint32_t len)
{
    acm_t *a = d->driver_data;
    if (!a || !a->bulk_out) return -1;
    return usb_bulk_out(d, a->bulk_out, buf, len);
}

int usb_acm_getc(usb_device_t *d)
{
    acm_t *a = d->driver_data;
    if (!a || a->rhead == a->rtail) return -1;
    uint8_t c = a->ring[a->rhead];
    a->rhead = (a->rhead + 1u) % ACM_RING;
    return c;
}

static int acm_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                      const void *cfg, uint16_t cfg_len)
{
    if (iface->bInterfaceClass    != USB_CLASS_CDC ||
        iface->bInterfaceSubClass != USB_CDC_SUBCLASS_ACM) return 0;

    acm_t *a = kmalloc(sizeof(*a));
    if (!a) return 0;
    memset(a, 0, sizeof(*a));
    a->dev = d; a->ctrl_if = iface->bInterfaceNumber;
    a->lc.dwDTERate = 115200; a->lc.bDataBits = 8;

    const uint8_t *p = (const uint8_t *)iface + iface->bLength;
    const uint8_t *end = (const uint8_t *)cfg + cfg_len;
    while (p < end && p[0] >= 2 && p[1] != USB_DESC_INTERFACE) {
        if (p[1] == 0x24 && p[2] == CDC_UNION && p[0] >= 5) a->data_if = p[4];
        p += p[0];
    }

    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep) break;
        if (USB_EP_TYPE(ep->bmAttributes) == USB_EP_TYPE_INTR &&
            USB_EP_IS_IN(ep->bEndpointAddress))
            a->intr_ep = ep->bEndpointAddress;
    }

    const usb_iface_desc_t *di = usb_find_iface(cfg, cfg_len, a->data_if, 0);
    if (di) {
        const usb_ep_desc_t *de = (const usb_ep_desc_t *)di;
        for (uint8_t i = 0; i < di->bNumEndpoints; i++) {
            de = usb_next_ep(de, end);
            if (!de || USB_EP_TYPE(de->bmAttributes) != USB_EP_TYPE_BULK) continue;
            if (USB_EP_IS_IN(de->bEndpointAddress)) a->bulk_in  = de->bEndpointAddress;
            else                                     a->bulk_out = de->bEndpointAddress;
        }
    }
    if (!a->bulk_in || !a->bulk_out) { kfree(a); return 0; }

    set_line(a);
    set_ctrl(a, ACM_DTR | ACM_RTS);

    ksnprintf(a->name, sizeof(a->name), "ttyUSB%d", g_nacm++);
    d->driver_data = a;

    char msg[40];
    ksnprintf(msg, sizeof(msg), "CDC ACM: %s at 115200", a->name);
    klog_ok("usb_cdc_acm", msg);
    return 1;
}

static void acm_poll(usb_device_t *d)
{
    acm_t *a = d->driver_data;
    if (!a || !a->bulk_in) return;
    int n = usb_bulk_in(d, a->bulk_in, a->rx, ACM_RX);
    if (n <= 0) return;

    for (int i = 0; i < n; i++) {
        uint16_t nx = (a->rtail + 1u) % ACM_RING;
        if (nx != a->rhead) { a->ring[a->rtail] = a->rx[i]; a->rtail = nx; }
        uart_putc((char)a->rx[i]);
    }
}

static void acm_disc(usb_device_t *d)
{
    if (d->driver_data) { kfree(d->driver_data); d->driver_data = NULL; }
}

static usb_driver_t acm_drv = {
    .name = "usb_cdc_acm", .probe = acm_probe, .disconnect = acm_disc, .poll = acm_poll,
};

void usb_cdc_acm_init(void)
{
    usb_driver_register(&acm_drv);
    klog_ok("usb_cdc_acm", "CDC ACM serial driver registered");
}
