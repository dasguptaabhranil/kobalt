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

#include "kobalt_igc.h"
#include "igc_api.h"
#include "igc_defines.h"
#include "igc_regs.h"
#include "../../../inc/pci_msi.h"
#include "../../../inc/kmalloc.h"
#include "../../../inc/kernel.h"
#include <string.h>

#define ALIGN128(p)  ((void *)(((uintptr_t)(p) + 127) & ~127UL))

#ifndef IGC_FC_PAUSE_TIME
#define IGC_FC_PAUSE_TIME       0x0680
#endif
#ifndef IGC_COLD_SHIFT
#define IGC_COLD_SHIFT          12
#endif
#ifndef IGC_COLLISION_THRESHOLD
#define IGC_COLLISION_THRESHOLD 15
#endif
#ifndef IGC_CT_SHIFT
#define IGC_CT_SHIFT            4
#endif

static void igc_tx_irq(int v, void *arg) { (void)v; (void)arg; }
static void igc_rx_irq(int v, void *arg) { (void)v; (void)arg; }
static void igc_lsc_irq(int v, void *arg)
{
    kobalt_igc_t *nic = arg;
    (void)v;
    kobalt_igc_link_update(nic);
}

static int setup_tx(kobalt_igc_t *nic)
{
    size_t sz = IGC_NUM_TX_DESC * sizeof(union igc_adv_tx_desc);
    nic->tx.raw = kmalloc(sz + 128);
    if (!nic->tx.raw) return -1;
    nic->tx.ring = ALIGN128(nic->tx.raw);
    memset(nic->tx.ring, 0, sz);
    nic->tx.head = nic->tx.tail = 0;

    u64 pa = (u64)(uintptr_t)nic->tx.ring;
    IGC_WRITE_REG(&nic->hw, IGC_TDBAL(0), (u32)(pa & 0xFFFFFFFF));
    IGC_WRITE_REG(&nic->hw, IGC_TDBAH(0), (u32)(pa >> 32));
    IGC_WRITE_REG(&nic->hw, IGC_TDLEN(0), IGC_NUM_TX_DESC * 16);
    IGC_WRITE_REG(&nic->hw, IGC_TDH(0), 0);
    IGC_WRITE_REG(&nic->hw, IGC_TDT(0), 0);

    u32 txdctl = IGC_READ_REG(&nic->hw, IGC_TXDCTL(0));
    txdctl |= IGC_TXDCTL_QUEUE_ENABLE;
    IGC_WRITE_REG(&nic->hw, IGC_TXDCTL(0), txdctl);
    return 0;
}

static int setup_rx(kobalt_igc_t *nic)
{
    size_t sz = IGC_NUM_RX_DESC * sizeof(union igc_adv_rx_desc);
    nic->rx.raw = kmalloc(sz + 128);
    if (!nic->rx.raw) return -1;
    nic->rx.ring = ALIGN128(nic->rx.raw);
    memset(nic->rx.ring, 0, sz);

    for (int i = 0; i < IGC_NUM_RX_DESC; i++) {
        nic->rx.bufs[i] = kmalloc(IGC_RX_BUF_SIZE);
        if (!nic->rx.bufs[i]) return -1;
        nic->rx.ring[i].read.pkt_addr = (u64)(uintptr_t)nic->rx.bufs[i];
        nic->rx.ring[i].read.hdr_addr = 0;
    }
    nic->rx.head = nic->rx.tail = 0;

    u64 pa = (u64)(uintptr_t)nic->rx.ring;
    IGC_WRITE_REG(&nic->hw, IGC_RDBAL(0), (u32)(pa & 0xFFFFFFFF));
    IGC_WRITE_REG(&nic->hw, IGC_RDBAH(0), (u32)(pa >> 32));
    IGC_WRITE_REG(&nic->hw, IGC_RDLEN(0), IGC_NUM_RX_DESC * 16);
    IGC_WRITE_REG(&nic->hw, IGC_RDH(0), 0);
    IGC_WRITE_REG(&nic->hw, IGC_RDT(0), IGC_NUM_RX_DESC - 1);

    u32 rxdctl = IGC_READ_REG(&nic->hw, IGC_RXDCTL(0));
    rxdctl |= IGC_RXDCTL_QUEUE_ENABLE;
    IGC_WRITE_REG(&nic->hw, IGC_RXDCTL(0), rxdctl);
    return 0;
}

