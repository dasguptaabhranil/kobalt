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

#ifndef DEVFS_IOCTL_H
#define DEVFS_IOCTL_H

#include <stdint.h>

#define BLKGETSIZE64   0x80081272u
#define BLKGETSS       0x1268u
#define BLKGETPBSZ     0x127Bu
#define BLKFLSBUF      0x1261u
#define BLKIDENTIFY    0x9900u

#define TIOCGWINSZ     0x5413u

#define DEVIOC_GETINFO 0xDE01u
#define DEVIOC_GETTAG  0xDE02u

typedef struct {
    uint64_t size_bytes;
    uint32_t sector_sz;
    uint32_t phys_sector_sz;
    char     model[40];
} devfs_blk_ident_t;

typedef struct {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} devfs_winsize_t;

typedef struct {
    char     name[64];
    uint32_t major;
    uint32_t minor;
    uint32_t tykid_class;
    uint32_t tykid_tag;
} devfs_info_t;

#endif
