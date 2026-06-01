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

#pragma once

#include <stdint.h>
#include <stddef.h>

#define BLKDEV_MAX          16u

#define BLKDEV_NAME_MAX     32u

typedef int (*blkdev_read_fn) (void *ctx,
                               uint64_t lba, uint32_t count,
                               void       *buf);

typedef int (*blkdev_write_fn)(void *ctx,
                               uint64_t lba, uint32_t count,
                               const void *buf);

typedef struct {
    char             name[BLKDEV_NAME_MAX];
    void            *ctx;
    blkdev_read_fn   read;
    blkdev_write_fn  write;
    uint64_t         num_sectors;
    uint32_t         sector_size;
    int              valid;
} blkdev_t;

int blkdev_register(const char     *name,
                    void           *ctx,
                    blkdev_read_fn  read,
                    blkdev_write_fn write,
                    uint64_t        num_sectors,
                    uint32_t        sector_size);

blkdev_t *blkdev_get(unsigned int index);

unsigned int blkdev_count(void);

int blkdev_read(blkdev_t *dev,
                uint64_t  lba,
                uint32_t  count,
                void     *buf);

int blkdev_write(blkdev_t   *dev,
                 uint64_t    lba,
                 uint32_t    count,
                 const void *buf);

typedef enum {
    BLKDEV_TYPE_UNKNOWN = 0,
    BLKDEV_TYPE_AHCI,
    BLKDEV_TYPE_NVME,
    BLKDEV_TYPE_VIRTIO,
    BLKDEV_TYPE_USB_MSC,
} blkdev_type_t;

typedef struct {
    blkdev_type_t type;
    int           slot;
    uint64_t      total_sectors;
    uint32_t      sector_size;
    uint32_t      phys_sector_size;
    char          model[40];
} blkdev_info_t;

int blkdev_get_info(int idx, blkdev_info_t *info);

typedef void (*blkdev_hotplug_cb_t)(int idx, int present);
void blkdev_set_hotplug_cb(blkdev_hotplug_cb_t cb);
