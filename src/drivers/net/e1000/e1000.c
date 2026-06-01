/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "e1000.h"
#include "e1000_api.h"
#include <kmalloc.h>
#include <kfmt.h>
#include <irq.h>
#include <idt.h>
#include <gdt.h>
#include <../net/lwip/include/lwip/etharp.h>
#include <../net/lwip/include/netif/ethernet.h>
#include <../net/lwip/include/lwip/pbuf.h>
#include <../net/lwip/include/lwip/ip4_addr.h>
#include <string.h>

#define LTXD_CMD_EOP    0x01
#define LTXD_CMD_IFCS   0x02
#define LTXD_CMD_RS     0x08
#define LTXD_STAT_DD    0x01

#define LRXD_STAT_DD    0x01
#define LRXD_STAT_EOP   0x02

#define TCTL_CT_SHIFT   4
#define TCTL_COLD_SHIFT 12

#define E1000_VENDOR    0x8086

extern void e1000_irq_entry(void);

static e1000_adapter_t g_adapter;
static int g_present;

static void setup_tx(e1000_adapter_t *ad)
{
    memset(ad->tx_ring, 0,
           E1000_NUM_TX_DESC * sizeof(struct e1000_legacy_tx_desc));

    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        ad->tx_ring[i].buf_addr = (uint64_t)(uintptr_t)ad->tx_bufs[i];
        ad->tx_ring[i].status   = LTXD_STAT_DD;
    }

    uint64_t base = (uint64_t)(uintptr_t)ad->tx_ring;
    E1000_WRITE_REG(&ad->hw, E1000_TDBAL(0), (uint32_t)base);
    E1000_WRITE_REG(&ad->hw, E1000_TDBAH(0), (uint32_t)(base >> 32));
    E1000_WRITE_REG(&ad->hw, E1000_TDLEN(0),
                    E1000_NUM_TX_DESC * sizeof(struct e1000_legacy_tx_desc));
    E1000_WRITE_REG(&ad->hw, E1000_TDH(0), 0);
    E1000_WRITE_REG(&ad->hw, E1000_TDT(0), 0);

    uint32_t tctl = E1000_READ_REG(&ad->hw, E1000_TCTL);
    tctl |= E1000_TCTL_EN | E1000_TCTL_PSP;
    tctl &= ~E1000_TCTL_CT;
    tctl |= (0x10u << TCTL_CT_SHIFT);
    tctl &= ~E1000_TCTL_COLD;
    tctl |= (0x40u << TCTL_COLD_SHIFT);
    E1000_WRITE_REG(&ad->hw, E1000_TCTL, tctl);
}

static void setup_rx(e1000_adapter_t *ad)
{
    memset(ad->rx_ring, 0,
           E1000_NUM_RX_DESC * sizeof(struct e1000_legacy_rx_desc));

    for (int i = 0; i < E1000_NUM_RX_DESC; i++)
        ad->rx_ring[i].buf_addr = (uint64_t)(uintptr_t)ad->rx_bufs[i];

    uint64_t base = (uint64_t)(uintptr_t)ad->rx_ring;
    E1000_WRITE_REG(&ad->hw, E1000_RDBAL(0), (uint32_t)base);
    E1000_WRITE_REG(&ad->hw, E1000_RDBAH(0), (uint32_t)(base >> 32));
    E1000_WRITE_REG(&ad->hw, E1000_RDLEN(0),
                    E1000_NUM_RX_DESC * sizeof(struct e1000_legacy_rx_desc));
    E1000_WRITE_REG(&ad->hw, E1000_RDH(0), 0);
    E1000_WRITE_REG(&ad->hw, E1000_RDT(0), E1000_NUM_RX_DESC - 1);

    ad->rx_tail = 0;

    uint32_t rctl = E1000_RCTL_EN | E1000_RCTL_BAM
                  | E1000_RCTL_SZ_2048 | E1000_RCTL_SECRC;
    E1000_WRITE_REG(&ad->hw, E1000_RCTL, rctl);
}

