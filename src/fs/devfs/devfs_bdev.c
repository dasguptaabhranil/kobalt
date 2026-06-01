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

#include "devfs.h"
#include "devfs_tykid.h"
#include "devfs_ioctl.h"
#include "../../inc/blkdev.h"
#include "../../inc/kernel.h"
#include "../../inc/kmalloc.h"
#include "../../inc/string.h"

#define DEVFS_MAX_BDEV  32u

typedef struct {
    int      blkdev_idx;
    uint32_t sect_sz;
    uint32_t phys_sect_sz;
    uint64_t total_sects;
    char     model[32];
    uint8_t  _inuse;
} devfs_bdev_priv_t;

static devfs_bdev_priv_t s_bpriv[DEVFS_MAX_BDEV];

static devfs_bdev_priv_t *alloc_bpriv(void)
{
    for (uint32_t i = 0; i < DEVFS_MAX_BDEV; i++)
        if (!s_bpriv[i]._inuse) {
            s_bpriv[i]._inuse = 1;
            return &s_bpriv[i];
        }
    return NULL;
}

static void free_bpriv(devfs_bdev_priv_t *p)
{
    if (p) p->_inuse = 0;
}

static int bdev_open(void *priv, int flags)
{
    (void)priv; (void)flags;

    return 0;
}

static void bdev_close(void *priv) { (void)priv; }

static ssize_t bdev_read(void *priv, void *buf, size_t n, uint64_t *pos)
{
    devfs_bdev_priv_t *bp = priv;
    if (!bp || !buf || !n) return 0;

    uint32_t sz = bp->sect_sz ? bp->sect_sz : 512u;
    uint64_t lba   = *pos / sz;
    uint64_t off   = *pos % sz;
    size_t   nsect = (n + sz - 1) / sz;

    blkdev_t *bd = blkdev_get((unsigned)bp->blkdev_idx);
    if (!bd) return -1;

    if (off || (n % sz)) {
        uint8_t bounce[4096];
        if (sz > sizeof(bounce)) return -1;
        size_t done = 0;
        while (done < n) {
            if (blkdev_read(bd, lba, 1, bounce) < 0)
                return done ? (ssize_t)done : -1;
            size_t chunk = sz - off;
            if (chunk > n - done) chunk = n - done;
            memcpy((uint8_t *)buf + done, bounce + off, chunk);
            done += chunk;
            lba++;
            off = 0;
        }
        *pos += done;
        return (ssize_t)done;
    }

    if (blkdev_read(bd, lba, (uint32_t)nsect, buf) < 0)
        return -1;
    *pos += n;
    return (ssize_t)n;
}

static ssize_t bdev_write(void *priv, const void *buf, size_t n, uint64_t *pos)
{
    devfs_bdev_priv_t *bp = priv;
    if (!bp || !buf || !n) return 0;

    uint32_t sz  = bp->sect_sz ? bp->sect_sz : 512u;
    uint64_t lba = *pos / sz;

    blkdev_t *bd = blkdev_get((unsigned)bp->blkdev_idx);
    if (!bd) return -1;

    if (*pos % sz || n % sz) {
        uint8_t bounce[4096];
        if (sz > sizeof(bounce)) return -1;
        uint64_t off  = *pos % sz;
        size_t   done = 0;
        while (done < n) {
            if (blkdev_read(bd, lba, 1, bounce) < 0)
                return done ? (ssize_t)done : -1;
            size_t chunk = sz - off;
            if (chunk > n - done) chunk = n - done;
            memcpy(bounce + off, (const uint8_t *)buf + done, chunk);
            if (blkdev_write(bd, lba, 1, bounce) < 0)
                return done ? (ssize_t)done : -1;
            done += chunk;
            lba++;
            off = 0;
        }
        *pos += done;
        return (ssize_t)done;
    }

    uint32_t nsect = (uint32_t)(n / sz);
    if (blkdev_write(bd, lba, nsect, buf) < 0)
        return -1;
    *pos += n;
    return (ssize_t)n;
}

static int bdev_ioctl(void *priv, unsigned long cmd, void *arg)
{
    devfs_bdev_priv_t *bp = priv;
    if (!bp) return -1;

    if (cmd == BLKGETSIZE64) {
        *(uint64_t *)arg = bp->total_sects * bp->sect_sz;
        return 0;
    }
    if (cmd == BLKGETSS) {
        *(uint32_t *)arg = bp->sect_sz;
        return 0;
    }
    if (cmd == BLKGETPBSZ) {
        *(uint32_t *)arg = bp->phys_sect_sz;
        return 0;
    }
    if (cmd == BLKFLSBUF) {

        return 0;
    }
    if (cmd == BLKIDENTIFY) {
        devfs_blk_ident_t *id = arg;
        id->size_bytes     = bp->total_sects * bp->sect_sz;
        id->sector_sz      = bp->sect_sz;
        id->phys_sector_sz = bp->phys_sect_sz;
        strncpy(id->model, bp->model, sizeof(id->model) - 1);
        return 0;
    }
    return -1;
}

static devfs_ops_t s_bdev_ops = {
    .open  = bdev_open,
    .close = bdev_close,
    .read  = bdev_read,
    .write = bdev_write,
    .ioctl = bdev_ioctl,
    .poll  = NULL,
};

void devfs_bdev_hotplug(int idx, int present)
{
    if (!g_devfs.mounted) return;

    blkdev_info_t info;
    if (blkdev_get_info(idx, &info) < 0) return;

    char devname[DEVFS_NAME_MAX];
    uint32_t major;

    switch (info.type) {
    case BLKDEV_TYPE_AHCI:
        ksnprintf(devname, sizeof(devname), "hd%d", info.slot);
        major = DEVFS_MAJOR_HD;
        break;
    case BLKDEV_TYPE_NVME:
        ksnprintf(devname, sizeof(devname), "nvme%d", info.slot);
        major = DEVFS_MAJOR_NVME;
        break;
    case BLKDEV_TYPE_VIRTIO:
        ksnprintf(devname, sizeof(devname), "vblk%d", info.slot);
        major = DEVFS_MAJOR_VBLK;
        break;
    default:
        ksnprintf(devname, sizeof(devname), "blk%d", idx);
        major = DEVFS_MAJOR_VBLK;
        break;
    }

    if (!present) {
        devfs_unregister(major, (uint32_t)idx);
        char msg[48];
        ksnprintf(msg, sizeof(msg), "/dev/%s removed", devname);
        klog_info("devfs", msg);
        return;
    }

    devfs_bdev_priv_t *bp = alloc_bpriv();
    if (!bp) {
        klog_warn("devfs", "bdev priv pool exhausted");
        return;
    }

    bp->blkdev_idx   = idx;
    bp->sect_sz      = info.sector_size ? info.sector_size : 512u;
    bp->phys_sect_sz = info.phys_sector_size ? info.phys_sector_size : bp->sect_sz;
    bp->total_sects  = info.total_sectors;
    strncpy(bp->model, info.model, sizeof(bp->model) - 1);

    int rc = devfs_register_bdev(major, (uint32_t)idx, devname,
                                 DEVFS_CLASS_BLOCK, &s_bdev_ops, bp);
    if (rc < 0) {
        free_bpriv(bp);
        char msg[64];
        ksnprintf(msg, sizeof(msg), "failed to create /dev/%s", devname);
        klog_warn("devfs", msg);
        return;
    }

    char msg[80];
    ksnprintf(msg, sizeof(msg), "/dev/%s: %llu sectors x %u bytes",
              devname,
              (unsigned long long)bp->total_sects,
              bp->sect_sz);
    klog_ok("devfs", msg);
}
