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

#include "../inc/xhci.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../inc/usb.h"
#include "../inc/usb_core.h"
#include "../inc/kernel.h"
#include "../inc/kmalloc.h"
#include "../../../inc/spinlock.h"
#include "../../../inc/devfs.h"

static usb_device_t *g_devs[USB_MAX_DEVICES];
static usb_driver_t *g_drvs[USB_MAX_DRIVERS];
static int g_ndev, g_ndrv;
static spinlock_t g_lock;

extern xhci_input_ctx_t *xhci_alloc_input_ctx(void);
extern void xhci_fill_ep_ctx(xhci_input_ctx_t *ic, uint8_t ep_id,
                              uint8_t ep_type, uint16_t mps, uint8_t interval,
                              xhci_ring_t *ring, uint8_t max_burst);
extern int  xhci_alloc_xfer_ring(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id);

extern void kobalt_usb_notify(uint8_t cls, uint8_t sub, uint8_t proto,
                               uint16_t vid, uint16_t pid);

#define USB_MAX_HID     16u
#define USB_MAX_ACMTTY  8u
#define USB_MAX_DEVFS_NODES (USB_MAX_HID + USB_MAX_ACMTTY)

typedef struct {
    uint32_t major;
    uint32_t minor;
    uint8_t  inuse;
} usb_devfs_slot_t;

static usb_devfs_slot_t s_devfs_slots[USB_MAX_DEVFS_NODES];

static int alloc_hid_slot(void)
{
    for (uint32_t i = 0u; i < USB_MAX_HID; i++) {
        if (!s_devfs_slots[i].inuse) {
            s_devfs_slots[i].major = DEVFS_MAJOR_INPUT;
            s_devfs_slots[i].minor = i;
            s_devfs_slots[i].inuse = 1u;
            return (int)i;
        }
    }
    return -1;
}

static int alloc_acm_slot(void)
{
    for (uint32_t i = USB_MAX_HID; i < USB_MAX_DEVFS_NODES; i++) {
        if (!s_devfs_slots[i].inuse) {
            uint32_t minor = i - USB_MAX_HID;
            s_devfs_slots[i].major = DEVFS_MAJOR_TTYUSB;
            s_devfs_slots[i].minor = minor;
            s_devfs_slots[i].inuse = 1u;
            return (int)i;
        }
    }
    return -1;
}

static ssize_t hid_cdev_read(void *priv, void *buf, size_t n, uint64_t *pos)
{
    usb_device_t *d = (usb_device_t *)priv;
    (void)pos;
    if (!d || !d->active) return -1;

    for (int i = 0; i < d->num_eps; i++) {
        usb_endpoint_t *ep = &d->eps[i];
        if (!ep->active) continue;
        if (ep->type != USB_EP_TYPE_INTR) continue;
        if (!USB_EP_IS_IN(ep->addr)) continue;

        size_t xfer = n < ep->mps ? n : ep->mps;
        int rc = usb_intr_in(d, ep->addr, buf, (uint32_t)xfer);
        if (rc < 0) return 0;
        return (ssize_t)rc;
    }
    return -1;
}

static devfs_ops_t s_hid_ops = {
    .open  = NULL,
    .close = NULL,
    .read  = hid_cdev_read,
    .write = NULL,
    .ioctl = NULL,
    .poll  = NULL,
};

static ssize_t acm_cdev_read(void *priv, void *buf, size_t n, uint64_t *pos)
{
    usb_device_t *d = (usb_device_t *)priv;
    (void)pos;
    if (!d || !d->active) return -1;

    for (int i = 0; i < d->num_eps; i++) {
        usb_endpoint_t *ep = &d->eps[i];
        if (!ep->active) continue;
        if (ep->type != USB_EP_TYPE_BULK) continue;
        if (!USB_EP_IS_IN(ep->addr)) continue;

        size_t xfer = n < (size_t)ep->mps ? n : (size_t)ep->mps;
        int rc = usb_bulk_in(d, ep->addr, buf, (uint32_t)xfer);
        if (rc < 0) return 0;
        return (ssize_t)rc;
    }
    return -1;
}

