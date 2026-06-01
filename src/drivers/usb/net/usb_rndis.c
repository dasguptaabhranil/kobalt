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

#define RNDIS_MSG_PACKET  0x00000001U
#define RNDIS_MSG_INIT    0x00000002U
#define RNDIS_MSG_INIT_C  0x80000002U
#define RNDIS_MSG_QUERY   0x00000004U
#define RNDIS_MSG_QUERY_C 0x80000004U
#define RNDIS_MSG_SET     0x00000005U
#define RNDIS_MSG_SET_C   0x80000005U

#define OID_GEN_CURRENT_PACKET_FILTER 0x0001010EU
#define OID_802_3_PERMANENT_ADDRESS   0x01010101U

#define NDIS_FILTER_DIRECTED  0x00000001U
#define NDIS_FILTER_MULTICAST 0x00000004U
#define NDIS_FILTER_BROADCAST 0x00000008U

#define RNDIS_STATUS_SUCCESS 0x00000000U

typedef struct __attribute__((packed)) {
    uint32_t MessageType, MessageLength, RequestId;
    uint32_t MajorVersion, MinorVersion, MaxTransferSize;
} rndis_init_t;

typedef struct __attribute__((packed)) {
    uint32_t MessageType, MessageLength, RequestId, Status;
    uint32_t MajorVersion, MinorVersion, DeviceFlags, Medium;
    uint32_t MaxPacketsPerTransfer, MaxTransferSize, PacketAlignmentFactor;
    uint32_t AFListOffset, AFListSize;
} rndis_init_c_t;

typedef struct __attribute__((packed)) {
    uint32_t MessageType, MessageLength, RequestId, Oid;
    uint32_t InformationBufferLength, InformationBufferOffset, DeviceVcHandle;
} rndis_query_t;

typedef struct __attribute__((packed)) {
    uint32_t MessageType, MessageLength, RequestId, Status;
    uint32_t InformationBufferLength, InformationBufferOffset;
} rndis_query_c_t;

typedef struct __attribute__((packed)) {
    uint32_t MessageType, MessageLength, DataOffset, DataLength;
    uint32_t OOBDataOffset, OOBDataLength, NumOOBDataElements;
    uint32_t PerPacketInfoOffset, PerPacketInfoLength, VcHandle, Reserved;
} rndis_pkt_t;

#define RNDIS_DATA_SZ 1664
#define RNDIS_CTRL_SZ 512

typedef struct {
    usb_device_t *dev;
    uint8_t  ctrl_if, data_if;
    uint8_t  intr_ep, bulk_in, bulk_out;
    uint32_t req_id;
    uint8_t  mac[6];
    struct netif nif;
    uint8_t  rx[RNDIS_DATA_SZ];
    uint8_t  tx[RNDIS_DATA_SZ];
    uint8_t  ctrl[RNDIS_CTRL_SZ];
} rndis_t;

static int g_rn;

static int rndis_ctrl_out(rndis_t *r, const void *msg, uint32_t len)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   0x00, 0, r->ctrl_if, (uint16_t)len);
    return usb_control_out(r->dev, &s, msg, (uint16_t)len);
}

static int rndis_ctrl_in(rndis_t *r, void *buf, uint16_t len)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   0x01, 0, r->ctrl_if, len);
    return usb_control_in(r->dev, &s, buf, len);
}

static int rndis_init_msg(rndis_t *r)
{
    rndis_init_t m = {0};
    m.MessageType     = RNDIS_MSG_INIT;
    m.MessageLength   = sizeof(m);
    m.RequestId       = ++r->req_id;
    m.MajorVersion    = 1;
    m.MaxTransferSize = RNDIS_DATA_SZ;
    if (rndis_ctrl_out(r, &m, sizeof(m)) < 0) return -1;
    rndis_init_c_t c = {0};
    if (rndis_ctrl_in(r, &c, sizeof(c)) < 0) return -1;
    return c.Status == RNDIS_STATUS_SUCCESS ? 0 : -1;
}

