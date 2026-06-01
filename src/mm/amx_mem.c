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

#include <amx_mem.h>
#include <amx.h>
#include <kmalloc.h>
#include <string.h>

void *amx_ctx_alloc(void **raw_out)
{
    size_t total = g_amx_xsave_size + 64 + sizeof(void *);
    void *raw = kmalloc(total);
    if (!raw) return NULL;

    uintptr_t ua = (uintptr_t)raw + sizeof(void *);
    ua = (ua + 63) & ~(uintptr_t)63;
    ((void **)ua)[-1] = raw;

    if (raw_out) *raw_out = raw;
    return (void *)ua;
}

void amx_ctx_free(void *raw)
{
    if (raw) kfree(raw);
}

void amx_ctx_init(void *ctx)
{
    memset(ctx, 0, g_amx_xsave_size);

    ((uint64_t *)ctx)[AMX_XSAVE_XCOMP_OFF / 8] = (1ULL << 63) | XCR0_AMX_MASK;
}
