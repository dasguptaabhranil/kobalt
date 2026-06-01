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

#include "ahci.h"
#include <pci.h>
#include <blkdev.h>
#include <kmalloc.h>
#include <kernel.h>
#include <kfmt.h>

static inline uint32_t mmio_read32(const volatile void *addr)
{
    return *(const volatile uint32_t *)addr;
}
static inline void mmio_write32(volatile void *addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}
#define HBA_REG(base, off)   ((volatile void *)((uintptr_t)(base) + (off)))
#define PORT_BASE(abar, p)   ((volatile void *)((uintptr_t)(abar) + 0x100u + (p)*0x80u))
#define PORT_REG(pb, off)    ((volatile void *)((uintptr_t)(pb)   + (off)))

static ahci_cmd_hdr_t g_cmd_list[AHCI_MAX_PORTS][AHCI_MAX_CMD_SLOTS]
    __attribute__((aligned(1024)));

static ahci_recv_fis_t g_recv_fis[AHCI_MAX_PORTS]
    __attribute__((aligned(256)));

static ahci_cmd_tbl_t g_cmd_tbl[AHCI_MAX_PORTS][AHCI_MAX_CMD_SLOTS]
    __attribute__((aligned(128)));

static uint8_t g_identify_buf[512] __attribute__((aligned(512)));

static volatile void *g_abar     = NULL;
static ahci_port_t    g_ports[AHCI_MAX_PORTS];
static int            g_num_drives = 0;
static int            g_initialised = 0;

#define AHCI_SPIN_LIMIT     4000000u

static int ahci_spin_while(const volatile uint32_t *reg, uint32_t mask,
                           uint32_t expected, uint32_t limit)
{
    uint32_t i;
    for (i = 0; i < limit; i++) {
        if ((mmio_read32(reg) & mask) == expected)
            return 0;
        cpu_relax();
    }
    return -1;
}

static inline void hba_write32(uint32_t offset, uint32_t val)
{
    mmio_write32(HBA_REG(g_abar, offset), val);
}

static inline uint32_t hba_read32(uint32_t offset)
{
    return mmio_read32(HBA_REG(g_abar, offset));
}

static inline void port_write32(volatile void *pb, uint32_t off, uint32_t val)
{
    mmio_write32(PORT_REG(pb, off), val);
}

static inline uint32_t port_read32(volatile void *pb, uint32_t off)
{
    return mmio_read32(PORT_REG(pb, off));
}
static void ata_copy_string(char *dst, const uint8_t *src, uint32_t words)
{
    uint32_t i;
    for (i = 0; i < words; i++) {
        uint16_t w;
        w = (uint16_t)((src[i*2] << 8u) | src[i*2 + 1u]);
        dst[i*2]     = (char)((w >> 8u) & 0xFFu);
        dst[i*2 + 1] = (char)(w & 0xFFu);
    }
    dst[words * 2] = '\0';
    int len = (int)(words * 2);
    while (len > 0 && dst[len-1] == ' ')
        dst[--len] = '\0';
}

static void ahci_memzero(void *dst, size_t n)
{
    uint8_t *p = (uint8_t *)dst;
    size_t i;
    for (i = 0; i < n; i++)
        p[i] = 0;
}

static void ahci_bios_handoff(void)
{
    uint32_t cap2 = hba_read32(AHCI_HBA_CAP2);
    if (!(cap2 & AHCI_CAP2_BOH))
        return;

    uint32_t bohc = hba_read32(AHCI_HBA_BOHC);

    if (!(bohc & AHCI_BOHC_SOO)) {
        hba_write32(AHCI_HBA_BOHC, bohc | AHCI_BOHC_OOH);
        return;
    }

    hba_write32(AHCI_HBA_BOHC, bohc | AHCI_BOHC_OOH);

    uint32_t i;
    for (i = 0; i < 250000u; i++) {
        if (!(hba_read32(AHCI_HBA_BOHC) & AHCI_BOHC_SOO))
            break;
        cpu_relax();
    }

    for (i = 0; i < 2000000u; i++) {
        if (!(hba_read32(AHCI_HBA_BOHC) & AHCI_BOHC_BB))
            break;
        cpu_relax();
    }
}

