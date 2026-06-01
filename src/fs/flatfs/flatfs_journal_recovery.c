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

#include "flatfs_internal.h"
#include "flatfs_journal.h"
#include "flatfs_super.h"

flatfs_err_t flatfs_journal_recover(void)
{
    flatfs_super_t *sb = g_fs.super;
    if (sb->state & FLATFS_STATE_REPLAY) return FLATFS_ERR_BADSTATE;

    sb->state |= FLATFS_STATE_REPLAY;
    flatfs_super_write(sb);

    flatfs_err_t e = flatfs_journal_replay();
    if (e) {
        sb->state |= FLATFS_STATE_ERROR;
        sb->error_count++;
        flatfs_super_write(sb);
        return e;
    }

    flatfs_journal_checkpoint();
    sb->state &= ~(FLATFS_STATE_DIRTY | FLATFS_STATE_REPLAY);
    sb->state |= FLATFS_STATE_CLEAN;
    flatfs_super_write(sb);
    return FLATFS_OK;
}