static int rndis_query(rndis_t *r, uint32_t oid, void *out, uint32_t olen)
{
    rndis_query_t q = {0};
    q.MessageType   = RNDIS_MSG_QUERY;
    q.MessageLength = sizeof(q);
    q.RequestId     = ++r->req_id;
    q.Oid           = oid;
    if (rndis_ctrl_out(r, &q, sizeof(q)) < 0) return -1;
    if (rndis_ctrl_in(r, r->ctrl, RNDIS_CTRL_SZ) < 0) return -1;
    rndis_query_c_t *c = (rndis_query_c_t *)r->ctrl;
    if (c->Status != RNDIS_STATUS_SUCCESS) return -1;
    uint32_t dl = c->InformationBufferLength < olen ? c->InformationBufferLength : olen;
    memcpy(out, r->ctrl + 8 + c->InformationBufferOffset, dl);
    return (int)dl;
}

static int rndis_set(rndis_t *r, uint32_t oid, const void *val, uint32_t vlen)
{
    uint32_t total = (uint32_t)sizeof(rndis_query_t) + vlen;
    if (total > RNDIS_CTRL_SZ) return -1;
    memset(r->ctrl, 0, total);
    rndis_query_t *q = (rndis_query_t *)r->ctrl;
    q->MessageType   = RNDIS_MSG_SET;
    q->MessageLength = total;
    q->RequestId     = ++r->req_id;
    q->Oid           = oid;
    q->InformationBufferLength = vlen;
    q->InformationBufferOffset = sizeof(*q) - 8;
    memcpy(r->ctrl + sizeof(*q), val, vlen);
    if (rndis_ctrl_out(r, r->ctrl, total) < 0) return -1;
    uint32_t c[4] = {0};
    rndis_ctrl_in(r, c, sizeof(c));
    return 0;
}

static err_t rndis_lwip_output(struct netif *nif, struct pbuf *p)
{
    rndis_t *r = (rndis_t *)nif->state;
    uint32_t len = 0;
    for (struct pbuf *q = p; q; q = q->next) len += q->len;
    uint32_t total = sizeof(rndis_pkt_t) + len;
    if (total > RNDIS_DATA_SZ) return ERR_MEM;

    rndis_pkt_t *h = (rndis_pkt_t *)r->tx;
    memset(h, 0, sizeof(*h));
    h->MessageType   = RNDIS_MSG_PACKET;
    h->MessageLength = total;
    h->DataOffset    = sizeof(*h) - 8;
    h->DataLength    = len;

    uint32_t off = sizeof(*h);
    for (struct pbuf *q = p; q; q = q->next) {
        memcpy(r->tx + off, q->payload, q->len);
        off += q->len;
    }
    return usb_bulk_out(r->dev, r->bulk_out, r->tx, total) >= 0 ? ERR_OK : ERR_IF;
}

static err_t rndis_netif_init(struct netif *nif)
{
    rndis_t *r = (rndis_t *)nif->state;
    nif->linkoutput = rndis_lwip_output;
    nif->output     = etharp_output;
    nif->mtu        = 1500;
    nif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    nif->hwaddr_len = ETH_HWADDR_LEN;
    memcpy(nif->hwaddr, r->mac, ETH_HWADDR_LEN);
    nif->name[0]    = 'r';
    nif->name[1]    = 'n';
    return ERR_OK;
}

#define CS_UNION 0x06

