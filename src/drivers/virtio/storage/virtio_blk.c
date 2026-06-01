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

#include "virtio_blk.h"
#include <virtio.h>
#include <pci.h>
#include <blkdev.h>
#include <kmalloc.h>
#include <kernel.h>
#include <kfmt.h>

static inline uint8_t mmio_read8(const volatile void *addr)
{
    return *(const volatile uint8_t *)addr;
}
static inline void mmio_write8(volatile void *addr, uint8_t val)
{
    *(volatile uint8_t *)addr = val;
}
static inline uint16_t mmio_read16(const volatile void *addr)
{
    return *(const volatile uint16_t *)addr;
}
static inline void mmio_write16(volatile void *addr, uint16_t val)
{
    *(volatile uint16_t *)addr = val;
}
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

static virtq_desc_t vblk_desc[VIRTQ_SIZE]
    __attribute__((aligned(4096)));

static uint8_t vblk_avail_raw[VIRTQ_AVAIL_BYTES + 2]
    __attribute__((aligned(4096)));

static uint8_t vblk_used_raw[VIRTQ_USED_BYTES + 2]
    __attribute__((aligned(4096)));

static virtio_blk_req_hdr_t g_req_hdrs[VIRTQ_SIZE]
    __attribute__((aligned(16)));

static uint8_t g_req_status[VIRTQ_SIZE]
    __attribute__((aligned(1)));

static volatile virtio_common_cfg_t  *g_common      = NULL;
static volatile virtio_blk_config_t  *g_blk_cfg     = NULL;
static volatile uint8_t              *g_isr          = NULL;
static volatile uint8_t              *g_notify_base  = NULL;
static uint32_t                       g_notify_mult  = 0;

static virtqueue_t  g_vq;
static uint64_t     g_capacity    = 0;
static uint32_t     g_blk_size    = 512;
static int          g_initialised = 0;

static int vblk_map_caps(pci_device_t *dev)
{
    uint16_t status = pci_read_config16(dev, PCI_CFG_STATUS);
    if (!(status & PCI_STS_CAP_LIST)) {
        klog_fail("virtio-blk", "no PCI capability list on device");
        return -1;
    }

    uint8_t cap_off = pci_read_config8(dev, PCI_CFG_CAP_PTR) & 0xFCu;

    while (cap_off != 0u) {
        uint8_t cap_id   = pci_read_config8(dev,  cap_off);
        uint8_t cap_next = pci_read_config8(dev, (uint16_t)(cap_off + 1u));

        if (cap_id != VIRTIO_PCI_CAP_VENDOR_ID) {
            cap_off = cap_next & 0xFCu;
            continue;
        }

        uint8_t  cfg_type = pci_read_config8 (dev, (uint16_t)(cap_off + 3u));
        uint8_t  bar      = pci_read_config8 (dev, (uint16_t)(cap_off + 4u));
        uint32_t region_off = pci_read_config32(dev, (uint16_t)(cap_off + 8u));

        if (bar >= PCI_MAX_BARS) {
            cap_off = cap_next & 0xFCu;
            continue;
        }

        uintptr_t bar_base = dev->bar[bar];
        if (bar_base == 0u) {
            cap_off = cap_next & 0xFCu;
            continue;
        }

        volatile void *region = (volatile void *)(bar_base + region_off);

        switch (cfg_type) {
        case VIRTIO_PCI_CAP_COMMON_CFG:
            g_common = (volatile virtio_common_cfg_t *)region;
            break;

        case VIRTIO_PCI_CAP_DEVICE_CFG:
            g_blk_cfg = (volatile virtio_blk_config_t *)region;
            break;

        case VIRTIO_PCI_CAP_ISR_CFG:
            g_isr = (volatile uint8_t *)region;
            break;

        case VIRTIO_PCI_CAP_NOTIFY_CFG:

            g_notify_base = (volatile uint8_t *)region;
            g_notify_mult = pci_read_config32(dev, (uint16_t)(cap_off + 16u));
            break;

        default:

            break;
        }

        cap_off = cap_next & 0xFCu;
    }

    if (!g_common) {
        klog_fail("virtio-blk", "COMMON_CFG capability not found");
        return -1;
    }
    if (!g_blk_cfg) {
        klog_fail("virtio-blk", "DEVICE_CFG capability not found");
        return -1;
    }
    if (!g_isr) {
        klog_fail("virtio-blk", "ISR_CFG capability not found");
        return -1;
    }
    if (!g_notify_base) {
        klog_fail("virtio-blk", "NOTIFY_CFG capability not found");
        return -1;
    }

    return 0;
}

