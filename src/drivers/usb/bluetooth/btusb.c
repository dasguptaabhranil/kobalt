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
#include "../inc/tykid.h"

extern int  hci_init(int(*)(void*,const void*,uint16_t),
                      int(*)(void*,const void*,uint16_t), void *priv);
extern void hci_recv_event(const uint8_t *data, uint16_t len);
extern tykid_gate_ctx_t *tykid_kobalt_get_ctx(void);
extern int               tykid_kobalt_builtin_approved(tykid_gate_ctx_t *ctx,
                                                        const char *name);

#define BT_CLASS    0xE0
#define BT_SUBCLASS 0x01
#define BT_PROTO    0x01

#define EVT_BUF 260
#define ACL_BUF 1028

typedef struct {
    usb_device_t *dev;
    uint8_t  iface;
    uint8_t  intr_ep, bulk_in, bulk_out;
    uint8_t  evt[EVT_BUF];
    uint8_t  acl[ACL_BUF];
} btusb_t;

static int bt_send_cmd(void *priv, const void *buf, uint16_t len)
{
    btusb_t *bt = priv;
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_DEVICE,
                   0x00, 0, 0, len);
    return usb_control_out(bt->dev, &s, buf, len);
}

static int bt_send_acl(void *priv, const void *buf, uint16_t len)
{
    btusb_t *bt = priv;
    return usb_bulk_out(bt->dev, bt->bulk_out, buf, len);
}

static int btusb_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                        const void *cfg, uint16_t clen)
{
    if (iface->bInterfaceClass    != BT_CLASS    ||
        iface->bInterfaceSubClass != BT_SUBCLASS ||
        iface->bInterfaceProtocol != BT_PROTO) return 0;

    btusb_t *bt = kmalloc(sizeof(*bt));
    if (!bt) return 0;
    memset(bt, 0, sizeof(*bt));
    bt->dev = d; bt->iface = iface->bInterfaceNumber;

    const uint8_t *end = (const uint8_t *)cfg + clen;
    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep) break;
        uint8_t t = USB_EP_TYPE(ep->bmAttributes);
        if (t == USB_EP_TYPE_INTR && USB_EP_IS_IN(ep->bEndpointAddress))
            bt->intr_ep  = ep->bEndpointAddress;
        if (t == USB_EP_TYPE_BULK && USB_EP_IS_IN(ep->bEndpointAddress))
            bt->bulk_in  = ep->bEndpointAddress;
        if (t == USB_EP_TYPE_BULK && !USB_EP_IS_IN(ep->bEndpointAddress))
            bt->bulk_out = ep->bEndpointAddress;
    }
    if (!bt->intr_ep || !bt->bulk_in || !bt->bulk_out) { kfree(bt); return 0; }

    d->driver_data = bt;

    if (hci_init(bt_send_cmd, bt_send_acl, bt) < 0) {
        klog_fail("btusb", "HCI init failed");
        kfree(bt);
        d->driver_data = NULL;
        return 0;
    }

    klog_ok("btusb", "Bluetooth adapter ready");
    return 1;
}

static void btusb_poll(usb_device_t *d)
{
    btusb_t *bt = d->driver_data;
    if (!bt) return;
    int n = usb_intr_in(d, bt->intr_ep, bt->evt, EVT_BUF);
    if (n > 0) hci_recv_event(bt->evt, (uint16_t)n);
    usb_bulk_in(d, bt->bulk_in, bt->acl, ACL_BUF);
}

static void btusb_disc(usb_device_t *d)
{
    if (d->driver_data) { kfree(d->driver_data); d->driver_data = NULL; }
}

static usb_driver_t btusb_drv = {
    .name = "btusb", .probe = btusb_probe, .disconnect = btusb_disc, .poll = btusb_poll,
};

void btusb_init(void)
{
    tykid_gate_ctx_t *ctx = tykid_kobalt_get_ctx();
    if (!tykid_kobalt_builtin_approved(ctx, "btusb")) {
        klog_info("btusb", "skipped by TYKID (no BT hardware)");
        return;
    }
    usb_driver_register(&btusb_drv);
    klog_ok("btusb", "USB Bluetooth driver registered");
}
