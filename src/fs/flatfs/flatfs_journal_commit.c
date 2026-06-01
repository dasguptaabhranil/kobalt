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

flatfs_err_t flatfs_journal_write_meta(uint64_t blk, const void *data)
{
    uint64_t tx;
    flatfs_err_t e = flatfs_journal_begin(&tx);
    if (e) return e;

    e = flatfs_journal_log(tx, blk, data);
    if (e) { flatfs_journal_abort(tx); return e; }

    return flatfs_journal_commit(tx);
}

flatfs_err_t flatfs_journal_write2(uint64_t blk0, const void *d0,
                                    uint64_t blk1, const void *d1)
{
    uint64_t tx;
    flatfs_err_t e = flatfs_journal_begin(&tx);
    if (e) return e;

    e = flatfs_journal_log(tx, blk0, d0);
    if (e) { flatfs_journal_abort(tx); return e; }

    e = flatfs_journal_log(tx, blk1, d1);
    if (e) { flatfs_journal_abort(tx); return e; }

    return flatfs_journal_commit(tx);
}