static void e1000_poll_rx(e1000_adapter_t *ad)
{
    for (;;) {
        uint32_t i = ad->rx_tail;
        struct e1000_legacy_rx_desc *d = &ad->rx_ring[i];

        if (!(d->status & LRXD_STAT_DD))
            break;

        uint16_t len = d->length;
        if (len && (d->status & LRXD_STAT_EOP)) {
            struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
            if (p) {
                memcpy(p->payload, ad->rx_bufs[i], len);
                if (ad->netif.input(p, &ad->netif) != ERR_OK)
                    pbuf_free(p);
            }
        }

        d->status = 0;
        d->buf_addr = (uint64_t)(uintptr_t)ad->rx_bufs[i];

        ad->rx_tail = (i + 1u) % E1000_NUM_RX_DESC;
        E1000_WRITE_REG(&ad->hw, E1000_RDT(0), i);
    }
}

void e1000_irq_handler(uint8_t irq, void *arg)
{
    (void)irq;
    e1000_adapter_t *ad = arg;
    uint32_t flags = spin_lock_irqsave(&ad->lock);
    uint32_t icr = E1000_READ_REG(&ad->hw, E1000_ICR);
    if (icr & (E1000_ICR_RXT0 | E1000_ICR_RXDMT0))
        e1000_poll_rx(ad);
    spin_unlock_irqrestore(&ad->lock, flags);
}

static err_t e1000_linkoutput(struct netif *nif, struct pbuf *p)
{
    e1000_adapter_t *ad = nif->state;
    uint32_t flags = spin_lock_irqsave(&ad->lock);

    uint32_t tail = ad->tx_tail;
    struct e1000_legacy_tx_desc *d = &ad->tx_ring[tail];

    if (!(d->status & LTXD_STAT_DD)) {
        spin_unlock_irqrestore(&ad->lock, flags);
        return ERR_MEM;
    }

    uint16_t len = 0;
    for (struct pbuf *q = p; q; q = q->next) {
        if (len + q->len > E1000_BUF_SIZE) break;
        memcpy(ad->tx_bufs[tail] + len, q->payload, q->len);
        len += (uint16_t)q->len;
    }

    d->buf_addr = (uint64_t)(uintptr_t)ad->tx_bufs[tail];
    d->length   = len;
    d->cmd      = LTXD_CMD_EOP | LTXD_CMD_IFCS | LTXD_CMD_RS;
    d->status   = 0;

    ad->tx_tail = (tail + 1u) % E1000_NUM_TX_DESC;
    E1000_WRITE_REG(&ad->hw, E1000_TDT(0), ad->tx_tail);

    spin_unlock_irqrestore(&ad->lock, flags);
    return ERR_OK;
}

static err_t e1000_netif_init(struct netif *nif)
{
    e1000_adapter_t *ad = nif->state;

    nif->linkoutput = e1000_linkoutput;
    nif->output     = etharp_output;
    nif->mtu        = 1500;
    nif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP
                    | NETIF_FLAG_LINK_UP;
    nif->hwaddr_len = ETHARP_HWADDR_LEN;
    memcpy(nif->hwaddr, ad->hw.mac.addr, ETHARP_HWADDR_LEN);
    return ERR_OK;
}