static int rndis_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                        const void *cfg, uint16_t cfg_len)
{
    int match = (iface->bInterfaceClass == 0xEF &&
                 iface->bInterfaceSubClass == 0x04 &&
                 iface->bInterfaceProtocol == 0x01) ||
                (iface->bInterfaceClass == USB_CLASS_WIRELESS &&
                 iface->bInterfaceSubClass == 0x01 &&
                 iface->bInterfaceProtocol == 0x03);
    if (!match) return 0;

    rndis_t *r = kmalloc(sizeof(*r));
    if (!r) return 0;
    memset(r, 0, sizeof(*r));
    r->dev = d; r->ctrl_if = iface->bInterfaceNumber;

    const uint8_t *end = (const uint8_t *)cfg + cfg_len;
    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep) break;
        if (USB_EP_TYPE(ep->bmAttributes) == USB_EP_TYPE_INTR &&
            USB_EP_IS_IN(ep->bEndpointAddress))
            r->intr_ep = ep->bEndpointAddress;
    }

    const uint8_t *p = (const uint8_t *)iface + iface->bLength;
    while (p < end && p[0] >= 2 && p[1] != USB_DESC_INTERFACE) {
        if (p[1] == 0x24 && p[2] == CS_UNION && p[0] >= 5) r->data_if = p[4];
        p += p[0];
    }

    const usb_iface_desc_t *di = usb_find_iface(cfg, cfg_len, r->data_if, 0);
    if (di) {
        const usb_ep_desc_t *de = (const usb_ep_desc_t *)di;
        for (uint8_t i = 0; i < di->bNumEndpoints; i++) {
            de = usb_next_ep(de, end);
            if (!de || USB_EP_TYPE(de->bmAttributes) != USB_EP_TYPE_BULK) continue;
            if (USB_EP_IS_IN(de->bEndpointAddress)) r->bulk_in  = de->bEndpointAddress;
            else                                     r->bulk_out = de->bEndpointAddress;
        }
    }
    if (!r->bulk_in || !r->bulk_out) { kfree(r); return 0; }

    if (rndis_init_msg(r) < 0) {
        klog_warn("usb_rndis", "RNDIS INIT failed");
        kfree(r); return 0;
    }
    rndis_query(r, OID_802_3_PERMANENT_ADDRESS, r->mac, 6);
    uint32_t flt = NDIS_FILTER_DIRECTED | NDIS_FILTER_BROADCAST | NDIS_FILTER_MULTICAST;
    rndis_set(r, OID_GEN_CURRENT_PACKET_FILTER, &flt, 4);

    ip4_addr_t z; ip4_addr_set_zero(&z);
    if (!netif_add(&r->nif, &z, &z, &z, r, rndis_netif_init, ethernet_input)) {
        kfree(r); return 0;
    }
    netif_set_up(&r->nif);
    netif_set_link_up(&r->nif);
    g_rn++;

    d->driver_data = r;

    char msg[64];
    ksnprintf(msg, sizeof(msg), "RNDIS: rn%d %02x:%02x:%02x:%02x:%02x:%02x",
              g_rn - 1,
              r->mac[0],r->mac[1],r->mac[2],r->mac[3],r->mac[4],r->mac[5]);
    klog_ok("usb_rndis", msg);
    return 1;
}

static void rndis_poll(usb_device_t *d)
{
    rndis_t *r = d->driver_data;
    if (!r || !r->bulk_in) return;
    int n = usb_bulk_in(d, r->bulk_in, r->rx, RNDIS_DATA_SZ);
    if (n <= (int)sizeof(rndis_pkt_t)) return;
    rndis_pkt_t *h = (rndis_pkt_t *)r->rx;
    if (h->MessageType != RNDIS_MSG_PACKET) return;
    uint8_t *pl = r->rx + 8 + h->DataOffset;
    uint32_t dlen = h->DataLength;
    if ((uintptr_t)(pl + dlen) > (uintptr_t)(r->rx + n)) return;
    struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)dlen, PBUF_POOL);
    if (!p) return;
    pbuf_take(p, pl, (u16_t)dlen);
    if (r->nif.input(p, &r->nif) != ERR_OK) pbuf_free(p);
}

static void rndis_disc(usb_device_t *d)
{
    rndis_t *r = d->driver_data;
    if (!r) return;
    netif_remove(&r->nif);
    kfree(r);
    d->driver_data = NULL;
}

static usb_driver_t rndis_drv = {
    .name = "usb_rndis", .probe = rndis_probe, .disconnect = rndis_disc, .poll = rndis_poll,
};

void usb_rndis_init(void)
{
    usb_driver_register(&rndis_drv);
    klog_ok("usb_rndis", "RNDIS driver registered");
}