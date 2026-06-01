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

#include "kobalt_ixgbe.h"
#include "ixgbe_api.h"
#include "../../../inc/pci_msi.h"
#include "../../../inc/kmalloc.h"
#include "../../../inc/kernel.h"
#include <string.h>

#define ALIGN128(p)  ((void *)(((uintptr_t)(p) + 127) & ~127UL))

static void ixgbe_tx_irq(int v, void *arg)  { (void)v; (void)arg; }
static void ixgbe_rx_irq(int v, void *arg)  { (void)v; (void)arg; }
static void ixgbe_lsc_irq(int v, void *arg)
{
    kobalt_ixgbe_t *nic = arg;
    (void)v;
    kobalt_ixgbe_link_update(nic);
}

static int setup_tx(kobalt_ixgbe_t *nic)
{
    size_t sz = IXGBE_NUM_TX_DESC * sizeof(union ixgbe_adv_tx_desc);
    nic->tx.raw = kmalloc(sz + 128);
    if (!nic->tx.raw) return -1;
    nic->tx.ring = ALIGN128(nic->tx.raw);
    memset(nic->tx.ring, 0, sz);
    nic->tx.head = nic->tx.tail = 0;

    u64 pa = (u64)(uintptr_t)nic->tx.ring;
    IXGBE_WRITE_REG(&nic->hw, IXGBE_TDBAL(0), (u32)(pa & 0xFFFFFFFF));
    IXGBE_WRITE_REG(&nic->hw, IXGBE_TDBAH(0), (u32)(pa >> 32));
    IXGBE_WRITE_REG(&nic->hw, IXGBE_TDLEN(0), IXGBE_NUM_TX_DESC * 16);
    IXGBE_WRITE_REG(&nic->hw, IXGBE_TDH(0), 0);
    IXGBE_WRITE_REG(&nic->hw, IXGBE_TDT(0), 0);

    u32 txdctl = IXGBE_READ_REG(&nic->hw, IXGBE_TXDCTL(0));
    txdctl |= IXGBE_TXDCTL_ENABLE;
    IXGBE_WRITE_REG(&nic->hw, IXGBE_TXDCTL(0), txdctl);
    return 0;
}

static int setup_rx(kobalt_ixgbe_t *nic)
{
    size_t sz = IXGBE_NUM_RX_DESC * sizeof(union ixgbe_adv_rx_desc);
    nic->rx.raw = kmalloc(sz + 128);
    if (!nic->rx.raw) return -1;
    nic->rx.ring = ALIGN128(nic->rx.raw);
    memset(nic->rx.ring, 0, sz);

    for (int i = 0; i < IXGBE_NUM_RX_DESC; i++) {
        nic->rx.bufs[i] = kmalloc(IXGBE_RX_BUF_SIZE);
        if (!nic->rx.bufs[i]) return -1;
        nic->rx.ring[i].read.pkt_addr = (u64)(uintptr_t)nic->rx.bufs[i];
        nic->rx.ring[i].read.hdr_addr = 0;
    }
    nic->rx.head = nic->rx.tail = 0;

    u64 pa = (u64)(uintptr_t)nic->rx.ring;
    IXGBE_WRITE_REG(&nic->hw, IXGBE_RDBAL(0), (u32)(pa & 0xFFFFFFFF));
    IXGBE_WRITE_REG(&nic->hw, IXGBE_RDBAH(0), (u32)(pa >> 32));
    IXGBE_WRITE_REG(&nic->hw, IXGBE_RDLEN(0), IXGBE_NUM_RX_DESC * 16);
    IXGBE_WRITE_REG(&nic->hw, IXGBE_RDH(0), 0);
    IXGBE_WRITE_REG(&nic->hw, IXGBE_RDT(0), IXGBE_NUM_RX_DESC - 1);

    u32 rxdctl = IXGBE_READ_REG(&nic->hw, IXGBE_RXDCTL(0));
    rxdctl |= IXGBE_RXDCTL_ENABLE;
    IXGBE_WRITE_REG(&nic->hw, IXGBE_RXDCTL(0), rxdctl);
    return 0;
}

static void setup_msix(kobalt_ixgbe_t *nic)
{
    msix_entry_t ents[3] = {{.entry=0},{.entry=1},{.entry=2}};
    if (pci_enable_msix(nic->pdev, ents, 3) < 0) {
        nic->vec_tx = nic->vec_rx = nic->vec_lsc = -1;
        return;
    }
    nic->vec_tx  = ents[0].vector;
    nic->vec_rx  = ents[1].vector;
    nic->vec_lsc = ents[2].vector;
    msi_register_handler(nic->vec_tx,  ixgbe_tx_irq,  nic);
    msi_register_handler(nic->vec_rx,  ixgbe_rx_irq,  nic);
    msi_register_handler(nic->vec_lsc, ixgbe_lsc_irq, nic);

    /*
     * IVAR(0) 82599 layout:
     *   bits[7:0]  = RxQ0 MSI-X entry | 0x80 (valid)
     *   bits[15:8] = TxQ0 MSI-X entry | 0x80 (valid)
     * IVAR_MISC bits[7:0] = LSC entry | 0x80
     */
    IXGBE_WRITE_REG(&nic->hw, IXGBE_IVAR(0),
        (0x80u | 1u) | ((0x80u | 0u) << 8));
    IXGBE_WRITE_REG(&nic->hw, IXGBE_IVAR_MISC, 0x80u | 2u);

    u32 mask = (1u << 0) | (1u << 1) | (1u << 2);
    IXGBE_WRITE_REG(&nic->hw, IXGBE_EIAC, mask);
    IXGBE_WRITE_REG(&nic->hw, IXGBE_EIMS, mask);
}

