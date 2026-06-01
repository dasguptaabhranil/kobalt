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

#ifndef __TYKID_SEAL_EXT_H__
#define __TYKID_SEAL_EXT_H__

#include "tykid_internal.h"

extern u32  kobalt_cpuid_microcode_rev(void);
extern void kobalt_smbios_uuid(u8 out[16]);
extern u64  kobalt_platform_security_version(void);
extern void kobalt_kernel_image_hash(u8 out[32]);

#define TYKID_SEAL_VECTOR_COUNT    4U
#define TYKID_HYPERSEAL_BYTES 32U
#define TYKID_SEAL_MIX_ROUNDS 24U

typedef struct TYKID_PACKED {
 u8 raw[32];
 u64 contribution;
 u8 valid;
} ty_seal_vector_t;

typedef struct TYKID_ALIGNED(64) {
    ty_seal_vector_t vectors[TYKID_SEAL_VECTOR_COUNT];
 u8 hyperseal[TYKID_HYPERSEAL_BYTES];
 u8 hyperseal_prev[TYKID_HYPERSEAL_BYTES];
    u64              collection_timestamp;
    u32              collection_count;
    bool8            frozen;
    bool8            pre_validated;
} ty_ext_seal_ctx_t;

#define TYKID_SEAL_MIX_C0   (0x6A09E667F3BCC908ULL ^ __TYKID_KOBALT_EXPECTED)
#define TYKID_SEAL_MIX_C1   (0xBB67AE8584CAA73BULL ^ ty_rotl64(__TYKID_KOBALT_EXPECTED, 13))
#define TYKID_SEAL_MIX_C2   (0x3C6EF372FE94F82BULL ^ ty_rotl64(__TYKID_KOBALT_EXPECTED, 27))
#define TYKID_SEAL_MIX_C3   (0xA54FF53A5F1D36F1ULL ^ ty_rotl64(__TYKID_KOBALT_EXPECTED, 41))
#define TYKID_SEAL_MIX_C4   (0x510E527FADE682D1ULL ^ ty_rotl64(__TYKID_KOBALT_EXPECTED, 55))
#define TYKID_SEAL_MIX_C5   (0x9B05688C2B3E6C1FULL ^ ty_rotl64(__TYKID_KOBALT_EXPECTED, 7))
#define TYKID_SEAL_MIX_C6   (0x1F83D9ABFB41BD6BULL ^ ty_rotl64(__TYKID_KOBALT_EXPECTED, 19))
#define TYKID_SEAL_MIX_C7   (0x5BE0CD19137E2179ULL ^ ty_rotl64(__TYKID_KOBALT_EXPECTED, 33))

#define TY_MIX_ROT(r, w) ( \
    ((const u8[]){14,16,52,57,23,40,5,37,25,33,46,12,58,22,32,32})[((r)*4+(w))%16] \
    ^ (u8)(__TYKID_KOBALT_EXPECTED >> (((r)+(w))*7 % 64)) )

static TYKID_ALWAYS_INL void
ty_seal_mix256(u64 s[4])
{

    s[0] ^= TYKID_SEAL_MIX_C0;
    s[1] ^= TYKID_SEAL_MIX_C1;
    s[2] ^= TYKID_SEAL_MIX_C2;
    s[3] ^= TYKID_SEAL_MIX_C3;

    for (u32 r = 0; r < TYKID_SEAL_MIX_ROUNDS; r++) {

        s[0] += s[1]; s[1] = ty_rotl64(s[1], TY_MIX_ROT(r,0)); s[1] ^= s[0];
        s[2] += s[3]; s[3] = ty_rotl64(s[3], TY_MIX_ROT(r,1)); s[3] ^= s[2];

        s[0] += s[3]; s[3] = ty_rotl64(s[3], TY_MIX_ROT(r,2)); s[3] ^= s[0];
        s[2] += s[1]; s[1] = ty_rotl64(s[1], TY_MIX_ROT(r,3)); s[1] ^= s[2];

        s[r & 3] ^= TYKID_SEAL_MIX_C0 + r * TYKID_SEAL_MIX_C4;
    }

    s[0] ^= TYKID_SEAL_MIX_C4;
    s[1] ^= TYKID_SEAL_MIX_C5;
    s[2] ^= TYKID_SEAL_MIX_C6;
    s[3] ^= TYKID_SEAL_MIX_C7;
}

