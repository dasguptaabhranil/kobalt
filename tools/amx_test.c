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

#include <amx.h>
#include <amx_init.h>
#include <amx_tile.h>
#include <amx_bf16.h>
#include <amx_int8.h>
#include <amx_state.h>
#include <amx_mem.h>
#include <kernel.h>
#include <string.h>

static void test_detect(void)
{
    kprintf("[amx_test] g_amx_supported=%d xsave_size=%u\n",
            g_amx_supported, g_amx_xsave_size);
    kprintf("[amx_test] bf16=%d int8=%d\n",
            amx_bf16_supported(), amx_int8_supported());
}

static void test_ctx_alloc(void)
{
    void *raw = NULL;
    void *ctx = amx_ctx_alloc(&raw);
    if (!ctx) { kputs("[amx_test] FAIL: ctx alloc\n"); return; }
    if ((uintptr_t)ctx & 63) { kputs("[amx_test] FAIL: alignment\n"); return; }
    if (((void **)ctx)[-1] != raw) { kputs("[amx_test] FAIL: back-ptr\n"); return; }
    amx_ctx_init(ctx);
    uint64_t xcomp = ((uint64_t *)ctx)[AMX_XSAVE_XCOMP_OFF / 8];
    if (!(xcomp & (1ULL << 63))) { kputs("[amx_test] FAIL: XCOMP_BV\n"); return; }
    amx_ctx_free(raw);
    kputs("[amx_test] PASS: ctx alloc/init/free\n");
}

static void test_tilerelease(void)
{
    if (!g_amx_supported) { kputs("[amx_test] SKIP: no AMX\n"); return; }

    static amx_tilecfg_t cfg __attribute__((aligned(64)));
    memset(&cfg, 0, sizeof(cfg));
    cfg.palette_id = 1;
    cfg.rows[0]    = 8;
    cfg.colsb[0]   = 32;
    cfg.rows[1]    = 8;
    cfg.colsb[1]   = 32;
    cfg.rows[2]    = 8;
    cfg.colsb[2]   = 32;

    amx_ldtilecfg(&cfg);
    amx_tilerelease();
    kputs("[amx_test] PASS: ldtilecfg + tilerelease\n");
}

static void test_save_restore(void)
{
    if (!g_amx_supported) { kputs("[amx_test] SKIP: no AMX\n"); return; }

    void *raw = NULL;
    void *ctx = amx_ctx_alloc(&raw);
    if (!ctx) { kputs("[amx_test] FAIL: alloc\n"); return; }
    amx_ctx_init(ctx);
    amx_state_save(ctx);
    amx_state_restore(ctx);
    amx_ctx_free(raw);
    kputs("[amx_test] PASS: xsaves/xrstors round-trip\n");
}

void amx_run_tests(void)
{
    kputs("[amx_test] starting\n");
    test_detect();
    test_ctx_alloc();
    test_tilerelease();
    test_save_restore();
    kputs("[amx_test] done\n");
}
