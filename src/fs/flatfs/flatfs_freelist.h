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

#ifndef FLATFS_FREELIST_H
#define FLATFS_FREELIST_H

#include "flatfs_alloc.h"

flatfs_err_t flatfs_freelist_load(uint32_t group_idx, flatfs_flgroup_t *out);
flatfs_err_t flatfs_freelist_flush(uint32_t group_idx,
                                    const flatfs_flgroup_t *in);
flatfs_err_t flatfs_freelist_rebuild(void);

#endif
