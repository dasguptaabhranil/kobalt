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

#ifndef FLATFS_SUPER_H
#define FLATFS_SUPER_H

#include "flatfs.h"

flatfs_err_t  flatfs_super_read(void *buf, uint64_t cap, flatfs_super_t **out);
flatfs_err_t  flatfs_super_write(flatfs_super_t *sb);
flatfs_err_t  flatfs_super_validate(const flatfs_super_t *sb);
flatfs_err_t  flatfs_super_init(void *buf, uint64_t cap, const char *label,
                                 uint64_t ninodes);
void          flatfs_super_mark_dirty(flatfs_super_t *sb);
void          flatfs_super_mark_clean(flatfs_super_t *sb);
void          flatfs_super_update_crc(flatfs_super_t *sb);

#endif
