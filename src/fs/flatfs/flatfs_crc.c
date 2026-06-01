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

#include "flatfs_crc.h"
#include "flatfs_internal.h"

static uint32_t crc_tab[256];
static int      tab_ready;

void flatfs_crc32_init_table(void)
{
    if (tab_ready) return;
    for (int i = 0; i < 256; i++) {
        uint32_t c = (uint32_t)i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (FLATFS_CRC32_POLY ^ (c >> 1)) : (c >> 1);
        crc_tab[i] = c;
    }
    tab_ready = 1;
}

uint32_t flatfs_crc32_update(uint32_t crc, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    crc ^= 0xFFFFFFFFu;
    while (len--)
        crc = crc_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

uint32_t flatfs_crc32_internal(const void *data, size_t len)
{
    if (!tab_ready) flatfs_crc32_init_table();
    return flatfs_crc32_update(0, data, len);
}

int flatfs_block_crc_ok(const void *blk, size_t sz)
{
    if (sz < 4) return 0;
    const uint8_t *b = (const uint8_t *)blk;
    uint32_t stored;
    FMEMCPY(&stored, b + sz - 4, 4);
    return flatfs_crc32_internal(blk, sz - 4) == stored;
}

void flatfs_block_crc_set(void *blk, size_t sz)
{
    if (sz < 4) return;
    uint8_t *b = (uint8_t *)blk;
    uint32_t c = flatfs_crc32_internal(b, sz - 4);
    FMEMCPY(b + sz - 4, &c, 4);
}
