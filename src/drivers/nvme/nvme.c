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

#include "nvme.h"
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
static inline uint64_t mmio_read64(const volatile void *addr)
{
    return *(const volatile uint64_t *)addr;
}
static inline void mmio_write64(volatile void *addr, uint64_t val)
{
    *(volatile uint64_t *)addr = val;
}

#define NVME_REG(base, off)  ((volatile void *)((uintptr_t)(base) + (off)))

static nvme_sqe_t g_admin_sq[NVME_ADMIN_QUEUE_DEPTH]
    __attribute__((aligned(PAGE_SIZE)));

static nvme_cqe_t g_admin_cq[NVME_ADMIN_QUEUE_DEPTH]
    __attribute__((aligned(PAGE_SIZE)));

static nvme_sqe_t g_iosq[NVME_IOQ_DEPTH]
    __attribute__((aligned(PAGE_SIZE)));

static nvme_cqe_t g_iocq[NVME_IOQ_DEPTH]
    __attribute__((aligned(PAGE_SIZE)));

static uint8_t g_identify_buf[4096]
    __attribute__((aligned(PAGE_SIZE)));

static volatile void *g_bar0    = NULL;
static uint32_t       g_dstrd   = 0;
static uint32_t       g_nsid    = 1;
static uint32_t       g_lba_shift = 9;
static uint64_t       g_ns_sectors = 0;
static uint32_t       g_lba_size   = 512;
static int            g_initialised = 0;

static uint32_t g_admin_sq_tail = 0;
static uint32_t g_admin_cq_head = 0;
static uint8_t  g_admin_cq_phase = 1;

static uint32_t g_iosq_tail  = 0;
static uint32_t g_iocq_head  = 0;
static uint8_t  g_iocq_phase = 1;

static uint16_t g_cid = 0;

static volatile uint32_t *nvme_sq_doorbell(uint16_t qid)
{
    uintptr_t off = NVME_DOORBELL_BASE +
                    (uintptr_t)(2u * qid) * g_dstrd;
    return (volatile uint32_t *)((uintptr_t)g_bar0 + off);
}

static volatile uint32_t *nvme_cq_doorbell(uint16_t qid)
{
    uintptr_t off = NVME_DOORBELL_BASE +
                    (uintptr_t)(2u * qid + 1u) * g_dstrd;
    return (volatile uint32_t *)((uintptr_t)g_bar0 + off);
}

static void nvme_memzero(void *dst, size_t n)
{
    uint8_t *p = (uint8_t *)dst;
    size_t i;
    for (i = 0; i < n; i++)
        p[i] = 0;
}

static uint64_t le64_at(const uint8_t *buf, size_t off)
{
    uint64_t v = 0;
    int i;
    for (i = 7; i >= 0; i--)
        v = (v << 8u) | buf[off + (size_t)i];
    return v;
}

static uint32_t le32_at(const uint8_t *buf, size_t off)
{
    return (uint32_t)buf[off]
         | ((uint32_t)buf[off+1] << 8u)
         | ((uint32_t)buf[off+2] << 16u)
         | ((uint32_t)buf[off+3] << 24u);
}

static uint32_t nvme_admin_submit(const nvme_sqe_t *sqe)
{

    uint32_t slot = g_admin_sq_tail;
    g_admin_sq[slot] = *sqe;
    compiler_barrier();

    g_admin_sq_tail = (g_admin_sq_tail + 1u) % NVME_ADMIN_QUEUE_DEPTH;

    *nvme_sq_doorbell(0) = g_admin_sq_tail;

    uint32_t limit = 2000000u;
    uint32_t i;
    for (i = 0; i < limit; i++) {
        volatile nvme_cqe_t *cqe = &g_admin_cq[g_admin_cq_head];
        uint16_t sf = cqe->sf;

        if ((sf & NVME_CQE_SF_P) != g_admin_cq_phase) {
            cpu_relax();
            continue;
        }

        uint32_t result = cqe->cdw0;
        uint8_t  sc     = (uint8_t)NVME_CQE_SF_SC(sf);
        uint8_t  sct    = (uint8_t)NVME_CQE_SF_SCT(sf);

        g_admin_cq_head++;
        if (g_admin_cq_head >= NVME_ADMIN_QUEUE_DEPTH) {
            g_admin_cq_head = 0;
            g_admin_cq_phase ^= 1u;
        }

        *nvme_cq_doorbell(0) = g_admin_cq_head;

        if (sct != 0 || sc != NVME_SC_SUCCESS) {
            klog_fail("nvme", "admin command: non-zero status");
            return ~0u;
        }

        return result;
    }

    klog_fail("nvme", "admin command: completion poll timeout");
    return ~0u;
}

