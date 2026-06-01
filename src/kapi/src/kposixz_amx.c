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

#include <kposixz_amx.h>
#include <kposixz_internal.h>
#include <amx.h>
#include <sched.h>
#include <tykid.h>

extern tykid_gate_ctx_t *tykid_kobalt_get_ctx(void);

long kposixz_amx(unsigned long op, unsigned long arg)
{
    (void)arg;

    if (!g_amx_supported)
        return -1;

    sched_thread_t *t = sched_current();

    switch (op) {
    case KPOSIXZ_AMX_PERM_REQUEST: {
        tykid_gate_ctx_t *ctx = tykid_kobalt_get_ctx();
        if (!ctx || tykid_verify_seal(ctx) != TYKID_OK)
            return -1;
        t->amx_permitted = 1;
        return 0;
    }

    case KPOSIXZ_AMX_PERM_REVOKE:
        t->amx_permitted = 0;
        return 0;

    case KPOSIXZ_AMX_QUERY:
        return (long)t->amx_permitted;

    default:
        return -1;
    }
}

s64 kpz_sys_amx(kpz_frame_t *f)
{
    long rc = kposixz_amx((unsigned long)f->arg1, (unsigned long)f->arg2);
    return rc < 0 ? KPZ_ERR(KPZE_INVAL) : (s64)rc;
}