static ssize_t acm_cdev_write(void *priv, const void *buf,
                               size_t n, uint64_t *pos)
{
    usb_device_t *d = (usb_device_t *)priv;
    (void)pos;
    if (!d || !d->active) return -1;

    for (int i = 0; i < d->num_eps; i++) {
        usb_endpoint_t *ep = &d->eps[i];
        if (!ep->active) continue;
        if (ep->type != USB_EP_TYPE_BULK) continue;
        if (USB_EP_IS_IN(ep->addr)) continue;

        size_t xfer = n < (size_t)ep->mps ? n : (size_t)ep->mps;
        int rc = usb_bulk_out(d, ep->addr, buf, (uint32_t)xfer);
        if (rc < 0) return -1;
        return (ssize_t)xfer;
    }
    return -1;
}

static devfs_ops_t s_acm_ops = {
    .open  = NULL,
    .close = NULL,
    .read  = acm_cdev_read,
    .write = acm_cdev_write,
    .ioctl = NULL,
    .poll  = NULL,
};

static void usb_devfs_register_iface(usb_device_t *d,
                                      const usb_iface_desc_t *iface)
{
    uint8_t cls = iface->bInterfaceClass;
    uint8_t sub = iface->bInterfaceSubClass;

    if (cls == 0x03u) {
        int slot = alloc_hid_slot();
        if (slot < 0) {
            klog_warn("usb", "HID devfs slot pool full");
            return;
        }
        uint32_t minor = s_devfs_slots[slot].minor;

        char devname[DEVFS_NAME_MAX];
        ksnprintf(devname, sizeof(devname), "input/event%u", (unsigned)minor);

        int rc = devfs_register_cdev(DEVFS_MAJOR_INPUT, minor, devname,
                                     DEVFS_CLASS_INPUT, &s_hid_ops, d);
        if (rc < 0) {

            s_devfs_slots[slot].inuse = 0u;
            return;
        }

        char msg[56];
        ksnprintf(msg, sizeof(msg),
                  "/dev/%s registered (slot %u, VID:PID %04x:%04x)",
                  devname, (unsigned)minor,
                  d->dev_desc.idVendor, d->dev_desc.idProduct);
        klog_ok("usb", msg);
        return;
    }

    if (cls == 0x02u && sub == 0x02u) {
        int slot = alloc_acm_slot();
        if (slot < 0) {
            klog_warn("usb", "CDC-ACM devfs slot pool full");
            return;
        }
        uint32_t minor = s_devfs_slots[slot].minor;

        char devname[DEVFS_NAME_MAX];
        ksnprintf(devname, sizeof(devname), "ttyUSB%u", (unsigned)minor);

        int rc = devfs_register_cdev(DEVFS_MAJOR_TTYUSB, minor, devname,
                                     DEVFS_CLASS_TTY, &s_acm_ops, d);
        if (rc < 0) {
            s_devfs_slots[slot].inuse = 0u;
            return;
        }

        char msg[56];
        ksnprintf(msg, sizeof(msg),
                  "/dev/%s registered (VID:PID %04x:%04x)",
                  devname,
                  d->dev_desc.idVendor, d->dev_desc.idProduct);
        klog_ok("usb", msg);
        return;
    }

}

void usb_driver_register(usb_driver_t *drv)
{
    if (g_ndrv < USB_MAX_DRIVERS) g_drvs[g_ndrv++] = drv;
}

static usb_device_t *alloc_dev(void)
{
    usb_device_t *d = kmalloc(sizeof(*d));
    if (d) memset(d, 0, sizeof(*d));
    return d;
}

static void free_dev(usb_device_t *d)
{
    if (!d) return;
    if (d->cfg_buf) { kfree(d->cfg_buf); d->cfg_buf = NULL; }
    if (d->in_ctx)  { kfree(d->in_ctx);  d->in_ctx  = NULL; }
    kfree(d);
}

usb_device_t *usb_get_device(uint8_t slot)
{
    for (int i = 0; i < g_ndev; i++)
        if (g_devs[i] && g_devs[i]->slot == slot) return g_devs[i];
    return NULL;
}

int usb_device_count(void) { return g_ndev; }

int usb_control_in(usb_device_t *d, const usb_setup_t *s, void *buf, uint16_t len)
{
    return xhci_ctrl_transfer(d->xc, d->slot, (const uint8_t *)s, buf, len, 1);
}

int usb_control_out(usb_device_t *d, const usb_setup_t *s, const void *buf, uint16_t len)
{
    return xhci_ctrl_transfer(d->xc, d->slot, (const uint8_t *)s, (void *)buf, len, 0);
}

int usb_bulk_in(usb_device_t *d, uint8_t ep_addr, void *buf, uint32_t len)
{
    return xhci_bulk_transfer(d->xc, d->slot, xhci_ep_index(ep_addr), buf, len);
}