static TYKID_NOINLINE void
ty_seal_collect_vectors(ty_ext_seal_ctx_t *esc)
{

    u8 sipkey[16];
    u64 base = __TYKID_KOBALT_EXPECTED;
    ty_put_u64le(sipkey,     base);
    ty_put_u64le(sipkey + 8, ~base);

    ty_memzero_secure(esc->vectors, sizeof(esc->vectors));
    esc->collection_count = 0;

    {
        ty_seal_vector_t *v = &esc->vectors[0];
        u32 ucrev = kobalt_cpuid_microcode_rev();
        ty_put_u32le(v->raw, ucrev);

        for (u32 i = 4; i < 32; i++) v->raw[i] = (u8)(ucrev >> ((i & 3) * 8)) ^ 0xA5U;
        v->contribution = ty_siphash24(sipkey, v->raw, 32);
        v->valid = 1;
        esc->collection_count++;
    }

    {
        ty_seal_vector_t *v = &esc->vectors[1];
        u8 uuid[16];
        kobalt_smbios_uuid(uuid);

        ty_memcpy(v->raw, uuid, 16);
        u64 h0 = ty_siphash24(sipkey, uuid, 16);
 u64 h1 = ty_siphash24(sipkey + 8, uuid, 16);
        ty_put_u64le(v->raw + 16, h0);
        ty_put_u64le(v->raw + 24, h1);
        v->contribution = h0 ^ ty_rotl64(h1, 32);

        u8 zero_check = 0;
        for (u32 i = 0; i < 16; i++) zero_check |= uuid[i];
        v->valid = zero_check ? 1 : 0;
        if (v->valid) esc->collection_count++;
    }

    {
        ty_seal_vector_t *v = &esc->vectors[2];
        u64 psv = kobalt_platform_security_version();
        ty_put_u64le(v->raw, psv);

        for (u32 i = 0; i < 8; i++) {
            v->raw[8  + i] = v->raw[7 - i] ^ 0x5AU;
            v->raw[16 + i] = (u8)(psv >> (i * 8)) ^ 0xC3U;
            v->raw[24 + i] = v->raw[i] ^ v->raw[8 + i];
        }
        v->contribution = ty_siphash24(sipkey, v->raw, 32);
        v->valid = (psv != 0) ? 1 : 0;
        if (v->valid) esc->collection_count++;
    }

    {
        ty_seal_vector_t *v = &esc->vectors[3];
 kobalt_kernel_image_hash(v->raw);
        v->contribution = ty_siphash24(sipkey, v->raw, 32);

        u8 nz = 0;
        for (u32 i = 0; i < 32; i++) nz |= v->raw[i];
        v->valid = nz ? 1 : 0;
        if (v->valid) esc->collection_count++;
    }

    ty_memzero_secure(sipkey, sizeof(sipkey));
}

static TYKID_NOINLINE void
ty_seal_compute_hyperseal(ty_ext_seal_ctx_t *esc)
{

    u64 c[4];
    for (u32 i = 0; i < 4; i++) {
        c[i] = esc->vectors[i].valid
             ? esc->vectors[i].contribution
 : (TYKID_SEAL_MIX_C0 ^ (u64)i * TYKID_SEAL_MIX_C3);
    }

    u64 s[4];
    s[0] = c[0] ^ c[1];
    s[1] = c[1] ^ c[2];
    s[2] = c[2] ^ c[3];
    s[3] = c[3] ^ ty_rotl64(c[0], 17);

    s[0] ^= __TYKID_KOBALT_EXPECTED;
    s[2] ^= ty_rotl64(__TYKID_KOBALT_EXPECTED, 43);

    ty_seal_mix256(s);

    ty_put_u64le(esc->hyperseal +  0, s[0]);
    ty_put_u64le(esc->hyperseal +  8, s[1]);
    ty_put_u64le(esc->hyperseal + 16, s[2]);
    ty_put_u64le(esc->hyperseal + 24, s[3]);

    ty_memzero_secure(c, sizeof(c));
    ty_memzero_secure(s, sizeof(s));
}

static TYKID_NOINLINE tykid_status_t
ty_ext_seal_preflight(ty_ext_seal_ctx_t *esc)
{
    ty_seal_collect_vectors(esc);
    ty_seal_compute_hyperseal(esc);
    esc->pre_validated = TYKID_TRUE;
    return esc->collection_count >= 2 ? TYKID_OK : TYKID_ERR_SEAL_BROKEN;
}

static TYKID_NOINLINE tykid_status_t
ty_ext_seal_init(ty_ext_seal_ctx_t *esc)
{
    if (esc->frozen) return TYKID_ERR_ALREADY_LOADED;

    if (!esc->pre_validated) {
        tykid_status_t pre = ty_ext_seal_preflight(esc);
        if (pre != TYKID_OK) return pre;
    }

    ty_seal_collect_vectors(esc);
    ty_seal_compute_hyperseal(esc);

    if (esc->pre_validated) {
        u8 tmp[TYKID_HYPERSEAL_BYTES];
        ty_memcpy(tmp, esc->hyperseal, TYKID_HYPERSEAL_BYTES);
        ty_seal_collect_vectors(esc);
        ty_seal_compute_hyperseal(esc);
        if (!ty_memeq(esc->hyperseal, tmp, TYKID_HYPERSEAL_BYTES)) {
            ty_memzero_secure(tmp, sizeof(tmp));
            return TYKID_ERR_SEAL_BROKEN;
        }
        ty_memzero_secure(tmp, sizeof(tmp));
    }

    ty_memcpy(esc->hyperseal_prev, esc->hyperseal, TYKID_HYPERSEAL_BYTES);
    esc->frozen = TYKID_TRUE;
    return TYKID_OK;
}

static TYKID_NOINLINE tykid_status_t
ty_ext_seal_recheck(ty_ext_seal_ctx_t *esc)
{
    u8 saved[TYKID_HYPERSEAL_BYTES];
    ty_memcpy(saved, esc->hyperseal, TYKID_HYPERSEAL_BYTES);

    ty_seal_collect_vectors(esc);
    ty_seal_compute_hyperseal(esc);

    bool8 ok = ty_memeq(esc->hyperseal, saved, TYKID_HYPERSEAL_BYTES);

    ty_memzero_secure(saved, sizeof(saved));
    return ok ? TYKID_OK : TYKID_ERR_SEAL_BROKEN;
}

static TYKID_ALWAYS_INL tykid_status_t
ty_ext_seal_verify(const ty_ext_seal_ctx_t *esc)
{
    if (!esc->frozen) return TYKID_ERR_SEAL_BROKEN;
    bool8 ok = ty_memeq(esc->hyperseal, esc->hyperseal_prev, TYKID_HYPERSEAL_BYTES);
    return ok ? TYKID_OK : TYKID_ERR_SEAL_BROKEN;
}

#endif
