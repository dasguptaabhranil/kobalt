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

#ifndef DEVFS_KOBALT_H
#define DEVFS_KOBALT_H

#include "../../../security/tykid/inc/tykid.h"

void devfs_kobalt_init(tykid_gate_ctx_t *ctx);
int  devfs_kobalt_is_mounted(void);
void devfs_kobalt_seal(void);

void devfs_bdev_hotplug(int blkdev_idx, int present);

#endif