void e1000_init(void)
{
    pci_device_t *pdev = NULL;
    uint32_t n = pci_count();
    for (uint32_t i = 0; i < n; i++) {
        pci_device_t *d = pci_get_device(i);
        if (d->vendor_id == E1000_VENDOR &&
            d->class_code == PCI_CLASS_NETWORK &&
            d->subclass   == 0x00u) {
            pdev = d;
            break;
        }
    }

    if (!pdev) {
        klog_warn("e1000", "no Intel GbE NIC found");
        return;
    }

    e1000_adapter_t *ad = &g_adapter;
    memset(ad, 0, sizeof(*ad));
    ad->lock = SPINLOCK_INIT;
    ad->pdev = pdev;

    pci_enable_device(pdev);

    uintptr_t bar0 = pci_bar_base(pdev, 0);
    if (!bar0) {
        klog_warn("e1000", "BAR0 is zero");
        return;
    }

    ad->hw.hw_addr = (u8 *)(uintptr_t)bar0;
    ad->hw.back    = &ad->osdep;

    ad->osdep.pdev       = pdev;
    ad->osdep.flash_addr = NULL;

    ad->hw.vendor_id            = pdev->vendor_id;
    ad->hw.device_id            = pdev->device_id;
    ad->hw.revision_id          = pdev->revision;
    ad->hw.subsystem_vendor_id  = pdev->subsystem_vendor;
    ad->hw.subsystem_device_id  = pdev->subsystem_id;

    if (e1000_set_mac_type(&ad->hw)) {
        klog_warn("e1000", "unsupported device ID");
        return;
    }

    if (e1000_setup_init_funcs(&ad->hw, true)) {
        klog_warn("e1000", "setup_init_funcs failed");
        return;
    }

    e1000_get_bus_info(&ad->hw);
    e1000_reset_hw(&ad->hw);
    e1000_init_hw(&ad->hw);
    e1000_read_mac_addr(&ad->hw);
    e1000_get_speed_and_duplex(&ad->hw,
        &(uint16_t){0}, &(uint16_t){0});

    ad->tx_ring = kmalloc(E1000_NUM_TX_DESC *
                          sizeof(struct e1000_legacy_tx_desc));
    ad->rx_ring = kmalloc(E1000_NUM_RX_DESC *
                          sizeof(struct e1000_legacy_rx_desc));

    for (int i = 0; i < E1000_NUM_TX_DESC; i++)
        ad->tx_bufs[i] = kmalloc(E1000_BUF_SIZE);
    for (int i = 0; i < E1000_NUM_RX_DESC; i++)
        ad->rx_bufs[i] = kmalloc(E1000_BUF_SIZE);

    if (!ad->tx_ring || !ad->rx_ring) {
        klog_warn("e1000", "ring alloc failed");
        return;
    }

    setup_tx(ad);
    setup_rx(ad);

    E1000_WRITE_REG(&ad->hw, E1000_IMS,
                    E1000_IMS_RXT0 | E1000_IMS_RXDMT0 | E1000_IMS_TXDW);

    uint8_t irq_line = (uint8_t)pdev->irq_line;
    uint8_t vec      = (uint8_t)(IRQ_IOAPIC_VECTOR_BASE + irq_line);
    idt_set_gate(vec, (uintptr_t)e1000_irq_entry,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, 0);
    irq_register(irq_line, vec, e1000_irq_handler, ad, "e1000");
    irq_unmask(irq_line);

    ip4_addr_t ip, nm, gw;
    IP4_ADDR(&ip, 10, 0, 2, 16);
    IP4_ADDR(&nm, 255, 255, 255, 0);
    IP4_ADDR(&gw, 10, 0, 2, 2);

    netif_add(&ad->netif, &ip, &nm, &gw, ad, e1000_netif_init, ethernet_input);
    netif_set_default(&ad->netif);
    netif_set_up(&ad->netif);

    g_present = 1;
    char msg[48];
    ksnprintf(msg, sizeof(msg), "e1000 up: %02x:%02x:%02x:%02x:%02x:%02x",
              ad->hw.mac.addr[0], ad->hw.mac.addr[1], ad->hw.mac.addr[2],
              ad->hw.mac.addr[3], ad->hw.mac.addr[4], ad->hw.mac.addr[5]);
    klog_ok("e1000", msg);
    klog_info("net", "static: 10.0.2.16 / 255.255.255.0  gw 10.0.2.2");
}

int e1000_present(void)
{
    return g_present;
}

struct netif *e1000_get_netif(void)
{
    return g_present ? &g_adapter.netif : NULL;
}

void e1000_poll(void)
{
    if (!g_present) return;
    uint32_t flags = spin_lock_irqsave(&g_adapter.lock);
    e1000_poll_rx(&g_adapter);
    spin_unlock_irqrestore(&g_adapter.lock, flags);
}