int usb_bulk_out(usb_device_t *d, uint8_t ep_addr, const void *buf, uint32_t len)
{
    return xhci_bulk_transfer(d->xc, d->slot, xhci_ep_index(ep_addr), (void *)buf, len);
}

int usb_intr_in(usb_device_t *d, uint8_t ep_addr, void *buf, uint32_t len)
{
    return xhci_intr_transfer(d->xc, d->slot, xhci_ep_index(ep_addr), buf, len);
}

int usb_get_descriptor(usb_device_t *d, uint8_t type, uint8_t idx,
                        uint16_t lang, void *buf, uint16_t len)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                   USB_REQ_GET_DESCRIPTOR,
                   (uint16_t)((uint16_t)type << 8) | idx, lang, len);
    return usb_control_in(d, &s, buf, len);
}

int usb_set_configuration(usb_device_t *d, uint8_t val)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                   USB_REQ_SET_CONFIGURATION, val, 0, 0);
    return usb_control_out(d, &s, NULL, 0);
}

int usb_set_interface(usb_device_t *d, uint8_t iface, uint8_t alt)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_IFACE,
                   USB_REQ_SET_INTERFACE, alt, iface, 0);
    return usb_control_out(d, &s, NULL, 0);
}

int usb_clear_halt(usb_device_t *d, uint8_t ep_addr)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT,
                   USB_REQ_CLEAR_FEATURE, USB_FEAT_ENDPOINT_HALT, ep_addr, 0);
    int r = usb_control_out(d, &s, NULL, 0);
    if (r == 0) xhci_reset_ep(d->xc, d->slot, xhci_ep_index(ep_addr));
    return r;
}

int usb_set_idle(usb_device_t *d, uint8_t iface, uint8_t rid, uint8_t dur)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   0x0A, (uint16_t)((uint16_t)dur << 8) | rid, iface, 0);
    return usb_control_out(d, &s, NULL, 0);
}

int usb_set_protocol(usb_device_t *d, uint8_t iface, uint8_t proto)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   0x0B, proto, iface, 0);
    return usb_control_out(d, &s, NULL, 0);
}

int usb_get_report(usb_device_t *d, uint8_t iface, uint8_t type,
                    uint8_t id, void *buf, uint16_t len)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   0x01, (uint16_t)((uint16_t)type << 8) | id, iface, len);
    return usb_control_in(d, &s, buf, len);
}

int usb_set_report(usb_device_t *d, uint8_t iface, uint8_t type,
                    uint8_t id, const void *buf, uint16_t len)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   0x09, (uint16_t)((uint16_t)type << 8) | id, iface, len);
    return usb_control_out(d, &s, buf, len);
}

static int cfg_endpoints(usb_device_t *d, const usb_iface_desc_t *iface,
                          const void *cfg_buf, uint16_t cfg_len)
{
    xhci_input_ctx_t *ic = xhci_alloc_input_ctx();
    if (!ic) return -1;

    ic->ctrl.add_flags = 1u;
    memcpy(&ic->slot, &d->xc->dev_ctx[d->slot]->slot, sizeof(xhci_slot_ctx_t));

    uint8_t max_ep = 1;
    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, (const uint8_t *)cfg_buf + cfg_len);
        if (!ep) break;

        uint8_t  ut  = USB_EP_TYPE(ep->bmAttributes);
        uint8_t  eid = xhci_ep_index(ep->bEndpointAddress);
        uint16_t mps = ep->wMaxPacketSize & 0x7FF;
        uint8_t  iv  = ep->bInterval;
        uint8_t  xt;

        switch (ut) {
        case USB_EP_TYPE_CTRL:  xt = EP_TYPE_CTRL; break;
        case USB_EP_TYPE_BULK:
            xt = USB_EP_IS_IN(ep->bEndpointAddress) ? EP_TYPE_BULK_IN : EP_TYPE_BULK_OUT; break;
        case USB_EP_TYPE_INTR:
            xt = USB_EP_IS_IN(ep->bEndpointAddress) ? EP_TYPE_INTR_IN : EP_TYPE_INTR_OUT; break;
        case USB_EP_TYPE_ISOCH:
            xt = USB_EP_IS_IN(ep->bEndpointAddress) ? EP_TYPE_ISOCH_IN : EP_TYPE_ISOCH_OUT; break;
        default: xt = EP_TYPE_BULK_OUT; break;
        }

        if (d->speed == USB_SPEED_HIGH || d->speed >= USB_SPEED_SUPER) {
            if (iv > 0) iv--;
        } else if (ut == USB_EP_TYPE_INTR) {
            uint8_t v = 0, m = iv;
            while (m > 1) { m >>= 1; v++; }
            iv = v + 3u;
        }

        if (xhci_alloc_xfer_ring(d->xc, d->slot, eid) < 0) { kfree(ic); return -1; }
        xhci_fill_ep_ctx(ic, eid, xt, mps, iv, d->xc->xfer_rings[d->slot][eid], 0);

        if (d->num_eps < USB_MAX_ENDPOINTS) {
            usb_endpoint_t *ue = &d->eps[d->num_eps++];
            ue->addr = ep->bEndpointAddress;
            ue->type = ut;
            ue->mps  = mps;
            ue->interval = ep->bInterval;
            ue->ep_id    = eid;
            ue->active   = 1;
        }
        if (eid > max_ep) max_ep = eid;
    }

    ic->slot.dw0 = (ic->slot.dw0 & ~(0x1Fu << 27)) | ((uint32_t)max_ep << 27);
    ic->ctrl.add_flags |= 1u;
    int r = xhci_configure_ep(d->xc, d->slot, ic);
    kfree(ic);
    return r;
}

