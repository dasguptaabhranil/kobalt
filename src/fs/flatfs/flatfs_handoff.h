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

#ifndef FLATFS_HANDOFF_H
#define FLATFS_HANDOFF_H

#include "flatfs.h"

#define FLATFS_HANDOFF_THRESHOLD  40
#define FLATFS_RECOVER_THRESHOLD  70

typedef enum {
    FLATFS_HS_NORMAL   = 0,
    FLATFS_HS_DEGRADED = 1,
    FLATFS_HS_HANDOFF  = 2,
    FLATFS_HS_RECOVERY = 3,
} flatfs_handoff_state_t;

void                  flatfs_handoff_init(void);
flatfs_handoff_state_t flatfs_handoff_state(void);
flatfs_err_t          flatfs_handoff_tick(void);
flatfs_err_t          flatfs_handoff_request(void);
flatfs_err_t          flatfs_handoff_reclaim(void);
int                   flatfs_handoff_active(void);

#endif