static int ahci_hba_reset(void)
{
    hba_write32(AHCI_HBA_GHC, AHCI_GHC_HR);
    if (ahci_spin_while((const volatile uint32_t *)HBA_REG(g_abar, AHCI_HBA_GHC),
                        AHCI_GHC_HR, 0, 1000000u) != 0) {
        klog_fail("ahci", "HBA reset timed out");
        return -1;
    }
    return 0;
}
static int ahci_port_stop(volatile void *pb)
{
    uint32_t cmd = port_read32(pb, AHCI_PORT_CMD);

    cmd &= ~AHCI_CMD_ST;
    port_write32(pb, AHCI_PORT_CMD, cmd);

    if (ahci_spin_while((const volatile uint32_t *)PORT_REG(pb, AHCI_PORT_CMD),
                        AHCI_CMD_CR, 0, 500000u) != 0) {
        klog_warn("ahci", "port: CMD.CR did not clear after ST=0");
    }
    cmd = port_read32(pb, AHCI_PORT_CMD);
    cmd &= ~AHCI_CMD_FRE;
    port_write32(pb, AHCI_PORT_CMD, cmd);

    if (ahci_spin_while((const volatile uint32_t *)PORT_REG(pb, AHCI_PORT_CMD),
                        AHCI_CMD_FR, 0, 500000u) != 0) {
        klog_warn("ahci", "port: CMD.FR did not clear after FRE=0");
    }

    return 0;
}

static void ahci_port_start(volatile void *pb)
{
    uint32_t cmd;

    ahci_spin_while((const volatile uint32_t *)PORT_REG(pb, AHCI_PORT_CMD),
                    AHCI_CMD_CR, 0, 500000u);

    cmd = port_read32(pb, AHCI_PORT_CMD);
    cmd |= AHCI_CMD_FRE | AHCI_CMD_ST;
    port_write32(pb, AHCI_PORT_CMD, cmd);
}

static void ahci_build_h2d_fis(uint8_t *cfis,
                                uint8_t  command,
                                uint64_t lba,
                                uint16_t count,
                                int      is_write)
{
    (void)is_write;
    ahci_memzero(cfis, 20);

    cfis[0]  = FIS_TYPE_H2D;
    cfis[1]  = FIS_H2D_C;
    cfis[2]  = command;
    cfis[3]  = 0;

    cfis[4]  = (uint8_t)(lba & 0xFFu);
    cfis[5]  = (uint8_t)((lba >>  8u) & 0xFFu);
    cfis[6]  = (uint8_t)((lba >> 16u) & 0xFFu);
    cfis[7]  = (uint8_t)(ATA_DEV_LBA | 0x20u);

    cfis[8]  = (uint8_t)((lba >> 24u) & 0xFFu);
    cfis[9]  = (uint8_t)((lba >> 32u) & 0xFFu);
    cfis[10] = (uint8_t)((lba >> 40u) & 0xFFu);
    cfis[11] = 0;
    cfis[12] = (uint8_t)(count & 0xFFu);
    cfis[13] = (uint8_t)((count >> 8u) & 0xFFu);

    cfis[14] = 0;
    cfis[15] = 0;
}

static int ahci_issue_and_poll(volatile void *pb, int slot)
{
    port_write32(pb, AHCI_PORT_IS,
                 port_read32(pb, AHCI_PORT_IS));

    port_write32(pb, AHCI_PORT_CI, 1u << (uint32_t)slot);

    uint32_t i;
    for (i = 0; i < AHCI_SPIN_LIMIT; i++) {
        uint32_t ci  = port_read32(pb, AHCI_PORT_CI);
        uint32_t is  = port_read32(pb, AHCI_PORT_IS);
        uint32_t tfd = port_read32(pb, AHCI_PORT_TFD);

        if (is & AHCI_IS_FATAL_MASK) {
            klog_fail("ahci", "command: fatal interrupt (TFES/HBFS/HBDS/IFS)");
            port_write32(pb, AHCI_PORT_IS, is);
            return -1;
        }

        if (tfd & AHCI_TFD_ERR) {
            klog_fail("ahci", "command: ATA task file error");
            return -1;
        }

        if (!(ci & (1u << (uint32_t)slot)))
            return 0;

        cpu_relax();
    }

    klog_fail("ahci", "command: polling timeout");
    return -1;
}