int kobalt_ixgbe_init(pci_device_t *pdev, kobalt_ixgbe_t *nic)
{
    memset(nic, 0, sizeof(*nic));
    nic->pdev           = pdev;
    nic->osdep.pdev     = pdev;
    nic->hw.back        = &nic->osdep;
    nic->hw.hw_addr     = (uint8_t *)pci_bar_base(pdev, 0);
    nic->hw.vendor_id   = pdev->vendor_id;
    nic->hw.device_id   = pdev->device_id;
    nic->hw.revision_id = pdev->revision;
    nic->hw.subsystem_vendor_id = pdev->subsystem_vendor;
    nic->hw.subsystem_device_id = pdev->subsystem_id;

    pci_enable_device(pdev);

    if (ixgbe_init_shared_code(&nic->hw) != IXGBE_SUCCESS) return -1;
    ixgbe_get_bus_info(&nic->hw);
    if (ixgbe_reset_hw(&nic->hw) != IXGBE_SUCCESS) return -1;
    if (ixgbe_init_hw(&nic->hw)  != IXGBE_SUCCESS) return -1;

    u32 lo = IXGBE_READ_REG(&nic->hw, IXGBE_RAL(0));
    u32 hi = IXGBE_READ_REG(&nic->hw, IXGBE_RAH(0));
    nic->mac[0] = lo & 0xff;        nic->mac[1] = (lo >> 8)  & 0xff;
    nic->mac[2] = (lo >> 16) & 0xff; nic->mac[3] = (lo >> 24) & 0xff;
    nic->mac[4] = hi & 0xff;        nic->mac[5] = (hi >> 8)  & 0xff;

    setup_msix(nic);

    if (setup_tx(nic) < 0) return -1;
    if (setup_rx(nic) < 0) return -1;

    IXGBE_WRITE_REG(&nic->hw, IXGBE_FCTRL,
        IXGBE_FCTRL_BAM | IXGBE_FCTRL_MPE);
    IXGBE_WRITE_REG(&nic->hw, IXGBE_RXCTRL,
        IXGBE_READ_REG(&nic->hw, IXGBE_RXCTRL) | IXGBE_RXCTRL_RXEN);
    IXGBE_WRITE_REG(&nic->hw, IXGBE_DMATXCTL,
        IXGBE_READ_REG(&nic->hw, IXGBE_DMATXCTL) | IXGBE_DMATXCTL_TE);

    nic->up = 1;
    return 0;
}

int kobalt_ixgbe_send(kobalt_ixgbe_t *nic, const void *buf, uint16_t len)
{
    if (!nic->up || len > 1522) return -1;

    uint16_t i    = nic->tx.tail;
    uint16_t next = (i + 1) % IXGBE_NUM_TX_DESC;
    if (next == nic->tx.head) return -1;

    nic->tx.bufs[i] = kmalloc(len);
    if (!nic->tx.bufs[i]) return -1;
    memcpy(nic->tx.bufs[i], buf, len);

    union ixgbe_adv_tx_desc *d = &nic->tx.ring[i];
    d->read.buffer_addr   = (u64)(uintptr_t)nic->tx.bufs[i];
    d->read.cmd_type_len  = IXGBE_ADVTXD_DTYP_DATA | IXGBE_ADVTXD_DCMD_EOP |
                            IXGBE_ADVTXD_DCMD_IFCS | IXGBE_ADVTXD_DCMD_RS  | len;
    d->read.olinfo_status = (u32)len << IXGBE_ADVTXD_PAYLEN_SHIFT;

    nic->tx.tail = next;
    IXGBE_WRITE_REG(&nic->hw, IXGBE_TDT(0), next);
    return 0;
}

int kobalt_ixgbe_poll(kobalt_ixgbe_t *nic, void *buf, uint16_t *out_len)
{
    uint16_t i    = nic->rx.tail;
    uint16_t next = (i + 1) % IXGBE_NUM_RX_DESC;

    union ixgbe_adv_rx_desc *d = &nic->rx.ring[next];
    u32 st = d->wb.upper.status_error;

    if (!(st & IXGBE_RXD_STAT_DD)) return -1;

    if (st & IXGBE_RXDADV_ERR_RXE) {
        d->wb.upper.status_error = 0;
        d->read.pkt_addr = (u64)(uintptr_t)nic->rx.bufs[next];
        IXGBE_WRITE_REG(&nic->hw, IXGBE_RDT(0), next);
        nic->rx.tail = next;
        return -1;
    }

    uint16_t len = d->wb.upper.length;
    if (len > IXGBE_RX_BUF_SIZE) len = IXGBE_RX_BUF_SIZE;
    memcpy(buf, nic->rx.bufs[next], len);
    *out_len = len;

    d->wb.upper.status_error = 0;
    d->read.pkt_addr = (u64)(uintptr_t)nic->rx.bufs[next];
    IXGBE_WRITE_REG(&nic->hw, IXGBE_RDT(0), next);
    nic->rx.tail = next;
    return 0;
}

void kobalt_ixgbe_link_update(kobalt_ixgbe_t *nic)
{
    ixgbe_link_speed speed;
    bool link_up;
    ixgbe_check_link(&nic->hw, &speed, &link_up, false);
}