void usb_port_changed(xhci_ctrl_t *xc, uint8_t port, uint32_t portsc)
{
    if (!(portsc & XHCI_PORTSC_CCS) || !(portsc & XHCI_PORTSC_PED)) return;

    uint8_t spd;
    switch (XHCI_PORT_SPEED(portsc)) {
    case XHCI_SPEED_LS:  spd = USB_SPEED_LOW;        break;
    case XHCI_SPEED_FS:  spd = USB_SPEED_FULL;       break;
    case XHCI_SPEED_HS:  spd = USB_SPEED_HIGH;       break;
    case XHCI_SPEED_SS:  spd = USB_SPEED_SUPER;      break;
    case XHCI_SPEED_SSP: spd = USB_SPEED_SUPER_PLUS; break;
    default:             spd = USB_SPEED_FULL;        break;
    }

    uint8_t slot = 0;
    if (xhci_enable_slot(xc, &slot) < 0) {
        klog_fail("usb", "enable slot failed");
        return;
    }

    usb_device_t *d = alloc_dev();
    if (!d) { xhci_disable_slot(xc, slot); return; }
    d->xc = xc; d->slot = slot; d->speed = spd; d->port = port;

    xhci_input_ctx_t *ic = xhci_alloc_input_ctx();
    if (!ic) goto fail;
    d->in_ctx = ic;

    ic->ctrl.add_flags = 3u;
    ic->slot.dw0 = (1u << 27) | ((uint32_t)spd << 20);
    ic->slot.dw1 = (uint32_t)port << 16;

    uint16_t mps0;
    switch (spd) {
    case USB_SPEED_SUPER: case USB_SPEED_SUPER_PLUS: mps0 = 512; break;
    case USB_SPEED_LOW:  mps0 = 8;  break;
    default:             mps0 = 64; break;
    }

    xhci_fill_ep_ctx(ic, 1, EP_TYPE_CTRL, mps0, 0, xc->xfer_rings[slot][1], 0);

    if (xhci_address_device(xc, slot, ic, 1) < 0) {
        klog_fail("usb", "address device (BSR) failed");
        goto fail;
    }

    uint8_t b8[8] = {0};
    usb_setup_t gs;
    usb_fill_setup(&gs, USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                   USB_REQ_GET_DESCRIPTOR, (uint16_t)USB_DESC_DEVICE << 8, 0, 8);
    if (usb_control_in(d, &gs, b8, 8) < 0) { klog_fail("usb", "dev desc 8B failed"); goto fail; }
    mps0 = b8[7] ? b8[7] : 8;

    xhci_input_ctx_t *ec = xhci_alloc_input_ctx();
    if (!ec) goto fail;
    ec->ctrl.add_flags = 2u;
    xhci_fill_ep_ctx(ec, 1, EP_TYPE_CTRL, mps0, 0, xc->xfer_rings[slot][1], 0);
    xhci_evaluate_ctx(xc, slot, ec);
    kfree(ec);

    xhci_input_ctx_t *ic2 = xhci_alloc_input_ctx();
    if (!ic2) goto fail;
    memcpy(ic2, ic, sizeof(*ic2));
    xhci_fill_ep_ctx(ic2, 1, EP_TYPE_CTRL, mps0, 0, xc->xfer_rings[slot][1], 0);
    if (xhci_address_device(xc, slot, ic2, 0) < 0) {
        klog_fail("usb", "address device failed");
        kfree(ic2);
        goto fail;
    }
    d->addr = (uint8_t)SLOT_CTX_DEV_ADDR(xc->dev_ctx[slot]->slot.dw3);
    kfree(ic2);

    if (usb_get_descriptor(d, USB_DESC_DEVICE, 0, 0,
                            &d->dev_desc, sizeof(d->dev_desc)) < 0) {
        klog_fail("usb", "get dev desc failed");
        goto fail;
    }

    usb_cfg_desc_t ch;
    if (usb_get_descriptor(d, USB_DESC_CONFIGURATION, 0, 0, &ch, sizeof(ch)) < 0)
        goto fail;
    d->cfg_len = ch.wTotalLength > USB_MAX_CFG_BUFSZ ? USB_MAX_CFG_BUFSZ : ch.wTotalLength;
    d->cfg_buf = kmalloc(d->cfg_len);
    if (!d->cfg_buf) goto fail;
    if (usb_get_descriptor(d, USB_DESC_CONFIGURATION, 0, 0, d->cfg_buf, d->cfg_len) < 0)
        goto fail;

    d->config = ch.bConfigurationValue;
    if (usb_set_configuration(d, d->config) < 0) goto fail;

    {
        char msg[64];
        ksnprintf(msg, sizeof(msg), "USB %04x:%04x slot=%u spd=%u addr=%u",
                  d->dev_desc.idVendor, d->dev_desc.idProduct, slot, spd, d->addr);
        klog_ok("usb", msg);
    }

    spin_lock(&g_lock);
    if (g_ndev < USB_MAX_DEVICES) { g_devs[g_ndev++] = d; d->active = 1; }
    spin_unlock(&g_lock);

    {
        const uint8_t *p   = d->cfg_buf;
        const uint8_t *end = p + d->cfg_len;
        if (p < end && p[0] >= 2) p += p[0];

        while (p < end && p[0] >= 2) {
            if (p[1] == USB_DESC_INTERFACE) {
                const usb_iface_desc_t *iface = (const usb_iface_desc_t *)p;

                kobalt_usb_notify(iface->bInterfaceClass,
                                  iface->bInterfaceSubClass,
                                  iface->bInterfaceProtocol,
                                  d->dev_desc.idVendor,
                                  d->dev_desc.idProduct);

                cfg_endpoints(d, iface, d->cfg_buf, d->cfg_len);

                for (int i = 0; i < g_ndrv; i++) {
                    if (g_drvs[i]->probe &&
                        g_drvs[i]->probe(d, iface, d->cfg_buf, d->cfg_len)) {
                        char m[48];
                        ksnprintf(m, sizeof(m), "driver '%s' bound iface %u",
                                  g_drvs[i]->name, iface->bInterfaceNumber);
                        klog_ok("usb", m);
                        break;
                    }
                }

                usb_devfs_register_iface(d, iface);
            }
            p += p[0];
        }
    }
    return;

fail:
    free_dev(d);
    xhci_disable_slot(xc, slot);
}

void usb_core_poll(void)
{
    for (int i = 0; i < xhci_ctrl_count(); i++) xhci_poll(xhci_get_ctrl(i));

    spin_lock(&g_lock);
    for (int i = 0; i < g_ndev; i++) {
        usb_device_t *d = g_devs[i];
        if (!d || !d->active) continue;
        for (int j = 0; j < g_ndrv; j++)
            if (g_drvs[j]->poll) g_drvs[j]->poll(d);
    }
    spin_unlock(&g_lock);
}

int usb_core_init(void)
{
    g_lock = SPINLOCK_INIT;
    memset(g_devs, 0, sizeof(g_devs));
    memset(g_drvs, 0, sizeof(g_drvs));
    memset(s_devfs_slots, 0, sizeof(s_devfs_slots));
    g_ndev = g_ndrv = 0;

    int n = xhci_init();
    if (n < 0) { klog_info("usb", "no USB controllers found"); return -1; }
    char msg[32];
    ksnprintf(msg, sizeof(msg), "%d xHCI controller(s) ready", n);
    klog_ok("usb", msg);
    return 0;
}