static int vblk_vq_init(virtqueue_t *vq, uint16_t queue_idx,
                        virtq_desc_t *desc_mem,
                        uint8_t *avail_raw, uint8_t *used_raw)
{

    mmio_write16(&g_common->queue_select, queue_idx);
    compiler_barrier();

    uint16_t qsz = mmio_read16(&g_common->queue_size);
    if (qsz == 0u) {
        klog_fail("virtio-blk", "device reports queue_size = 0");
        return -1;
    }
    if (qsz > VIRTQ_SIZE)
        qsz = (uint16_t)VIRTQ_SIZE;

    mmio_write16(&g_common->queue_size, qsz);

    mmio_write16(&g_common->queue_msix_vector, 0xFFFFu);

    mmio_write64(&g_common->queue_desc,
                 (uint64_t)(uintptr_t)desc_mem);
    mmio_write64(&g_common->queue_driver,
                 (uint64_t)(uintptr_t)avail_raw);
    mmio_write64(&g_common->queue_device,
                 (uint64_t)(uintptr_t)used_raw);

    vq->desc          = desc_mem;
    vq->avail         = (volatile virtq_avail_t *)avail_raw;
    vq->used          = (volatile virtq_used_t  *)used_raw;
    vq->queue_size    = qsz;
    vq->last_used_idx = 0u;
    vq->num_free      = qsz;
    vq->free_head     = 0u;

    uint16_t i;
    for (i = 0u; i < (uint16_t)(qsz - 1u); i++) {
        desc_mem[i].next  = (uint16_t)(i + 1u);
        desc_mem[i].flags = VIRTQ_DESC_F_NEXT;
        desc_mem[i].addr  = 0u;
        desc_mem[i].len   = 0u;
    }
    desc_mem[qsz - 1u].next  = 0u;
    desc_mem[qsz - 1u].flags = 0u;
    desc_mem[qsz - 1u].addr  = 0u;
    desc_mem[qsz - 1u].len   = 0u;

    vq->avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT;
    vq->avail->idx   = 0u;
    vq->used->flags  = 0u;
    vq->used->idx    = 0u;

    uint16_t notify_off = mmio_read16(&g_common->queue_notify_off);
    vq->notify = (volatile uint16_t *)(
        g_notify_base + (uintptr_t)notify_off * (uintptr_t)g_notify_mult);

    compiler_barrier();
    mmio_write16(&g_common->queue_enable, 1u);
    compiler_barrier();

    return 0;
}