static int nvme_ioq_submit(const nvme_sqe_t *sqe)
{

    uint32_t slot = g_iosq_tail;
    g_iosq[slot] = *sqe;
    compiler_barrier();

    g_iosq_tail = (g_iosq_tail + 1u) % NVME_IOQ_DEPTH;

    *nvme_sq_doorbell(NVME_IOQ_ID) = g_iosq_tail;

    uint32_t limit = 4000000u;
    uint32_t i;
    for (i = 0; i < limit; i++) {
        volatile nvme_cqe_t *cqe = &g_iocq[g_iocq_head];
        uint16_t sf = cqe->sf;

        if ((sf & NVME_CQE_SF_P) != g_iocq_phase) {
            cpu_relax();
            continue;
        }

        uint8_t sc  = (uint8_t)NVME_CQE_SF_SC(sf);
        uint8_t sct = (uint8_t)NVME_CQE_SF_SCT(sf);

        g_iocq_head++;
        if (g_iocq_head >= NVME_IOQ_DEPTH) {
            g_iocq_head = 0;
            g_iocq_phase ^= 1u;
        }

        *nvme_cq_doorbell(NVME_IOQ_ID) = g_iocq_head;

        if (sct != 0 || sc != NVME_SC_SUCCESS) {
            klog_fail("nvme", "I/O command: non-zero status");
            return -1;
        }

        return 0;
    }

    klog_fail("nvme", "I/O command: completion poll timeout");
    return -1;
}

static nvme_sqe_t nvme_make_sqe(uint8_t opcode, uint32_t nsid)
{
    nvme_sqe_t sqe;
    nvme_memzero(&sqe, sizeof(sqe));
    uint16_t cid = g_cid++;
    if (g_cid == 0) g_cid = 1;
    sqe.cdw0 = NVME_CDW0_OPC(opcode) | NVME_CDW0_FUSE_NONE |
               NVME_CDW0_PSDT_PRP   | NVME_CDW0_CID(cid);
    sqe.nsid = nsid;
    return sqe;
}

static int nvme_identify_controller(void)
{
    nvme_memzero(g_identify_buf, 4096);

    nvme_sqe_t sqe = nvme_make_sqe(NVME_ADMIN_IDENTIFY, 0);
    sqe.prp1  = (uint64_t)(uintptr_t)g_identify_buf;
    sqe.prp2  = 0;
    sqe.cdw10 = NVME_IDENTIFY_CNS_CTRL;

    if (nvme_admin_submit(&sqe) == ~0u)
        return -1;

    return 0;
}

static int nvme_identify_namespace(uint32_t nsid)
{
    nvme_memzero(g_identify_buf, 4096);

    nvme_sqe_t sqe = nvme_make_sqe(NVME_ADMIN_IDENTIFY, nsid);
    sqe.prp1  = (uint64_t)(uintptr_t)g_identify_buf;
    sqe.prp2  = 0;
    sqe.cdw10 = NVME_IDENTIFY_CNS_NS;

    if (nvme_admin_submit(&sqe) == ~0u)
        return -1;

    g_ns_sectors = le64_at(g_identify_buf, NVME_ID_NS_NSZE);

    uint8_t flbas    = g_identify_buf[NVME_ID_NS_FLBAS];
    uint8_t lbaf_idx = flbas & 0x0Fu;

    uint32_t lbaf = le32_at(g_identify_buf,
                            NVME_ID_NS_LBAF_BASE + (size_t)lbaf_idx * 4u);
    uint8_t lbads = (uint8_t)NVME_LBAF_LBADS(lbaf);

    if (lbads < 9u || lbads > 16u) {
        klog_fail("nvme", "namespace: unsupported LBA data size");
        return -1;
    }

    g_lba_shift = lbads;
    g_lba_size  = 1u << lbads;

    return 0;
}

static int nvme_create_iocq(void)
{
    nvme_sqe_t sqe = nvme_make_sqe(NVME_ADMIN_CREATE_CQ, 0);
    sqe.prp1  = (uint64_t)(uintptr_t)g_iocq;
    sqe.cdw10 = NVME_Q_CDW10(NVME_IOQ_ID, NVME_IOQ_DEPTH);

    sqe.cdw11 = NVME_CQ_CDW11_PC;

    if (nvme_admin_submit(&sqe) == ~0u) {
        klog_fail("nvme", "Create I/O CQ failed");
        return -1;
    }
    klog_ok("nvme", "I/O Completion Queue created");
    return 0;
}

