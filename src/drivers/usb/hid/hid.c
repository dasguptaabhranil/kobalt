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

#define HID_DESC_HID    0x21
#define HID_DESC_REPORT 0x22

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bDescriptorType2;
    uint16_t wDescriptorLength;
} hid_desc_t;

typedef struct {
    uint16_t usage_page;
    uint16_t usage;
    int  (*init)(usb_device_t *d, uint8_t iface, uint8_t intr_ep,
                 const uint8_t *rdesc, uint16_t rlen);
    void (*report)(usb_device_t *d, const uint8_t *buf, uint8_t len);
    void (*deinit)(usb_device_t *d);
} hid_subdrv_t;

#define MAX_SUBDRVS 8
static hid_subdrv_t g_sub[MAX_SUBDRVS];
static int g_nsub;

#define HID_MAX_RPT 64

typedef struct {
    usb_device_t *dev;
    uint8_t       iface;
    uint8_t       intr_ep;
    uint8_t       rpt[HID_MAX_RPT];
    uint8_t       rpt_len;
    uint16_t      up, usage;
    hid_subdrv_t *sub;
} hid_dev_t;

static void parse_usage(const uint8_t *d, uint16_t len, uint16_t *pg, uint16_t *us)
{
    *pg = *us = 0;
    int gp = 0, gu = 0;
    uint16_t i = 0;
    while (i < len) {
        uint8_t b = d[i], tag = (b >> 4) & 0xF, type = (b >> 2) & 0x3;
        uint8_t sz = b & 0x3;
        if (sz == 3) sz = 4;
        i++;
        uint32_t v = 0;
        for (uint8_t j = 0; j < sz && i < len; j++, i++) v |= (uint32_t)d[i] << (j * 8);
        if (type == 1 && tag == 0) { *pg = (uint16_t)v; gp = 1; }
        if (type == 0 && tag == 0 && !gu) { *us = (uint16_t)v; gu = 1; }
        if (gp && gu) break;
    }
}

void hid_register_subdriver(uint16_t pg, uint16_t us,
    int  (*init)(usb_device_t*, uint8_t, uint8_t, const uint8_t*, uint16_t),
    void (*rpt)(usb_device_t*, const uint8_t*, uint8_t),
    void (*deinit)(usb_device_t*))
{
    if (g_nsub >= MAX_SUBDRVS) return;
    g_sub[g_nsub].usage_page = pg;
    g_sub[g_nsub].usage      = us;
    g_sub[g_nsub].init       = init;
    g_sub[g_nsub].report     = rpt;
    g_sub[g_nsub].deinit     = deinit;
    g_nsub++;
}

static int hid_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                      const void *cfg_buf, uint16_t cfg_len)
{
    if (iface->bInterfaceClass != USB_CLASS_HID) return 0;

    const uint8_t *p = (const uint8_t *)iface + iface->bLength;
    const uint8_t *end = (const uint8_t *)cfg_buf + cfg_len;
    hid_desc_t hd;
    int found = 0;
    while (p < end && p[0] >= 2) {
        if (p[1] == HID_DESC_HID) { memcpy(&hd, p, p[0] < sizeof(hd) ? p[0] : sizeof(hd)); found = 1; break; }
        if (p[1] == USB_DESC_INTERFACE) break;
        p += p[0];
    }
    if (!found) return 0;

    uint8_t intr_ep = 0;
    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep) break;
        if (USB_EP_TYPE(ep->bmAttributes) == USB_EP_TYPE_INTR && USB_EP_IS_IN(ep->bEndpointAddress)) {
            intr_ep = ep->bEndpointAddress; break;
        }
    }
    if (!intr_ep) return 0;

    uint16_t rlen = hd.wDescriptorLength;
    if (!rlen || rlen > 512) rlen = 64;
    uint8_t *rdesc = kmalloc(rlen);
    if (!rdesc) return 0;

    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_IFACE,
                   USB_REQ_GET_DESCRIPTOR, (uint16_t)HID_DESC_REPORT << 8,
                   iface->bInterfaceNumber, rlen);
    if (usb_control_in(d, &s, rdesc, rlen) < 0) { kfree(rdesc); return 0; }

    uint16_t up, usage;
    parse_usage(rdesc, rlen, &up, &usage);
    if (!up) {
        if (iface->bInterfaceProtocol == USB_HID_PROTO_KEYBOARD) { up = 1; usage = 6; }
        else if (iface->bInterfaceProtocol == USB_HID_PROTO_MOUSE) { up = 1; usage = 2; }
    }

    usb_set_idle(d, iface->bInterfaceNumber, 0, 0);

    hid_dev_t *hd2 = kmalloc(sizeof(*hd2));
    if (!hd2) { kfree(rdesc); return 0; }
    memset(hd2, 0, sizeof(*hd2));
    hd2->dev = d;
    hd2->iface = iface->bInterfaceNumber;
    hd2->intr_ep = intr_ep;
    hd2->rpt_len = ep ? ((ep->wMaxPacketSize & 0x7FF) > HID_MAX_RPT ? HID_MAX_RPT : (uint8_t)(ep->wMaxPacketSize & 0x7FF)) : 8;
    hd2->up = up; hd2->usage = usage;

    for (int i = 0; i < g_nsub; i++) {
        if (g_sub[i].usage_page == up && g_sub[i].usage == usage) {
            hd2->sub = &g_sub[i];
            if (g_sub[i].init) g_sub[i].init(d, hd2->iface, intr_ep, rdesc, rlen);
            break;
        }
    }

    kfree(rdesc);
    d->driver_data = hd2;

    char msg[48];
    ksnprintf(msg, sizeof(msg), "HID page=%04x usage=%04x ep=%02x", up, usage, intr_ep);
    klog_ok("hid", msg);
    return 1;
}

static void hid_poll(usb_device_t *d)
{
    hid_dev_t *h = d->driver_data;
    if (!h || !h->intr_ep) return;
    if (usb_intr_in(d, h->intr_ep, h->rpt, h->rpt_len) < 0) return;
    if (h->sub && h->sub->report) h->sub->report(d, h->rpt, h->rpt_len);
}

static void hid_disconnect(usb_device_t *d)
{
    hid_dev_t *h = d->driver_data;
    if (!h) return;
    if (h->sub && h->sub->deinit) h->sub->deinit(d);
    kfree(h);
    d->driver_data = NULL;
}

static usb_driver_t hid_drv = {
    .name = "hid", .probe = hid_probe,
    .disconnect = hid_disconnect, .poll = hid_poll,
};

void hid_init(void)
{
    usb_driver_register(&hid_drv);
    klog_ok("hid", "HID class driver registered");
}