static int vblk_do_request(uint32_t type, uint64_t sector,
                            void *buf, uint32_t count)
{
    if (!g_initialised || buf == NULL || count == 0u)
        return -1;

    virtqueue_t *vq = &g_vq;

    if (vq->num_free < 3u) {
        klog_warn("virtio-blk", "virtqueue full — request dropped");
        return -1;
    }

    uint16_t hdr_idx  = vq->free_head;
    vq->free_head     = vq->desc[hdr_idx].next;
    vq->num_free--;

    uint16_t dat_idx  = vq->free_head;
    vq->free_head     = vq->desc[dat_idx].next;
    vq->num_free--;

    uint16_t sts_idx  = vq->free_head;
    vq->free_head     = vq->desc[sts_idx].next;
    vq->num_free--;

    virtio_blk_req_hdr_t *hdr = &g_req_hdrs[hdr_idx];
    hdr->type     = type;
    hdr->reserved = 0u;
    hdr->sector   = sector;

    vq->desc[hdr_idx].addr  = (uint64_t)(uintptr_t)hdr;
    vq->desc[hdr_idx].len   = (uint32_t)sizeof(virtio_blk_req_hdr_t);
    vq->desc[hdr_idx].flags = VIRTQ_DESC_F_NEXT;
    vq->desc[hdr_idx].next  = dat_idx;

    uint32_t byte_count = count * 512u;

    uint16_t dat_flags = VIRTQ_DESC_F_NEXT;
    if (type == VIRTIO_BLK_T_IN)
        dat_flags |= VIRTQ_DESC_F_WRITE;

    vq->desc[dat_idx].addr  = (uint64_t)(uintptr_t)buf;
    vq->desc[dat_idx].len   = byte_count;
    vq->desc[dat_idx].flags = dat_flags;
    vq->desc[dat_idx].next  = sts_idx;

    g_req_status[sts_idx] = (uint8_t)0xFFu;

    vq->desc[sts_idx].addr  = (uint64_t)(uintptr_t)&g_req_status[sts_idx];
    vq->desc[sts_idx].len   = 1u;
    vq->desc[sts_idx].flags = VIRTQ_DESC_F_WRITE;
    vq->desc[sts_idx].next  = 0u;

    uint16_t avail_slot = vq->avail->idx % vq->queue_size;
    vq->avail->ring[avail_slot] = hdr_idx;
    compiler_barrier();
    vq->avail->idx++;
    compiler_barrier();

    mmio_write16(vq->notify, VBLK_VQ_REQ);

    uint32_t limit = 4000000u;
    uint32_t i;
    for (i = 0u; i < limit; i++) {
        if (vq->used->idx != vq->last_used_idx)
            break;
        cpu_relax();
    }

    if (vq->used->idx == vq->last_used_idx) {
        klog_fail("virtio-blk", "request timed out waiting for used ring");

        goto reclaim;
    }

    vq->last_used_idx++;

    uint8_t status = g_req_status[sts_idx];

reclaim:

    vq->desc[sts_idx].next  = vq->free_head;
    vq->desc[sts_idx].flags = VIRTQ_DESC_F_NEXT;
    vq->free_head = sts_idx;
    vq->num_free++;

    vq->desc[dat_idx].next  = vq->free_head;
    vq->desc[dat_idx].flags = VIRTQ_DESC_F_NEXT;
    vq->free_head = dat_idx;
    vq->num_free++;

    vq->desc[hdr_idx].next  = vq->free_head;
    vq->desc[hdr_idx].flags = VIRTQ_DESC_F_NEXT;
    vq->free_head = hdr_idx;
    vq->num_free++;

    if (i >= limit)
        return -1;

    if (status != VIRTIO_BLK_S_OK) {
        klog_fail("virtio-blk", status == VIRTIO_BLK_S_IOERR
                  ? "device returned I/O error"
                  : "device returned UNSUPP");
        return -1;
    }

    return 0;
}

static int vblk_blkdev_read(void *ctx, uint64_t lba, uint32_t count,
                             void *buf)
{
    (void)ctx;
    return virtio_blk_read(lba, count, buf);
}

static int vblk_blkdev_write(void *ctx, uint64_t lba, uint32_t count,
                              const void *buf)
{
    (void)ctx;
    return virtio_blk_write(lba, count, buf);
}