static int ahci_identify(int port_idx, int atapi)
{
    ahci_port_t *p    = &g_ports[port_idx];
    volatile void *pb = p->port_regs;
    int slot = 0;

    ahci_memzero(g_identify_buf, 512);

    ahci_cmd_tbl_t *tbl = &g_cmd_tbl[port_idx][slot];
    ahci_memzero(tbl, sizeof(*tbl));

    tbl->prdt[0].dba  = (uint32_t)((uint64_t)(uintptr_t)g_identify_buf & 0xFFFFFFFFu);
    tbl->prdt[0].dbau = (uint32_t)((uint64_t)(uintptr_t)g_identify_buf >> 32u);
    tbl->prdt[0].dbc  = AHCI_PRDT_DBC(512u) | AHCI_PRDT_I;

    uint8_t cmd = atapi ? ATA_CMD_IDENTIFY_PACKET : ATA_CMD_IDENTIFY;
    ahci_build_h2d_fis(tbl->cfis, cmd, 0, 0, 0);

    ahci_cmd_hdr_t *hdr = &g_cmd_list[port_idx][slot];
    hdr->flags = (uint16_t)(AHCI_CH_CFL(5u));
    hdr->prdtl = 1u;
    hdr->prdbc = 0u;
    hdr->ctba  = (uint32_t)((uint64_t)(uintptr_t)tbl & 0xFFFFFFFFu);
    hdr->ctbau = (uint32_t)((uint64_t)(uintptr_t)tbl >> 32u);

    compiler_barrier();
    return ahci_issue_and_poll(pb, slot);
}

static void ahci_parse_identify(int port_idx)
{
    ahci_port_t *p    = &g_ports[port_idx];
    const uint8_t *id = g_identify_buf;

    uint16_t gen;
    gen  = (uint16_t)id[ATA_IDENT_GENERAL];
    gen |= (uint16_t)((uint16_t)id[ATA_IDENT_GENERAL + 1u] << 8u);
    p->atapi = (gen & ATA_IDENT_GEN_ATAPI) ? 1 : 0;

    uint64_t lba48 = 0;
    lba48 |= (uint64_t)id[ATA_IDENT_LBA48_0]
           | ((uint64_t)id[ATA_IDENT_LBA48_0 + 1u] << 8u);
    lba48 |= ((uint64_t)id[ATA_IDENT_LBA48_1]
           | ((uint64_t)id[ATA_IDENT_LBA48_1 + 1u] << 8u)) << 16u;
    lba48 |= ((uint64_t)id[ATA_IDENT_LBA48_2]
           | ((uint64_t)id[ATA_IDENT_LBA48_2 + 1u] << 8u)) << 32u;
    lba48 |= ((uint64_t)id[ATA_IDENT_LBA48_3]
           | ((uint64_t)id[ATA_IDENT_LBA48_3 + 1u] << 8u)) << 48u;

    if (lba48 == 0u) {
        uint32_t lba28;
        lba28  = (uint32_t)id[ATA_IDENT_LBA28_LO]
               | ((uint32_t)id[ATA_IDENT_LBA28_LO + 1u] << 8u);
        lba28 |= ((uint32_t)id[ATA_IDENT_LBA28_HI]
               | ((uint32_t)id[ATA_IDENT_LBA28_HI + 1u] << 8u)) << 16u;
        p->num_sectors = lba28;
    } else {
        p->num_sectors = lba48;
    }

    uint16_t phy_sect;
    phy_sect  = (uint16_t)id[ATA_IDENT_PHY_SECT];
    phy_sect |= (uint16_t)((uint16_t)id[ATA_IDENT_PHY_SECT + 1u] << 8u);

    if ((phy_sect & 0xC000u) == 0x4000u && (phy_sect & (1u << 12u))) {
        uint32_t lss;
        lss  = (uint32_t)id[ATA_IDENT_SECTOR_SIZE]
             | ((uint32_t)id[ATA_IDENT_SECTOR_SIZE + 1u] << 8u);
        lss |= ((uint32_t)id[ATA_IDENT_SECTOR_SIZE + 2u]
             | ((uint32_t)id[ATA_IDENT_SECTOR_SIZE + 3u] << 8u)) << 16u;
        p->sector_size = lss * 2u;
    } else {
        p->sector_size = 512u;
    }
    ata_copy_string(p->model,  id + ATA_IDENT_MODEL,  20u);
    ata_copy_string(p->serial, id + ATA_IDENT_SERIAL, 10u);
}

