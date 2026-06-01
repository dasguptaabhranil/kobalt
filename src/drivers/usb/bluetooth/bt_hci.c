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

#define OGF_HC_BB   0x03
#define OGF_INFO    0x04
#define HCI_OP(o,c) ((uint16_t)(((o)<<10)|(c)))

#define OP_RESET         HCI_OP(OGF_HC_BB, 0x0003)
#define OP_READ_BD_ADDR  HCI_OP(OGF_INFO,  0x0009)
#define OP_READ_FEATURES HCI_OP(OGF_INFO,  0x0003)
#define OP_READ_VERSION  HCI_OP(OGF_INFO,  0x0001)
#define OP_READ_BUFSZ    HCI_OP(OGF_INFO,  0x0005)

#define EV_CMD_COMPLETE 0x0E

typedef struct __attribute__((packed)) { uint16_t opcode; uint8_t plen; } hci_cmd_hdr_t;
typedef struct __attribute__((packed)) { uint8_t event; uint8_t plen; } hci_evt_hdr_t;
typedef struct __attribute__((packed)) { uint16_t handle; uint16_t dlen; } hci_acl_hdr_t;
typedef struct __attribute__((packed)) { uint8_t ncmds; uint16_t opcode; uint8_t status; } hci_cc_t;

#define EVT_RING  16
#define PKT_MAX   260

typedef struct { uint8_t data[PKT_MAX]; uint16_t len; } evt_slot_t;

static evt_slot_t g_evts[EVT_RING];
static volatile uint8_t g_eh, g_et;

static void evt_push(const uint8_t *d, uint16_t len)
{
    uint8_t nx = (uint8_t)((g_et + 1u) % EVT_RING);
    if (nx == g_eh) return;
    uint16_t n = len > PKT_MAX ? PKT_MAX : len;
    memcpy(g_evts[g_et].data, d, n);
    g_evts[g_et].len = n;
    g_et = nx;
}

typedef struct {
    int  (*send_cmd)(void *priv, const void *buf, uint16_t len);
    int  (*send_acl)(void *priv, const void *buf, uint16_t len);
    void *priv;
    uint8_t  bd_addr[6];
    uint8_t  lmp_ver;
    uint16_t hci_rev;
    uint8_t  features[8];
    uint16_t acl_mtu, acl_pkts;
    int      ready, le;
} hci_dev_t;

static hci_dev_t g_hci;

int hci_send_cmd(uint16_t op, const void *params, uint8_t plen)
{
    if (!g_hci.send_cmd) return -1;
    uint8_t buf[3 + 255];
    hci_cmd_hdr_t *h = (hci_cmd_hdr_t *)buf;
    h->opcode = op; h->plen = plen;
    if (plen && params) memcpy(buf + sizeof(*h), params, plen);
    return g_hci.send_cmd(g_hci.priv, buf, (uint16_t)(sizeof(*h) + plen));
}

int hci_send_acl(uint16_t handle, const void *data, uint16_t dlen)
{
    if (!g_hci.send_acl) return -1;
    uint8_t *buf = kmalloc(sizeof(hci_acl_hdr_t) + dlen);
    if (!buf) return -1;
    hci_acl_hdr_t *h = (hci_acl_hdr_t *)buf;
    h->handle = (uint16_t)(handle & 0x0FFF) | (2u << 12);
    h->dlen   = dlen;
    memcpy(buf + sizeof(*h), data, dlen);
    int r = g_hci.send_acl(g_hci.priv, buf, (uint16_t)(sizeof(*h) + dlen));
    kfree(buf);
    return r;
}

static int wait_cc(uint16_t op, void *out, uint8_t omax, int ms)
{
    while (ms-- > 0) {
        while (g_eh != g_et) {
            evt_slot_t *s = &g_evts[g_eh];
            g_eh = (uint8_t)((g_eh + 1u) % EVT_RING);
            if (s->len < 2) continue;
            const hci_evt_hdr_t *eh = (const hci_evt_hdr_t *)s->data;
            if (eh->event != EV_CMD_COMPLETE) continue;
            if (s->len < sizeof(*eh) + sizeof(hci_cc_t)) continue;
            const hci_cc_t *cc = (const hci_cc_t *)(s->data + sizeof(*eh));
            if (cc->opcode != op) continue;
            if (cc->status) return -1;
            if (out && omax) {
                uint16_t n = (uint16_t)(s->len - sizeof(*eh) - sizeof(*cc));
                if (n > omax) n = omax;
                memcpy(out, s->data + sizeof(*eh) + sizeof(*cc), n);
            }
            return 0;
        }
        for (volatile int x = 0; x < 100000; x++);
    }
    return -1;
}

void hci_recv_event(const uint8_t *data, uint16_t len)
{
    evt_push(data, len);
}

int hci_init(int (*scmd)(void*, const void*, uint16_t),
              int (*sacl)(void*, const void*, uint16_t),
              void *priv)
{
    memset(&g_hci, 0, sizeof(g_hci));
    g_hci.send_cmd = scmd;
    g_hci.send_acl = sacl;
    g_hci.priv     = priv;
    g_eh = g_et    = 0;

    for (volatile int x = 0; x < 500000; x++);

    if (hci_send_cmd(OP_RESET, NULL, 0) < 0) return -1;
    if (wait_cc(OP_RESET, NULL, 0, 2000) < 0) {
        klog_fail("bt_hci", "HCI Reset timed out");
        return -1;
    }

    hci_send_cmd(OP_READ_VERSION, NULL, 0);
    uint8_t vb[8] = {0};
    if (wait_cc(OP_READ_VERSION, vb, 8, 1000) == 0) {
        g_hci.hci_rev = (uint16_t)(vb[1] | ((uint16_t)vb[2] << 8));
        g_hci.lmp_ver = vb[3];
    }

    hci_send_cmd(OP_READ_FEATURES, NULL, 0);
    if (wait_cc(OP_READ_FEATURES, g_hci.features, 8, 1000) == 0)
        g_hci.le = !!(g_hci.features[4] & (1u << 6));

    hci_send_cmd(OP_READ_BD_ADDR, NULL, 0);
    wait_cc(OP_READ_BD_ADDR, g_hci.bd_addr, 6, 1000);

    hci_send_cmd(OP_READ_BUFSZ, NULL, 0);
    uint8_t bb[7] = {0};
    if (wait_cc(OP_READ_BUFSZ, bb, 7, 1000) == 0) {
        g_hci.acl_mtu  = (uint16_t)(bb[0] | ((uint16_t)bb[1] << 8));
        g_hci.acl_pkts = (uint16_t)(bb[3] | ((uint16_t)bb[4] << 8));
    }

    g_hci.ready = 1;

    char msg[64];
    ksnprintf(msg, sizeof(msg), "BT: %02x:%02x:%02x:%02x:%02x:%02x LMP v%u LE=%d",
              g_hci.bd_addr[5], g_hci.bd_addr[4], g_hci.bd_addr[3],
              g_hci.bd_addr[2], g_hci.bd_addr[1], g_hci.bd_addr[0],
              g_hci.lmp_ver, g_hci.le);
    klog_ok("bt_hci", msg);
    return 0;
}

const uint8_t *hci_bd_addr(void)      { return g_hci.bd_addr; }
int            hci_le_supported(void) { return g_hci.le; }
int            hci_is_ready(void)     { return g_hci.ready; }