static void setup_msix(kobalt_igc_t *nic)
{
    msix_entry_t ents[3] = {{.entry=0},{.entry=1},{.entry=4}};
    if (pci_enable_msix(nic->pdev, ents, 3) < 0) {
        nic->vec_tx = nic->vec_rx = nic->vec_lsc = -1;
        return;
    }
    nic->vec_tx  = ents[0].vector;
    nic->vec_rx  = ents[1].vector;
    nic->vec_lsc = ents[2].vector;
    msi_register_handler(nic->vec_tx,  igc_tx_irq,  nic);
    msi_register_handler(nic->vec_rx,  igc_rx_irq,  nic);
    msi_register_handler(nic->vec_lsc, igc_lsc_irq, nic);

    /*
     * IVAR0: bits[7:0]=TX0 entry, bits[15:8]=RX0 entry, bit7/15=valid
     * IVAR_MISC: bits[7:0]=link/other entry, bit7=valid
     */
    IGC_WRITE_REG(&nic->hw, IGC_IVAR0,     0x8180u);
    IGC_WRITE_REG(&nic->hw, IGC_IVAR_MISC, 0x0084u);

    u32 mask = (1u<<0)|(1u<<1)|(1u<<4);
    IGC_WRITE_REG(&nic->hw, IGC_EIAC, mask);
    IGC_WRITE_REG(&nic->hw, IGC_EIMS, mask);
}

int kobalt_igc_init(pci_device_t *pdev, kobalt_igc_t *nic)
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

    if (igc_setup_init_funcs(&nic->hw, 1) != IGC_SUCCESS) return -1;
    igc_get_bus_info(&nic->hw);
    if (igc_reset_hw(&nic->hw) != IGC_SUCCESS) return -1;
    if (igc_init_hw(&nic->hw)  != IGC_SUCCESS) return -1;

    u32 lo = IGC_READ_REG(&nic->hw, IGC_RAL(0));
    u32 hi = IGC_READ_REG(&nic->hw, IGC_RAH(0));
    nic->mac[0] = lo & 0xff;       nic->mac[1] = (lo>>8)  & 0xff;
    nic->mac[2] = (lo>>16) & 0xff; nic->mac[3] = (lo>>24) & 0xff;
    nic->mac[4] = hi & 0xff;       nic->mac[5] = (hi>>8)  & 0xff;

    setup_msix(nic);

    if (setup_tx(nic) < 0) return -1;
    if (setup_rx(nic) < 0) return -1;

    /*
     * RCTL: enable, broadcast accept, strip FCS, 2KB buf size
     * TCTL: enable, pad short packets, full-duplex collision distance
     */
    IGC_WRITE_REG(&nic->hw, IGC_RCTL,
        IGC_RCTL_EN | IGC_RCTL_BAM | IGC_RCTL_SECRC | IGC_RCTL_LPE);
    IGC_WRITE_REG(&nic->hw, IGC_TCTL,
        IGC_TCTL_EN | IGC_TCTL_PSP |
        (IGC_COLLISION_THRESHOLD << IGC_CT_SHIFT) |
        (IGC_FC_PAUSE_TIME << IGC_COLD_SHIFT));

    nic->up = 1;
    return 0;
}

int kobalt_igc_send(kobalt_igc_t *nic, const void *buf, uint16_t len)
{
    if (!nic->up || len > 1522) return -1;

    uint16_t i = nic->tx.tail;
    uint16_t next = (i + 1) % IGC_NUM_TX_DESC;
    if (next == nic->tx.head) return -1;

    nic->tx.bufs[i] = kmalloc(len);
    if (!nic->tx.bufs[i]) return -1;
    memcpy(nic->tx.bufs[i], buf, len);

    union igc_adv_tx_desc *d = &nic->tx.ring[i];
    d->read.buffer_addr  = (u64)(uintptr_t)nic->tx.bufs[i];
    d->read.cmd_type_len = IGC_ADVTXD_DTYP_DATA | IGC_ADVTXD_DCMD_EOP |
                           IGC_ADVTXD_DCMD_IFCS | IGC_ADVTXD_DCMD_RS  | len;
    d->read.olinfo_status = (u32)len << IGC_ADVTXD_PAYLEN_SHIFT;

    nic->tx.tail = next;
    IGC_WRITE_REG(&nic->hw, IGC_TDT(0), next);
    return 0;
}

int kobalt_igc_poll(kobalt_igc_t *nic, void *buf, uint16_t *out_len)
{
    uint16_t i = nic->rx.tail;
    uint16_t next = (i + 1) % IGC_NUM_RX_DESC;

    union igc_adv_rx_desc *d = &nic->rx.ring[next];
    u32 st = d->wb.upper.status_error;

    if (!(st & IGC_RXD_STAT_DD)) return -1;
    if (st & IGC_RXDEXT_STATERR_RXE) {
        d->wb.upper.status_error = 0;
        d->read.pkt_addr = (u64)(uintptr_t)nic->rx.bufs[next];
        IGC_WRITE_REG(&nic->hw, IGC_RDT(0), next);
        nic->rx.tail = next;
        return -1;
    }

    uint16_t len = d->wb.upper.length;
    if (len > IGC_RX_BUF_SIZE) len = IGC_RX_BUF_SIZE;
    memcpy(buf, nic->rx.bufs[next], len);
    *out_len = len;

    d->wb.upper.status_error = 0;
    d->read.pkt_addr = (u64)(uintptr_t)nic->rx.bufs[next];
    IGC_WRITE_REG(&nic->hw, IGC_RDT(0), next);
    nic->rx.tail = next;
    return 0;
}

void kobalt_igc_link_update(kobalt_igc_t *nic)
{
    igc_check_for_link(&nic->hw);
}