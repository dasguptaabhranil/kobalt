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
#include "flatfs_handoff.h"
#include "flatfs_tykid.h"
#include "flatfs_monitor.h"
#include "flatfs_journal.h"
#include "flatfs_super.h"
#include "flatfs_btree.h"

extern flatfs_err_t flatfs_arbiter_notify_degraded(void);
extern flatfs_err_t flatfs_arbiter_notify_recovered(void);

static flatfs_handoff_state_t g_hs = FLATFS_HS_NORMAL;

void flatfs_handoff_init(void)
{
    g_hs = FLATFS_HS_NORMAL;
}

flatfs_handoff_state_t flatfs_handoff_state(void)
{
    return g_hs;
}

int flatfs_handoff_active(void)
{
    return g_hs == FLATFS_HS_HANDOFF;
}

flatfs_err_t flatfs_handoff_request(void)
{
    if (g_hs != FLATFS_HS_DEGRADED) return FLATFS_ERR_BADSTATE;
    flatfs_err_t e = flatfs_arbiter_notify_degraded();
    if (e) return e;
    g_hs = FLATFS_HS_HANDOFF;
    g_fs.super->state |= FLATFS_STATE_ARBITER;
    flatfs_tykid_audit_handoff(1);
    flatfs_super_write(g_fs.super);
    return FLATFS_OK;
}

flatfs_err_t flatfs_handoff_reclaim(void)
{
    if (g_hs != FLATFS_HS_HANDOFF && g_hs != FLATFS_HS_RECOVERY)
        return FLATFS_ERR_BADSTATE;

    flatfs_journal_checkpoint();
    flatfs_btree_verify();
    flatfs_arbiter_notify_recovered();

    g_hs = FLATFS_HS_NORMAL;
    g_fs.super->state &= ~FLATFS_STATE_ARBITER;
    flatfs_tykid_audit_handoff(0);
    flatfs_super_write(g_fs.super);
    return FLATFS_OK;
}

flatfs_err_t flatfs_handoff_tick(void)
{
    int score = flatfs_mon_health_score();

    switch (g_hs) {
    case FLATFS_HS_NORMAL:
        if (score < FLATFS_HANDOFF_THRESHOLD)
            g_hs = FLATFS_HS_DEGRADED;
        break;

    case FLATFS_HS_DEGRADED:
        if (score >= FLATFS_RECOVER_THRESHOLD) {
            g_hs = FLATFS_HS_NORMAL;
        } else {

            flatfs_handoff_request();
        }
        break;

    case FLATFS_HS_HANDOFF:
        if (score >= FLATFS_RECOVER_THRESHOLD)
            g_hs = FLATFS_HS_RECOVERY;
        break;

    case FLATFS_HS_RECOVERY:
        if (score >= FLATFS_RECOVER_THRESHOLD)
            flatfs_handoff_reclaim();
        else
            g_hs = FLATFS_HS_HANDOFF;
        break;
    }
    return FLATFS_OK;
}
