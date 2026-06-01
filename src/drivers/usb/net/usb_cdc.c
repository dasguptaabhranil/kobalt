/* Copyright (c) 2026  Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../inc/usb.h"
#include "../inc/usb_core.h"
#include "../inc/kernel.h"
#include "../inc/kmalloc.h"
#include <lwip/netif.h>
#include <lwip/etharp.h>
#include <lwip/pbuf.h>
#include <lwip/ip4_addr.h>
#include <netif/ethernet.h>

#define CDC_ECM_SUBCLASS  0x06
#define CDC_NCM_SUBCLASS  0x0D
#define CDC_CS_UNION      0x06
#define CDC_CS_ETHERNET   0x0F
#define CDC_SET_FILTER    0x43
#define FILTER_DIRECTED   (1u << 1)
#define FILTER_BROADCAST  (1u << 1)

typedef struct __attribute__((packed)) {
    uint8_t  bLength, bDescriptorType, bDescriptorSubtype;
    uint8_t  iMACAddress;
    uint32_t bmEthernetStatistics;
    uint16_t wMaxSegmentSize;
    uint16_t wNumberMCFilters;
    uint8_t  bNumberPowerFilters;
} ecm_desc_t;

#define CDC_RX 1600

typedef struct {
    usb_device_t *dev;
    uint8_t  ctrl_if, data_if;
    uint8_t  intr_ep, bulk_in, bulk_out;
    uint8_t  mac[6];
    uint16_t mtu;
    struct netif nif;
    uint8_t  rx[CDC_RX];
    int      up;
} cdc_t;

static int g_cdc_n;

static int parse_mac(const char *s, uint8_t *mac)
{
    if (!s) return -1;
    for (int i = 0; i < 6; i++) {
        uint8_t hi = (uint8_t)s[i*2], lo = (uint8_t)s[i*2+1];
        hi -= hi >= 'A' ? ('A'-10) : hi >= 'a' ? ('a'-10) : '0';
        lo -= lo >= 'A' ? ('A'-10) : lo >= 'a' ? ('a'-10) : '0';
        mac[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

static int get_mac(usb_device_t *d, uint8_t idx, uint8_t *mac)
{
    uint8_t buf[28] = {0};
    if (usb_get_descriptor(d, USB_DESC_STRING, idx, 0x0409, buf, sizeof(buf)) < 0)
        return -1;
    char asc[13];
    for (int i = 0; i < 12; i++) asc[i] = (char)buf[2 + i*2];
    asc[12] = '\0';
    return parse_mac(asc, mac);
}

static err_t cdc_lwip_output(struct netif *nif, struct pbuf *p)
{
    cdc_t *c = (cdc_t *)nif->state;
    if (!c->up) return ERR_IF;
    static uint8_t tx[CDC_RX];
    uint16_t total = 0;
    for (struct pbuf *q = p; q; q = q->next) {
        if (total + q->len > sizeof(tx)) return ERR_MEM;
        memcpy(tx + total, q->payload, q->len);
        total += (uint16_t)q->len;
    }
    return usb_bulk_out(c->dev, c->bulk_out, tx, total) >= 0 ? ERR_OK : ERR_IF;
}

static err_t cdc_netif_init(struct netif *nif)
{
    cdc_t *c = (cdc_t *)nif->state;
    nif->linkoutput = cdc_lwip_output;
    nif->output     = etharp_output;
    nif->mtu        = (u16_t)c->mtu;
    nif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    nif->hwaddr_len = ETH_HWADDR_LEN;
    memcpy(nif->hwaddr, c->mac, ETH_HWADDR_LEN);
    nif->name[0]    = 'u';
    nif->name[1]    = 'e';
    return ERR_OK;
}

static int cdc_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                      const void *cfg, uint16_t cfg_len)
{
    if (iface->bInterfaceClass    != USB_CLASS_CDC ||
        (iface->bInterfaceSubClass != CDC_ECM_SUBCLASS &&
         iface->bInterfaceSubClass != CDC_NCM_SUBCLASS))
        return 0;

    cdc_t *c = kmalloc(sizeof(*c));
    if (!c) return 0;
    memset(c, 0, sizeof(*c));
    c->dev = d; c->ctrl_if = iface->bInterfaceNumber; c->mtu = 1500;

    const uint8_t *p = (const uint8_t *)iface + iface->bLength;
    const uint8_t *end = (const uint8_t *)cfg + cfg_len;
    uint8_t mac_idx = 0;
    while (p < end && p[0] >= 2 && p[1] != USB_DESC_INTERFACE) {
        if (p[1] == 0x24) {
            if (p[2] == CDC_CS_UNION && p[0] >= 5) c->data_if = p[4];
            if (p[2] == CDC_CS_ETHERNET && p[0] >= (uint8_t)sizeof(ecm_desc_t)) {
                const ecm_desc_t *e = (const ecm_desc_t *)p;
                mac_idx = e->iMACAddress;
                c->mtu  = e->wMaxSegmentSize;
            }
        }
        p += p[0];
    }

    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep) break;
        if (USB_EP_TYPE(ep->bmAttributes) == USB_EP_TYPE_INTR &&
            USB_EP_IS_IN(ep->bEndpointAddress))
            c->intr_ep = ep->bEndpointAddress;
    }

    const usb_iface_desc_t *di = usb_find_iface(cfg, cfg_len, c->data_if, 1);
    if (di) {
        usb_set_interface(d, c->data_if, 1);
        const usb_ep_desc_t *de = (const usb_ep_desc_t *)di;
        for (uint8_t i = 0; i < di->bNumEndpoints; i++) {
            de = usb_next_ep(de, end);
            if (!de || USB_EP_TYPE(de->bmAttributes) != USB_EP_TYPE_BULK) continue;
            if (USB_EP_IS_IN(de->bEndpointAddress)) c->bulk_in  = de->bEndpointAddress;
            else                                     c->bulk_out = de->bEndpointAddress;
        }
    }
    if (!c->bulk_in || !c->bulk_out) { kfree(c); return 0; }

    if (mac_idx) get_mac(d, mac_idx, c->mac);

    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   CDC_SET_FILTER, FILTER_DIRECTED | FILTER_BROADCAST,
                   c->ctrl_if, 0);
    usb_control_out(d, &s, NULL, 0);
    c->up = 1;

    ip4_addr_t z; ip4_addr_set_zero(&z);
    if (!netif_add(&c->nif, &z, &z, &z, c, cdc_netif_init, ethernet_input)) {
        kfree(c); return 0;
    }
    netif_set_up(&c->nif);
    netif_set_link_up(&c->nif);
    g_cdc_n++;

    d->driver_data = c;

    char msg[64];
    ksnprintf(msg, sizeof(msg), "CDC ECM: ue%d %02x:%02x:%02x:%02x:%02x:%02x",
              g_cdc_n - 1,
              c->mac[0],c->mac[1],c->mac[2],c->mac[3],c->mac[4],c->mac[5]);
    klog_ok("usb_cdc", msg);
    return 1;
}

static void cdc_poll(usb_device_t *d)
{
    cdc_t *c = d->driver_data;
    if (!c || !c->bulk_in || !c->up) return;
    int n = usb_bulk_in(d, c->bulk_in, c->rx, CDC_RX);
    if (n <= 0) return;
    struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)n, PBUF_POOL);
    if (!p) return;
    pbuf_take(p, c->rx, (u16_t)n);
    if (c->nif.input(p, &c->nif) != ERR_OK) pbuf_free(p);
}

static void cdc_disc(usb_device_t *d)
{
    cdc_t *c = d->driver_data;
    if (!c) return;
    netif_remove(&c->nif);
    kfree(c);
    d->driver_data = NULL;
}

static usb_driver_t cdc_drv = {
    .name = "usb_cdc", .probe = cdc_probe, .disconnect = cdc_disc, .poll = cdc_poll,
};

void usb_cdc_init(void)
{
    usb_driver_register(&cdc_drv);
    klog_ok("usb_cdc", "CDC ECM driver registered");
}