static int ahci_port_init(int port_idx, uint32_t num_slots)
{
    volatile void *pb = PORT_BASE(g_abar, (uint32_t)port_idx);
    ahci_port_t   *p  = &g_ports[port_idx];
    p->port_regs = pb;

    uint32_t ssts = port_read32(pb, AHCI_PORT_SSTS);
    if ((ssts & AHCI_SSTS_DET_MASK) != AHCI_SSTS_DET_CONN)
        return 0;

    if ((ssts & AHCI_SSTS_IPM_MASK) != AHCI_SSTS_IPM_ACT)
        return 0;

    ahci_port_stop(pb);

    uint64_t clb_pa = (uint64_t)(uintptr_t)g_cmd_list[port_idx];
    port_write32(pb, AHCI_PORT_CLB,  (uint32_t)(clb_pa & 0xFFFFFFFFu));
    port_write32(pb, AHCI_PORT_CLBU, (uint32_t)(clb_pa >> 32u));

    uint64_t fb_pa = (uint64_t)(uintptr_t)&g_recv_fis[port_idx];
    port_write32(pb, AHCI_PORT_FB,   (uint32_t)(fb_pa & 0xFFFFFFFFu));
    port_write32(pb, AHCI_PORT_FBU,  (uint32_t)(fb_pa >> 32u));

    uint32_t s;
    for (s = 0; s < num_slots; s++) {
        ahci_cmd_hdr_t *hdr = &g_cmd_list[port_idx][s];
        ahci_memzero(hdr, sizeof(*hdr));
        uint64_t tbl_pa = (uint64_t)(uintptr_t)&g_cmd_tbl[port_idx][s];
        hdr->ctba  = (uint32_t)(tbl_pa & 0xFFFFFFFFu);
        hdr->ctbau = (uint32_t)(tbl_pa >> 32u);
        hdr->prdtl = 0;
    }

    port_write32(pb, AHCI_PORT_SERR,
                 port_read32(pb, AHCI_PORT_SERR));
    port_write32(pb, AHCI_PORT_IS,
                 port_read32(pb, AHCI_PORT_IS));

    uint32_t cap = hba_read32(AHCI_HBA_CAP);
    if (cap & AHCI_CAP_SSS) {
        uint32_t cmd_reg = port_read32(pb, AHCI_PORT_CMD);
        cmd_reg |= AHCI_CMD_SUD | AHCI_CMD_POD;
        port_write32(pb, AHCI_PORT_CMD, cmd_reg);
        uint32_t spin;
        for (spin = 0; spin < 2000000u; spin++) {
            ssts = port_read32(pb, AHCI_PORT_SSTS);
            if ((ssts & AHCI_SSTS_DET_MASK) == AHCI_SSTS_DET_CONN)
                break;
            cpu_relax();
        }
    }

    uint32_t cmd_reg = port_read32(pb, AHCI_PORT_CMD);
    cmd_reg = (cmd_reg & ~AHCI_CMD_ICC_MASK) | AHCI_CMD_ICC_ACTIVE;
    port_write32(pb, AHCI_PORT_CMD, cmd_reg);

    ahci_port_start(pb);

    if (ahci_spin_while((const volatile uint32_t *)PORT_REG(pb, AHCI_PORT_TFD),
                        AHCI_TFD_BSY | AHCI_TFD_DRQ, 0, 500000u) != 0) {
        klog_warn("ahci", "port: device BSY/DRQ did not clear");
    }

    p->num_slots = num_slots;

    uint32_t sig = port_read32(pb, AHCI_PORT_SIG);
    if (sig == AHCI_SIG_SATAPI) {
        klog_info("ahci", "port: SATAPI device (limited support)");
        p->atapi = 1;
    } else if (sig != AHCI_SIG_SATA && sig != 0xFFFFFFFFu) {
        klog_warn("ahci", "port: unrecognised signature — skipping");
        ahci_port_stop(pb);
        return 0;
    }

    if (ahci_identify(port_idx, p->atapi) != 0) {
        klog_fail("ahci", "IDENTIFY DEVICE failed");
        ahci_port_stop(pb);
        return -1;
    }

    ahci_parse_identify(port_idx);
    p->valid = 1;
    return 1;
}

static int ahci_blkdev_read(void *ctx, uint64_t lba, uint32_t count, void *buf)
{
    return ahci_read((int)(uintptr_t)ctx, lba, count, buf);
}

static int ahci_blkdev_write(void *ctx, uint64_t lba, uint32_t count, const void *buf)
{
    return ahci_write((int)(uintptr_t)ctx, lba, count, buf);
}