static int nvme_create_iosq(void)
{
    nvme_sqe_t sqe = nvme_make_sqe(NVME_ADMIN_CREATE_SQ, 0);
    sqe.prp1  = (uint64_t)(uintptr_t)g_iosq;
    sqe.cdw10 = NVME_Q_CDW10(NVME_IOQ_ID, NVME_IOQ_DEPTH);

    sqe.cdw11 = NVME_SQ_CDW11_PC |
                NVME_SQ_CDW11_QPRIO_MEDIUM |
                NVME_SQ_CDW11_CQID(NVME_IOQ_ID);

    if (nvme_admin_submit(&sqe) == ~0u) {
        klog_fail("nvme", "Create I/O SQ failed");
        return -1;
    }
    klog_ok("nvme", "I/O Submission Queue created");
    return 0;
}

static int nvme_blkdev_read(void *ctx, uint64_t lba, uint32_t count, void *buf)
{
    uint32_t nsid = (uint32_t)(uintptr_t)ctx;
    return nvme_read(nsid, lba, count, buf);
}

static int nvme_blkdev_write(void *ctx, uint64_t lba, uint32_t count,
                              const void *buf)
{
    uint32_t nsid = (uint32_t)(uintptr_t)ctx;
    return nvme_write(nsid, lba, count, buf);
}

int nvme_init(void)
{

    pci_device_t *dev = pci_find_class(NVME_PCI_CLASS, NVME_PCI_SUBCLASS);
    if (!dev) {
        klog_info("nvme", "no NVMe controller found on PCI bus");
        return -1;
    }

    pci_enable_device(dev);

    uintptr_t bar0_pa = pci_bar_base(dev, NVME_BAR_IDX);
    if (bar0_pa == 0) {
        klog_fail("nvme", "BAR0 is zero or I/O space");
        return -1;
    }
    g_bar0 = (volatile void *)bar0_pa;

    klog_ok("nvme", "NVMe controller found, BAR0 mapped");

    uint64_t cap = mmio_read64(NVME_REG(g_bar0, NVME_REG_CAP));

    g_dstrd = (uint32_t)(4u << NVME_CAP_DSTRD(cap));

    uint32_t to_ms = (uint32_t)NVME_CAP_TO(cap) * 500u;
    if (to_ms == 0) to_ms = 5000u;
    uint32_t rdy_limit = to_ms * 200u;

    uint32_t cc = mmio_read32(NVME_REG(g_bar0, NVME_REG_CC));
    if (cc & NVME_CC_EN) {
        cc &= ~NVME_CC_EN;
        mmio_write32(NVME_REG(g_bar0, NVME_REG_CC), cc);
    }

    uint32_t i;
    for (i = 0; i < rdy_limit; i++) {
        if (!(mmio_read32(NVME_REG(g_bar0, NVME_REG_CSTS)) & NVME_CSTS_RDY))
            break;
        cpu_relax();
    }
    if (mmio_read32(NVME_REG(g_bar0, NVME_REG_CSTS)) & NVME_CSTS_RDY) {
        klog_fail("nvme", "controller did not quiesce (CSTS.RDY stuck at 1)");
        return -1;
    }

    nvme_memzero(g_admin_sq, sizeof(g_admin_sq));
    nvme_memzero(g_admin_cq, sizeof(g_admin_cq));
    nvme_memzero(g_iosq,     sizeof(g_iosq));
    nvme_memzero(g_iocq,     sizeof(g_iocq));

    g_admin_sq_tail = 0;
    g_admin_cq_head = 0;
    g_admin_cq_phase = 1;
    g_iosq_tail  = 0;
    g_iocq_head  = 0;
    g_iocq_phase = 1;
    g_cid = 1;

    mmio_write32(NVME_REG(g_bar0, NVME_REG_AQA),
                 NVME_AQA(NVME_ADMIN_QUEUE_DEPTH, NVME_ADMIN_QUEUE_DEPTH));

    mmio_write64(NVME_REG(g_bar0, NVME_REG_ASQ),
                 (uint64_t)(uintptr_t)g_admin_sq);

    mmio_write64(NVME_REG(g_bar0, NVME_REG_ACQ),
                 (uint64_t)(uintptr_t)g_admin_cq);

    cc = NVME_CC_CSS_NVM        |
         NVME_CC_MPS(0u)        |
         NVME_CC_AMS_RR         |
         NVME_CC_SHN_NONE       |
         NVME_CC_IOSQES(NVME_SQ_ENTRY_SHIFT) |
         NVME_CC_IOCQES(NVME_CQ_ENTRY_SHIFT) |
         NVME_CC_EN;

    memory_barrier();
    mmio_write32(NVME_REG(g_bar0, NVME_REG_CC), cc);

    for (i = 0; i < rdy_limit; i++) {
        uint32_t csts = mmio_read32(NVME_REG(g_bar0, NVME_REG_CSTS));
        if (csts & NVME_CSTS_CFS) {
            klog_fail("nvme", "controller fatal status after enable");
            return -1;
        }
        if (csts & NVME_CSTS_RDY)
            break;
        cpu_relax();
    }
    if (!(mmio_read32(NVME_REG(g_bar0, NVME_REG_CSTS)) & NVME_CSTS_RDY)) {
        klog_fail("nvme", "controller did not become ready (CSTS.RDY timeout)");
        return -1;
    }

    klog_ok("nvme", "controller enabled and ready");

    if (nvme_identify_controller() != 0) {
        klog_fail("nvme", "Identify Controller failed");
        return -1;
    }
    klog_ok("nvme", "Identify Controller succeeded");

    g_nsid = 1;
    if (nvme_identify_namespace(g_nsid) != 0) {
        klog_fail("nvme", "Identify Namespace 1 failed");
        return -1;
    }
    klog_ok("nvme", "Identify Namespace succeeded");

    if (nvme_create_iocq() != 0)
        return -1;

    if (nvme_create_iosq() != 0)
        return -1;

    int idx = blkdev_register("nvme0",
                              (void *)(uintptr_t)g_nsid,
                              nvme_blkdev_read,
                              nvme_blkdev_write,
                              g_ns_sectors,
                              g_lba_size);
    if (idx < 0) {
        klog_warn("nvme", "blkdev table full — NVMe namespace not registered");
    } else {
        klog_ok("nvme", "NVMe namespace registered with blkdev");
    }

    g_initialised = 1;
    klog_ok("nvme", "NVMe init complete");
    return (idx >= 0) ? 1 : 0;
}

