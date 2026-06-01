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

#ifndef compiler_barrier
#define compiler_barrier() __asm__ __volatile__("" ::: "memory")
#endif

#define VIRTIO_VENDOR_ID            0x1AF4u
#define VIRTIO_BLK_DEVICE_MODERN    0x1042u
#define VIRTIO_BLK_DEVICE_LEGACY    0x1001u

#define VIRTIO_BLK_F_SIZE_MAX   (1u << 1)
#define VIRTIO_BLK_F_SEG_MAX    (1u << 2)
#define VIRTIO_BLK_F_GEOMETRY   (1u << 4)
#define VIRTIO_BLK_F_RO         (1u << 5)
#define VIRTIO_BLK_F_BLK_SIZE   (1u << 6)
#define VIRTIO_BLK_F_FLUSH      (1u << 9)
#define VIRTIO_BLK_F_TOPOLOGY   (1u << 10)
#define VIRTIO_BLK_F_CONFIG_WCE (1u << 11)
#define VIRTIO_BLK_F_DISCARD    (1u << 13)
#define VIRTIO_BLK_F_WRITE_ZEROES (1u << 14)

#ifndef VIRTIO_F_ACCESS_PLATFORM
#define VIRTIO_F_ACCESS_PLATFORM    (1ULL << 33)
#endif

#define VBLK_DRIVER_FEATURES_LO \
    (VIRTIO_BLK_F_BLK_SIZE | VIRTIO_BLK_F_SEG_MAX)
#define VBLK_DRIVER_FEATURES_HI \
    ((uint32_t)((VIRTIO_F_VERSION_1 | VIRTIO_F_ACCESS_PLATFORM) >> 32u))

typedef struct __attribute__((packed)) {
    uint64_t    capacity;
    uint32_t    size_max;
    uint32_t    seg_max;
    struct {
        uint16_t cylinders;
        uint8_t  heads;
        uint8_t  sectors;
    } geometry;
    uint32_t    blk_size;
    struct {
        uint8_t  physical_block_exp;
        uint8_t  alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;
    } topology;
    uint8_t     writeback;
    uint8_t     _unused0[3];
    uint32_t    max_discard_sectors;
    uint32_t    max_discard_seg;
    uint32_t    discard_sector_alignment;
    uint32_t    max_write_zeroes_sectors;
    uint32_t    max_write_zeroes_seg;
    uint8_t     write_zeroes_may_unmap;
    uint8_t     _unused1[3];
} virtio_blk_config_t;

#define VIRTIO_BLK_T_IN         0u
#define VIRTIO_BLK_T_OUT        1u
#define VIRTIO_BLK_T_FLUSH      4u
#define VIRTIO_BLK_T_DISCARD    11u
#define VIRTIO_BLK_T_WRITE_ZEROES 13u

#define VIRTIO_BLK_S_OK         0u
#define VIRTIO_BLK_S_IOERR      1u
#define VIRTIO_BLK_S_UNSUPP     2u

typedef struct __attribute__((packed)) {
    uint32_t    type;
    uint32_t    reserved;
    uint64_t    sector;
} virtio_blk_req_hdr_t;

_Static_assert(sizeof(virtio_blk_req_hdr_t) == 16,
               "virtio_blk_req_hdr_t must be 16 bytes");

#define VBLK_VQ_REQ    0u

int virtio_blk_init(void);

int virtio_blk_read(uint64_t sector, uint32_t count, void *buf);

int virtio_blk_write(uint64_t sector, uint32_t count, const void *buf);
