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

#include "../inc/tykid_internal.h"

#define SANDBOX_KIND_MMIO   0
#define SANDBOX_KIND_DMA    1
#define SANDBOX_KIND_IRQ    2
#define SANDBOX_KIND_POLICY 3

#define SANDBOX_DMA_LIMIT    (256ULL * 1024 * 1024)
#define SANDBOX_VIOLATION_THRESHOLD  16U

TYKID_INTERNAL void
ty_sandbox_record_access(tykid_gate_ctx_t *ctx, u32 drv_idx,
                          u8 kind, u64 addr, usz len)
{
    if (drv_idx >= TYKID_MAX_DRIVERS) return;

    ty_sandbox_state_t *s = &ctx->sandbox[drv_idx];
    if (!s->active) return;

    s->last_tsc = kobalt_tsc_read();

    switch (kind) {
    case SANDBOX_KIND_MMIO:
        s->mmio_access_count++;
        break;

    case SANDBOX_KIND_DMA: {
        s->dma_bytes_issued += (u64)len;

        if (s->dma_bytes_issued > SANDBOX_DMA_LIMIT) {
            s->policy_violations++;
            TY_LOG(ctx, TY_LOG_WARN,
                   "sandbox: drv[%u] DMA limit exceeded (%llu bytes)",
                   drv_idx, (unsigned long long)s->dma_bytes_issued);
        }
        break;
    }

    case SANDBOX_KIND_IRQ:
        s->irq_count++;
        break;

    case SANDBOX_KIND_POLICY:
        s->policy_violations++;
        if (s->policy_violations >= SANDBOX_VIOLATION_THRESHOLD) {
            TY_LOG(ctx, TY_LOG_ERROR,
                   "sandbox: drv[%u] exceeded violation threshold — revoking",
                   drv_idx);
            if (drv_idx < ctx->reg.count)
                tykid_revoke_driver(ctx, ctx->reg.entries[drv_idx].name);
        }
        break;
    }

    (void)addr;
}

TYKID_INTERNAL u64
ty_sandbox_violation_count(const tykid_gate_ctx_t *ctx, u32 drv_idx)
{
    if (drv_idx >= TYKID_MAX_DRIVERS) return 0;
    return ctx->sandbox[drv_idx].policy_violations;
}

TYKID_API void
tykid_sandbox_activate(tykid_gate_ctx_t *ctx, u32 drv_idx)
{
    if (!ctx || drv_idx >= TYKID_MAX_DRIVERS) return;
    ty_memzero_secure(&ctx->sandbox[drv_idx], sizeof(ctx->sandbox[drv_idx]));
    ctx->sandbox[drv_idx].active = TYKID_TRUE;
}