int ahci_init(void)
{
    pci_device_t *dev = pci_find_class(AHCI_PCI_CLASS, AHCI_PCI_SUBCLASS);
    if (!dev) {
        klog_warn("ahci", "no AHCI controller found on PCI bus");
        return -1;
    }

    pci_enable_device(dev);

    uintptr_t abar_pa = pci_bar_base(dev, AHCI_BAR_IDX);
    if (abar_pa == 0) {
        klog_fail("ahci", "BAR5 (ABAR) is zero or I/O space");
        return -1;
    }
    g_abar = (volatile void *)abar_pa;

    klog_ok("ahci", "AHCI controller found, ABAR mapped");

    ahci_bios_handoff();

    if (ahci_hba_reset() != 0)
        return -1;

    hba_write32(AHCI_HBA_GHC, AHCI_GHC_AE);
    compiler_barrier();

    uint32_t cap     = hba_read32(AHCI_HBA_CAP);
    uint32_t pi      = hba_read32(AHCI_HBA_PI);
    uint32_t num_slots = ((cap & AHCI_CAP_NCS_MASK) >> AHCI_CAP_NCS_SHIFT) + 1u;

    if (num_slots > AHCI_MAX_CMD_SLOTS)
        num_slots = AHCI_MAX_CMD_SLOTS;

    klog_ok("ahci", "HBA reset complete, enumerating ports");

    int drives = 0;
    uint32_t p;
    for (p = 0; p < AHCI_MAX_PORTS; p++) {
        if (!(pi & (1u << (uint32_t)p)))
            continue;

        int rc = ahci_port_init((int)p, num_slots);
        if (rc <= 0)
            continue;
        if (g_ports[p].atapi)
            continue;

        char name[16];
        name[0]  = 'a'; name[1] = 'h'; name[2] = 'c'; name[3] = 'i';
        name[4]  = (char)('0' + drives);
        name[5]  = '\0';

        int idx = blkdev_register(name,
                                  (void *)(uintptr_t)p,
                                  ahci_blkdev_read,
                                  ahci_blkdev_write,
                                  g_ports[p].num_sectors,
                                  g_ports[p].sector_size);
        if (idx < 0) {
            klog_warn("ahci", "blkdev table full — drive not registered");
        } else {
            klog_ok("ahci", "SATA drive registered with blkdev");
            drives++;
        }
    }

    g_num_drives  = drives;
    g_initialised = 1;

    if (drives == 0)
        klog_info("ahci", "no SATA drives found");
    else
        klog_ok("ahci", "AHCI init complete");

    return drives;
}

static int ahci_do_rw(int port_idx, uint64_t lba, uint32_t count,
                      void *buf, int write)
{
    if (!g_initialised || port_idx < 0 || (uint32_t)port_idx >= AHCI_MAX_PORTS)
        return -1;

    ahci_port_t *p = &g_ports[port_idx];
    if (!p->valid)
        return -1;

    if (buf == NULL || count == 0)
        return -1;

    if (lba + count > p->num_sectors)
        return -1;

    volatile void *pb = p->port_regs;

    int slot = 0;

    uint32_t max_sectors = (4u * 1024u * 1024u) / p->sector_size;
    if (count > max_sectors)
        count = max_sectors;

    uint32_t byte_count = count * p->sector_size;

    ahci_cmd_tbl_t *tbl = &g_cmd_tbl[port_idx][slot];
    ahci_memzero(tbl, sizeof(*tbl));

    uint64_t buf_pa = (uint64_t)(uintptr_t)buf;
    tbl->prdt[0].dba  = (uint32_t)(buf_pa & 0xFFFFFFFFu);
    tbl->prdt[0].dbau = (uint32_t)(buf_pa >> 32u);
    tbl->prdt[0].dbc  = AHCI_PRDT_DBC(byte_count) | AHCI_PRDT_I;

    uint8_t cmd = write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    ahci_build_h2d_fis(tbl->cfis, cmd, lba, (uint16_t)count, write);

    ahci_cmd_hdr_t *hdr = &g_cmd_list[port_idx][slot];
    uint16_t flags = (uint16_t)(AHCI_CH_CFL(5u));
    if (write) flags |= (uint16_t)AHCI_CH_WRITE;
    hdr->flags = flags;
    hdr->prdtl = 1u;
    hdr->prdbc = 0u;

    compiler_barrier();
    return ahci_issue_and_poll(pb, slot);
}

int ahci_read(int port_idx, uint64_t lba, uint32_t count, void *buf)
{
    return ahci_do_rw(port_idx, lba, count, buf, 0);
}

int ahci_write(int port_idx, uint64_t lba, uint32_t count, const void *buf)
{
    return ahci_do_rw(port_idx, lba, count, (void *)buf, 1);
}
