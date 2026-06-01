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

#ifdef TYKID_FUZZ

#include "../inc/tykid_internal.h"
#include <stdint.h>
#include <stddef.h>

static u8 g_fuzz_key[16] = {
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF
};

int LLVMFuzzerTestOneInput_pci(const uint8_t *data, size_t size)
{
    if (size < 64) return 0;

    tykid_hw_device_t dev;
    ty_memzero_secure(&dev, sizeof(dev));

    dev.vendor_id  = (u16)(data[0] | ((u16)data[1] << 8));
    dev.device_id  = (u16)(data[2] | ((u16)data[3] << 8));
    dev.class_code = (u32)(data[4] | ((u32)data[5]<<8) | ((u32)data[6]<<16));
    dev.subsys_id  = (u32)(data[8] | ((u32)data[9]<<8)
                         | ((u32)data[10]<<16) | ((u32)data[11]<<24));
    dev.mmio_base  = (u64)data[12] & ~0xFULL;
    dev.mmio_size  = (u64)(data[20] | ((u64)data[21]<<8));
    dev.bus        = data[28];
    dev.slot       = data[29] & 0x1F;
    dev.func       = data[30] & 0x07;
    dev.irq        = data[31];

    dev.ty_class = ty_hw_classify(dev.vendor_id, dev.device_id, dev.class_code);
    ty_hw_fingerprint_one(&dev, g_fuzz_key);

    return 0;
}

int LLVMFuzzerTestOneInput_audit(const uint8_t *data, size_t size)
{
    if (size < 64) return 0;

    static tykid_gate_ctx_t fake_ctx;
    static int ctx_init = 0;
    if (!ctx_init) {
        ty_memzero_secure(&fake_ctx, sizeof(fake_ctx));
        ty_memcpy(fake_ctx.session_key, g_fuzz_key, 16);
        ty_memcpy(fake_ctx.hmac_key,    g_fuzz_key, 16);
        ctx_init = 1;
    }

    (void)data;
    (void)size;

    ty_audit_verify_chain(&fake_ctx);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0) return 0;
    if (data[0] & 1)
        return LLVMFuzzerTestOneInput_pci(data + 1, size - 1);
    return LLVMFuzzerTestOneInput_audit(data + 1, size - 1);
}

#endif