int virtio_blk_init(void)
{

    pci_device_t *dev = pci_find_device(VIRTIO_VENDOR_ID,
                                        VIRTIO_BLK_DEVICE_MODERN);
    if (!dev) {

        dev = pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_BLK_DEVICE_LEGACY);
    }
    if (!dev) {
        klog_warn("virtio-blk", "no VirtIO block device found on PCI bus");
        return -1;
    }

    pci_enable_device(dev);

    if (vblk_map_caps(dev) != 0)
        return -1;

    klog_ok("virtio-blk", "VirtIO PCI capability regions mapped");

    mmio_write8(&g_common->device_status, 0u);
    compiler_barrier();

    uint32_t i;
    for (i = 0u; i < 100000u; i++) {
        if (mmio_read8(&g_common->device_status) == 0u)
            break;
        cpu_relax();
    }

    mmio_write8(&g_common->device_status, VIRTIO_STATUS_ACKNOWLEDGE);
    compiler_barrier();

    mmio_write8(&g_common->device_status,
                VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    compiler_barrier();

    mmio_write32(&g_common->device_feature_select, 0u);
    compiler_barrier();
    uint32_t dev_feat_lo = mmio_read32(&g_common->device_feature);

    mmio_write32(&g_common->device_feature_select, 1u);
    compiler_barrier();
    uint32_t dev_feat_hi = mmio_read32(&g_common->device_feature);

    uint32_t drv_feat_hi = VBLK_DRIVER_FEATURES_HI;
    if (!(dev_feat_hi & drv_feat_hi)) {
        klog_fail("virtio-blk", "device does not support VIRTIO_F_VERSION_1");
        mmio_write8(&g_common->device_status, VIRTIO_STATUS_FAILED);
        return -1;
    }

    mmio_write32(&g_common->driver_feature_select, 0u);
    compiler_barrier();
    mmio_write32(&g_common->driver_feature,
                 dev_feat_lo & VBLK_DRIVER_FEATURES_LO);

    mmio_write32(&g_common->driver_feature_select, 1u);
    compiler_barrier();
    mmio_write32(&g_common->driver_feature,
                 dev_feat_hi & drv_feat_hi);

    mmio_write8(&g_common->device_status,
                VIRTIO_STATUS_ACKNOWLEDGE |
                VIRTIO_STATUS_DRIVER      |
                VIRTIO_STATUS_FEATURES_OK);
    compiler_barrier();

    uint8_t sts = mmio_read8(&g_common->device_status);
    if (!(sts & VIRTIO_STATUS_FEATURES_OK)) {
        klog_fail("virtio-blk", "device rejected negotiated features (FEATURES_OK cleared)");
        mmio_write8(&g_common->device_status, VIRTIO_STATUS_FAILED);
        return -1;
    }
    klog_ok("virtio-blk", "feature negotiation succeeded");

    if (vblk_vq_init(&g_vq, VBLK_VQ_REQ,
                     vblk_desc,
                     vblk_avail_raw,
                     vblk_used_raw) != 0) {
        klog_fail("virtio-blk", "request virtqueue initialisation failed");
        mmio_write8(&g_common->device_status, VIRTIO_STATUS_FAILED);
        return -1;
    }
    klog_ok("virtio-blk", "request virtqueue initialised");

    mmio_write8(&g_common->device_status,
                VIRTIO_STATUS_ACKNOWLEDGE |
                VIRTIO_STATUS_DRIVER      |
                VIRTIO_STATUS_FEATURES_OK |
                VIRTIO_STATUS_DRIVER_OK);
    compiler_barrier();

    sts = mmio_read8(&g_common->device_status);
    if (sts & (VIRTIO_STATUS_NEEDS_RESET | VIRTIO_STATUS_FAILED)) {
        klog_fail("virtio-blk", "device entered error state after DRIVER_OK");
        return -1;
    }

    uint8_t gen_before, gen_after;
    uint32_t retries = 0u;
    do {
        gen_before  = mmio_read8(&g_common->config_generation);
        g_capacity  = mmio_read64(&g_blk_cfg->capacity);
        g_blk_size  = mmio_read32(&g_blk_cfg->blk_size);
        compiler_barrier();
        gen_after   = mmio_read8(&g_common->config_generation);
        retries++;
    } while (gen_before != gen_after && retries < 16u);

    if (g_blk_size == 0u)
        g_blk_size = 512u;

    int idx = blkdev_register("vblk0",
                              NULL,
                              vblk_blkdev_read,
                              vblk_blkdev_write,
                              g_capacity,
                              g_blk_size);
    if (idx < 0) {
        klog_warn("virtio-blk", "blkdev table full — device not registered");

    } else {
        klog_ok("virtio-blk", "VirtIO block device registered with blkdev");
    }

    g_initialised = 1;
    klog_ok("virtio-blk", "VirtIO-Blk init complete");
    return 0;
}

int virtio_blk_read(uint64_t sector, uint32_t count, void *buf)
{
    if (!g_initialised)
        return -1;
    if (count == 0u || buf == NULL)
        return -1;

    if (sector + count > g_capacity) {
        klog_fail("virtio-blk", "read: LBA out of range");
        return -1;
    }

    return vblk_do_request(VIRTIO_BLK_T_IN, sector, buf, count);
}

int virtio_blk_write(uint64_t sector, uint32_t count, const void *buf)
{
    if (!g_initialised)
        return -1;
    if (count == 0u || buf == NULL)
        return -1;

    if (sector + count > g_capacity) {
        klog_fail("virtio-blk", "write: LBA out of range");
        return -1;
    }

    return vblk_do_request(VIRTIO_BLK_T_OUT, sector, (void *)buf, count);
}