static int nvme_do_rw(uint32_t nsid, uint64_t slba, uint32_t nlb,
                      void *buf, int write)
{
    if (!g_initialised || buf == NULL || nlb == 0)
        return -1;

    if (slba + nlb > g_ns_sectors)
        return -1;

    uint64_t byte_count = (uint64_t)nlb << g_lba_shift;

    uint64_t buf_pa  = (uint64_t)(uintptr_t)buf;
    uint32_t page_sz = PAGE_SIZE;
    uint64_t offset_in_page = buf_pa & (uint64_t)(page_sz - 1u);
    uint64_t first_page_bytes = (uint64_t)page_sz - offset_in_page;

    uint64_t prp1 = buf_pa;
    uint64_t prp2 = 0;

    if (byte_count > first_page_bytes) {
        uint64_t remaining = byte_count - first_page_bytes;
        if (remaining > (uint64_t)page_sz) {
            klog_fail("nvme", "I/O request spans more than 2 pages");
            return -1;
        }

        prp2 = (buf_pa & ~(uint64_t)(page_sz - 1u)) + (uint64_t)page_sz;
    }

    nvme_sqe_t sqe = nvme_make_sqe(
        write ? NVME_IO_WRITE : NVME_IO_READ,
        nsid);

    sqe.prp1  = prp1;
    sqe.prp2  = prp2;

    sqe.cdw10 = (uint32_t)(slba & 0xFFFFFFFFu);
    sqe.cdw11 = (uint32_t)(slba >> 32u);

    sqe.cdw12 = (nlb - 1u) & 0xFFFFu;

    return nvme_ioq_submit(&sqe);
}

int nvme_read(uint32_t nsid, uint64_t slba, uint32_t count, void *buf)
{
    return nvme_do_rw(nsid, slba, count, buf, 0);
}

int nvme_write(uint32_t nsid, uint64_t slba, uint32_t count, const void *buf)
{

    return nvme_do_rw(nsid, slba, count, (void *)buf, 1);
}
