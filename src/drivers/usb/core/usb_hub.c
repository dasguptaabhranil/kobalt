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

#define HUB_SET_FEATURE   3
#define HUB_CLEAR_FEATURE 1
#define HUB_GET_STATUS    0

#define HUB_PORT_RESET   4
#define HUB_PORT_POWER   8
#define HUB_C_PORT_CONN  16
#define HUB_C_PORT_RESET 20

#define PS_CONN     (1u << 0)
#define PS_ENABLE   (1u << 1)
#define PS_LS       (1u << 9)
#define PS_HS       (1u << 10)
#define PC_CONN     (1u << 16)
#define PC_RESET    (1u << 20)

#define HUB_MAX_PORTS 8

typedef struct {
    usb_device_t *dev;
    uint8_t num_ports;
    uint8_t intr_ep;
    uint8_t status_buf[1];
} hub_t;

static int hub_port_feat(usb_device_t *d, uint8_t port, uint16_t feat, int set)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | 0x03,
                   set ? HUB_SET_FEATURE : HUB_CLEAR_FEATURE, feat, port, 0);
    return usb_control_out(d, &s, NULL, 0);
}

static int hub_get_port_status(usb_device_t *d, uint8_t port, uint32_t *st)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_IN | USB_TYPE_CLASS | 0x03,
                   HUB_GET_STATUS, 0, port, 4);
    uint32_t v = 0;
    int r = usb_control_in(d, &s, &v, 4);
    if (r == 0 && st) *st = v;
    return r;
}

static int hub_reset_port(usb_device_t *d, uint8_t port)
{
    hub_port_feat(d, port, HUB_PORT_RESET, 1);
    for (int t = 0; t < 200; t++) {
        for (volatile int x = 0; x < 100000; x++);
        uint32_t st = 0;
        hub_get_port_status(d, port, &st);
        if (st & PC_RESET) {
            hub_port_feat(d, port, HUB_C_PORT_RESET, 0);
            return (st & PS_ENABLE) ? 0 : -1;
        }
    }
    return -1;
}

static void hub_enum_port(hub_t *h, uint8_t port)
{
    uint32_t st = 0;
    if (hub_get_port_status(h->dev, port, &st) < 0 || !(st & PS_CONN)) return;
    hub_port_feat(h->dev, port, HUB_C_PORT_CONN, 0);
    if (hub_reset_port(h->dev, port) < 0) { klog_warn("usb_hub", "port reset failed"); return; }
    hub_get_port_status(h->dev, port, &st);
    if (!(st & PS_ENABLE)) return;

    uint32_t psc;
    if (st & PS_LS)      psc = (XHCI_SPEED_LS << 10) | XHCI_PORTSC_CCS | XHCI_PORTSC_PED;
    else if (st & PS_HS) psc = (XHCI_SPEED_HS << 10) | XHCI_PORTSC_CCS | XHCI_PORTSC_PED;
    else                 psc = (XHCI_SPEED_FS << 10) | XHCI_PORTSC_CCS | XHCI_PORTSC_PED;
    usb_port_changed(h->dev->xc, port, psc);
}

static int hub_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                      const void *cfg_buf, uint16_t cfg_len)
{
    if (iface->bInterfaceClass != USB_CLASS_HUB) return 0;

    hub_t *h = kmalloc(sizeof(*h));
    if (!h) return 0;
    memset(h, 0, sizeof(*h));
    h->dev = d;

    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_DEVICE,
                   6 , 0x2900, 0, sizeof(usb_hub_desc_t));
    usb_hub_desc_t hd;
    if (usb_control_in(d, &s, &hd, sizeof(hd)) < 0 || !hd.bNbrPorts) { kfree(h); return 0; }
    h->num_ports = hd.bNbrPorts > HUB_MAX_PORTS ? HUB_MAX_PORTS : hd.bNbrPorts;

    const uint8_t *end = (const uint8_t *)cfg_buf + cfg_len;
    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep) break;
        if (USB_EP_TYPE(ep->bmAttributes) == USB_EP_TYPE_INTR && USB_EP_IS_IN(ep->bEndpointAddress)) {
            h->intr_ep = ep->bEndpointAddress;
            break;
        }
    }

    for (uint8_t p = 1; p <= h->num_ports; p++) {
        hub_port_feat(d, p, HUB_PORT_POWER, 1);
        for (volatile int x = 0; x < (int)hd.bPwrOn2PwrGood * 200000; x++);
    }
    for (uint8_t p = 1; p <= h->num_ports; p++) {
        uint32_t st = 0;
        hub_get_port_status(d, p, &st);
        if (st & PS_CONN) hub_enum_port(h, p);
    }

    d->driver_data = h;
    char msg[32];
    ksnprintf(msg, sizeof(msg), "hub: %u port(s)", h->num_ports);
    klog_ok("usb_hub", msg);
    return 1;
}

static void hub_poll(usb_device_t *d)
{
    hub_t *h = d->driver_data;
    if (!h || !h->intr_ep) return;
    if (usb_intr_in(d, h->intr_ep, h->status_buf, 1) < 0) return;
    for (uint8_t p = 1; p <= h->num_ports; p++)
        if (h->status_buf[0] & (1u << (p % 8))) hub_enum_port(h, p);
}

static void hub_disconnect(usb_device_t *d)
{
    if (d->driver_data) { kfree(d->driver_data); d->driver_data = NULL; }
}

static usb_driver_t hub_drv = {
    .name = "usb_hub", .probe = hub_probe,
    .disconnect = hub_disconnect, .poll = hub_poll,
};

void usb_hub_init(void)
{
    usb_driver_register(&hub_drv);
    klog_ok("usb_hub", "hub driver registered");
}
