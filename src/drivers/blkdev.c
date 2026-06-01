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

#include <blkdev.h>
#include <kfmt.h>

static blkdev_t g_blkdev_table[BLKDEV_MAX];
static unsigned int g_blkdev_count = 0u;
static blkdev_hotplug_cb_t s_hotplug_cb = NULL;

static void blkdev_strncpy(char *dst, const char *src, size_t n)
{
    size_t i;
    if (n == 0u) return;
    for (i = 0u; i < n - 1u && src[i] != '\0'; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

int blkdev_register(const char     *name,
                    void           *ctx,
                    blkdev_read_fn  read,
                    blkdev_write_fn write,
                    uint64_t        num_sectors,
                    uint32_t        sector_size)
{

    if (!name || !read || !write) {
        klog_fail("blkdev", "register: NULL name or callback");
        return -1;
    }
    if (sector_size == 0u) {
        klog_fail("blkdev", "register: sector_size cannot be zero");
        return -1;
    }
    if (g_blkdev_count >= BLKDEV_MAX) {
        klog_fail("blkdev", "register: device table full");
        return -1;
    }

    unsigned int idx = g_blkdev_count;
    blkdev_t *dev = &g_blkdev_table[idx];

    blkdev_strncpy(dev->name, name, BLKDEV_NAME_MAX);
    dev->ctx         = ctx;
    dev->read        = read;
    dev->write       = write;
    dev->num_sectors = num_sectors;
    dev->sector_size = sector_size;
    dev->valid       = 1;

    g_blkdev_count++;

    if (s_hotplug_cb)
        s_hotplug_cb((int)idx, 1);

    return (int)idx;
}

blkdev_t *blkdev_get(unsigned int index)
{
    if (index >= BLKDEV_MAX)
        return NULL;
    if (!g_blkdev_table[index].valid)
        return NULL;
    return &g_blkdev_table[index];
}

unsigned int blkdev_count(void)
{
    return g_blkdev_count;
}

int blkdev_read(blkdev_t *dev,
                uint64_t  lba,
                uint32_t  count,
                void     *buf)
{
    if (!dev || !dev->valid || !dev->read)
        return -1;
    if (count == 0u || !buf)
        return -1;
    if (lba + count > dev->num_sectors)
        return -1;

    return dev->read(dev->ctx, lba, count, buf);
}

int blkdev_write(blkdev_t   *dev,
                 uint64_t    lba,
                 uint32_t    count,
                 const void *buf)
{
    if (!dev || !dev->valid || !dev->write)
        return -1;
    if (count == 0u || !buf)
        return -1;
    if (lba + count > dev->num_sectors)
        return -1;

    return dev->write(dev->ctx, lba, count, buf);
}

int blkdev_get_info(int idx, blkdev_info_t *info)
{
    if (idx < 0 || (unsigned)idx >= g_blkdev_count) return -1;
    blkdev_t *d = &g_blkdev_table[idx];
    if (!d->valid) return -1;

    const char *n = d->name;
    if (n[0]=='a' && n[1]=='h' && n[2]=='c' && n[3]=='i')
        info->type = BLKDEV_TYPE_AHCI;
    else if (n[0]=='n' && n[1]=='v' && n[2]=='m' && n[3]=='e')
        info->type = BLKDEV_TYPE_NVME;
    else if (n[0]=='v' && n[1]=='b' && n[2]=='l' && n[3]=='k')
        info->type = BLKDEV_TYPE_VIRTIO;
    else
        info->type = BLKDEV_TYPE_UNKNOWN;

    int slot = 0;
    const char *p = n;
    while (*p && (*p < '0' || *p > '9')) p++;
    while (*p >= '0' && *p <= '9') { slot = slot * 10 + (*p - '0'); p++; }
    info->slot = slot;

    info->total_sectors   = d->num_sectors;
    info->sector_size     = d->sector_size;
    info->phys_sector_size = d->sector_size;
    blkdev_strncpy(info->model, d->name, sizeof(info->model));
    return 0;
}

void blkdev_set_hotplug_cb(blkdev_hotplug_cb_t cb)
{
    s_hotplug_cb = cb;

    for (unsigned int i = 0; i < g_blkdev_count; i++) {
        if (g_blkdev_table[i].valid)
            cb((int)i, 1);
    }
}
