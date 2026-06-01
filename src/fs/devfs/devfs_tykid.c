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

int devfs_tykid_register_node(devfs_node_t *n)
{
    if (!n) return -1;
    n->tykid_tag = n->tykid_class
                 ^ (uint32_t)(KOBALT_KERNEL_IDENT & 0xFFFFFFFFu)
                 ^ (uint32_t)((uint64_t)n->dev << 4);
    return 0;
}

int devfs_tykid_check(devfs_node_t *node, int flags)
{
    (void)flags;
    if (!node) return -1;
    if (node->sealed && node->tykid_tag == 0) return -1;
    return 0;
}

ssize_t devfs_tykid_entropy_read(void *buf, size_t n)
{
    uint8_t *out = buf;
    size_t done = 0;
    while (done < n) {
        uint64_t rnd = 0;
        uint8_t ok = 0;
        __asm__ volatile("rdrand %0; setc %1" : "=r"(rnd), "=qm"(ok));
        if (!ok) break;
        size_t chunk = (n - done < 8u) ? (n - done) : 8u;
        for (size_t i = 0; i < chunk; i++)
            out[done + i] = (uint8_t)(rnd >> (i * 8));
        done += chunk;
    }
    return (ssize_t)done;
}
