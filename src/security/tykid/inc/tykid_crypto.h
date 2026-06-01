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

#include "tykid_internal.h"

#define SIPHASH_MAGIC_0  0x736f6d6570736575ULL
#define SIPHASH_MAGIC_1  0x646f72616e646f6dULL
#define SIPHASH_MAGIC_2  0x6c7967656e657261ULL
#define SIPHASH_MAGIC_3  0x7465646279746573ULL

#define SIPROUND(v0,v1,v2,v3)        \
    do {                              \
        v0 += v1; v1 = ty_rotl64(v1,13); v1 ^= v0; v0 = ty_rotl64(v0,32); \
        v2 += v3; v3 = ty_rotl64(v3,16); v3 ^= v2;                          \
        v0 += v3; v3 = ty_rotl64(v3,21); v3 ^= v0;                          \
        v2 += v1; v1 = ty_rotl64(v1,17); v1 ^= v2; v2 = ty_rotl64(v2,32); \
    } while(0)

TYKID_INTERNAL u64
ty_siphash24(const u8 *key16, const void *data, usz len)
{
    const u8 *p = (const u8 *)data;

    u64 k0 = ty_get_u64le(key16);
    u64 k1 = ty_get_u64le(key16 + 8);

    u64 v0 = k0 ^ SIPHASH_MAGIC_0;
    u64 v1 = k1 ^ SIPHASH_MAGIC_1;
    u64 v2 = k0 ^ SIPHASH_MAGIC_2;
    u64 v3 = k1 ^ SIPHASH_MAGIC_3;

    usz blocks = len >> 3;
    for (usz i = 0; i < blocks; i++, p += 8) {
        u64 m = ty_get_u64le(p);
        v3 ^= m;
        SIPROUND(v0, v1, v2, v3);
        SIPROUND(v0, v1, v2, v3);
        v0 ^= m;
    }

    u64 last = (u64)(len & 0xFF) << 56;
    usz rem   = len & 7;
    switch (rem) {
 case 7: last |= (u64)p[6] << 48;
 case 6: last |= (u64)p[5] << 40;
 case 5: last |= (u64)p[4] << 32;
        case 4: last |= (u64)ty_get_u32le(p); break;
 case 3: last |= (u64)p[2] << 16;
 case 2: last |= (u64)p[1] << 8;
        case 1: last |= (u64)p[0];       break;
        default: break;
    }

    v3 ^= last;
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    v0 ^= last;

    v2 ^= 0xFFULL;
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);

    return v0 ^ v1 ^ v2 ^ v3;
}

TYKID_INTERNAL void
ty_siphash128_pub(const u8 *key16, const u8 *data, usz len, u8 out[16])
{
    const u8 *p = data;

    u64 k0 = ty_get_u64le(key16);
    u64 k1 = ty_get_u64le(key16 + 8);

    u64 v0 = k0 ^ SIPHASH_MAGIC_0;
    u64 v1 = k1 ^ SIPHASH_MAGIC_1;
    u64 v2 = k0 ^ SIPHASH_MAGIC_2;
    u64 v3 = k1 ^ SIPHASH_MAGIC_3;
    v1 ^= 0xEEULL;

    usz blocks = len >> 3;
    for (usz i = 0; i < blocks; i++, p += 8) {
        u64 m = ty_get_u64le(p);
        v3 ^= m;
        SIPROUND(v0, v1, v2, v3);
        SIPROUND(v0, v1, v2, v3);
        v0 ^= m;
    }

    u64 last = (u64)(len & 0xFF) << 56;
    usz rem = len & 7;
    switch (rem) {
    case 7: last |= (u64)p[6] << 48;
    case 6: last |= (u64)p[5] << 40;
    case 5: last |= (u64)p[4] << 32;
    case 4: last |= (u64)ty_get_u32le(p); break;
    case 3: last |= (u64)p[2] << 16;
    case 2: last |= (u64)p[1] << 8;
    case 1: last |= (u64)p[0]; break;
    default: break;
    }

    v3 ^= last;
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    v0 ^= last;

    v2 ^= 0xEEULL;
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    u64 lo = v0 ^ v1 ^ v2 ^ v3;

    v1 ^= 0xDDULL;
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    SIPROUND(v0, v1, v2, v3);
    u64 hi = v0 ^ v1 ^ v2 ^ v3;

    ty_put_u64le(out,     lo);
    ty_put_u64le(out + 8, hi);
}

static const u32 BLAKE2S_IV[8] = {
    0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
    0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static const u8 BLAKE2S_SIGMA[10][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
    {14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3},
    {11, 8,12, 0, 5, 2,15,13,10,14, 3, 6, 7, 1, 9, 4},
    { 7, 9, 3, 1,13,12,11,14, 2, 6, 5,10, 4, 0,15, 8},
    { 9, 0, 5, 7, 2, 4,10,15,14, 1,11,12, 6, 8, 3,13},
    { 2,12, 6,10, 0,11, 8, 3, 4,13, 7, 5,15,14, 1, 9},
    {12, 5, 1,15,14,13, 4,10, 0, 7, 6, 3, 9, 2, 8,11},
    {13,11, 7,14,12, 1, 3, 9, 5, 0,15, 4, 8, 6, 2,10},
    { 6,15,14, 9,11, 3, 0, 8,12, 2,13, 7, 1, 4,10, 5},
    {10, 2, 8, 4, 7, 6, 1, 5,15,11, 9,14, 3,12,13, 0}
};

#define B2S_G(r, i, a, b, c, d, m) \
    do { \
        a = a + b + m[BLAKE2S_SIGMA[r][2*i+0]]; \
        d = ty_rotr32(d ^ a, 16); \
        c = c + d; \
        b = ty_rotr32(b ^ c, 12); \
        a = a + b + m[BLAKE2S_SIGMA[r][2*i+1]]; \
        d = ty_rotr32(d ^ a, 8); \
        c = c + d; \
        b = ty_rotr32(b ^ c, 7); \
    } while(0)

static TYKID_NOINLINE void
blake2s_compress(ty_blake2s_ctx_t *S, const u8 block[BLAKE2S_BLOCKBYTES])
{
    u32 m[16], v[16];

    for (u32 i = 0; i < 16; i++)
        m[i] = ty_get_u32le(block + i * 4);

    for (u32 i = 0; i < 8; i++) v[i] = S->h[i];
    v[ 8] = BLAKE2S_IV[0];
    v[ 9] = BLAKE2S_IV[1];
    v[10] = BLAKE2S_IV[2];
    v[11] = BLAKE2S_IV[3];
    v[12] = S->t[0] ^ BLAKE2S_IV[4];
    v[13] = S->t[1] ^ BLAKE2S_IV[5];
    v[14] = S->f[0] ^ BLAKE2S_IV[6];
    v[15] = S->f[1] ^ BLAKE2S_IV[7];

    for (u32 r = 0; r < 10; r++) {
        B2S_G(r, 0, v[ 0], v[ 4], v[ 8], v[12], m);
        B2S_G(r, 1, v[ 1], v[ 5], v[ 9], v[13], m);
        B2S_G(r, 2, v[ 2], v[ 6], v[10], v[14], m);
        B2S_G(r, 3, v[ 3], v[ 7], v[11], v[15], m);
        B2S_G(r, 4, v[ 0], v[ 5], v[10], v[15], m);
        B2S_G(r, 5, v[ 1], v[ 6], v[11], v[12], m);
        B2S_G(r, 6, v[ 2], v[ 7], v[ 8], v[13], m);
        B2S_G(r, 7, v[ 3], v[ 4], v[ 9], v[14], m);
    }

    for (u32 i = 0; i < 8; i++)
        S->h[i] ^= v[i] ^ v[i + 8];
}

TYKID_INTERNAL void
ty_blake2s_init(ty_blake2s_ctx_t *S, const u8 *key, u8 klen, u8 outlen)
{
    ty_memzero_secure(S, sizeof(*S));
    for (u32 i = 0; i < 8; i++) S->h[i] = BLAKE2S_IV[i];

    u32 param0 = (u32)outlen
               | ((u32)klen << 8)
 | (1u << 16)
 | (1u << 24);
    S->h[0] ^= param0;
    S->outlen = outlen;

    if (klen > 0) {
        u8 block[BLAKE2S_BLOCKBYTES];
        ty_memzero_secure(block, sizeof(block));
        ty_memcpy(block, key, klen);
        ty_blake2s_update(S, block, BLAKE2S_BLOCKBYTES);
 ty_memzero_secure(block, sizeof(block));
    }
}

TYKID_INTERNAL void
ty_blake2s_update(ty_blake2s_ctx_t *S, const u8 *in, usz inlen)
{
    while (inlen > 0) {
        usz left = S->buflen;
        usz fill = BLAKE2S_BLOCKBYTES - left;

        if (inlen > fill) {
            ty_memcpy(S->buf + left, in, fill);
            S->buflen = 0;
            in    += fill;
            inlen -= fill;
            S->t[0] += BLAKE2S_BLOCKBYTES;
            if (S->t[0] < BLAKE2S_BLOCKBYTES) S->t[1]++;
            blake2s_compress(S, S->buf);
        } else {
            ty_memcpy(S->buf + left, in, inlen);
            S->buflen += (u32)inlen;
            inlen = 0;
        }
    }
}

TYKID_INTERNAL void
ty_blake2s_final(ty_blake2s_ctx_t *S, u8 *out)
{
    S->t[0] += S->buflen;
    if (S->t[0] < S->buflen) S->t[1]++;
    S->f[0] = 0xFFFFFFFFUL;

    for (u32 i = S->buflen; i < BLAKE2S_BLOCKBYTES; i++)
        S->buf[i] = 0;

    blake2s_compress(S, S->buf);

    for (u32 i = 0; i < (u32)(S->outlen + 3) / 4; i++)
        ty_put_u32le(out + i * 4, S->h[i]);

 ty_memzero_secure(S, sizeof(*S));
}

TYKID_INTERNAL void
ty_hmac_blake2s(const u8 *key, usz klen,
                const u8 *msg, usz mlen,
                u8 out[BLAKE2S_OUTBYTES])
{

    u8 ikey[BLAKE2S_BLOCKBYTES], okey[BLAKE2S_BLOCKBYTES];
    u8 inner[BLAKE2S_OUTBYTES];

    if (klen > BLAKE2S_BLOCKBYTES) {
        ty_blake2s_ctx_t tmp;
        ty_blake2s_init(&tmp, NULL, 0, BLAKE2S_OUTBYTES);
        ty_blake2s_update(&tmp, key, klen);
        ty_blake2s_final(&tmp, ikey);
        ty_memzero_secure(ikey + BLAKE2S_OUTBYTES,
                          BLAKE2S_BLOCKBYTES - BLAKE2S_OUTBYTES);
        klen = BLAKE2S_OUTBYTES;
    } else {
        ty_memcpy(ikey, key, klen);
        ty_memzero_secure(ikey + klen, BLAKE2S_BLOCKBYTES - klen);
    }

    for (usz i = 0; i < BLAKE2S_BLOCKBYTES; i++) {
        okey[i] = ikey[i] ^ 0x5CU;
        ikey[i] ^= 0x36U;
    }

    {
        ty_blake2s_ctx_t S;
        ty_blake2s_init(&S, NULL, 0, BLAKE2S_OUTBYTES);
        ty_blake2s_update(&S, ikey, BLAKE2S_BLOCKBYTES);
        ty_blake2s_update(&S, msg, mlen);
        ty_blake2s_final(&S, inner);
    }

    {
        ty_blake2s_ctx_t S;
        ty_blake2s_init(&S, NULL, 0, BLAKE2S_OUTBYTES);
        ty_blake2s_update(&S, okey, BLAKE2S_BLOCKBYTES);
        ty_blake2s_update(&S, inner, BLAKE2S_OUTBYTES);
        ty_blake2s_final(&S, out);
    }

    ty_memzero_secure(ikey, sizeof(ikey));
    ty_memzero_secure(okey, sizeof(okey));
    ty_memzero_secure(inner, sizeof(inner));
}

static const u8 CHACHA20_SIGMA[16] = "expand 32-byte k";

static TYKID_ALWAYS_INL void
chacha20_qr(u32 *a, u32 *b, u32 *c, u32 *d)
{
    *a += *b; *d = ty_rotl32(*d ^ *a, 16);
    *c += *d; *b = ty_rotl32(*b ^ *c, 12);
    *a += *b; *d = ty_rotl32(*d ^ *a, 8);
    *c += *d; *b = ty_rotl32(*b ^ *c, 7);
}

static TYKID_NOINLINE void
chacha20_block(u32 out[16], const u32 in[16])
{
    u32 x[16];
    ty_memcpy(x, in, 64);

    for (u32 i = 0; i < 10; i++) {

        chacha20_qr(&x[0],&x[4],&x[ 8],&x[12]);
        chacha20_qr(&x[1],&x[5],&x[ 9],&x[13]);
        chacha20_qr(&x[2],&x[6],&x[10],&x[14]);
        chacha20_qr(&x[3],&x[7],&x[11],&x[15]);

        chacha20_qr(&x[0],&x[5],&x[10],&x[15]);
        chacha20_qr(&x[1],&x[6],&x[11],&x[12]);
        chacha20_qr(&x[2],&x[7],&x[ 8],&x[13]);
        chacha20_qr(&x[3],&x[4],&x[ 9],&x[14]);
    }

    for (u32 i = 0; i < 16; i++)
        out[i] = x[i] + in[i];
}

TYKID_INTERNAL void
ty_chacha20_init(ty_chacha20_ctx_t *ctx, const u8 key[32], const u8 nonce[8])
{
    ty_memzero_secure(ctx, sizeof(*ctx));

    ctx->state[0] = ty_get_u32le(CHACHA20_SIGMA + 0);
    ctx->state[1] = ty_get_u32le(CHACHA20_SIGMA + 4);
    ctx->state[2] = ty_get_u32le(CHACHA20_SIGMA + 8);
    ctx->state[3] = ty_get_u32le(CHACHA20_SIGMA + 12);

    for (u32 i = 0; i < 8; i++)
        ctx->state[4 + i] = ty_get_u32le(key + i * 4);

    ctx->state[12] = 0;
    ctx->state[13] = 0;

    ctx->state[14] = ty_get_u32le(nonce + 0);
    ctx->state[15] = ty_get_u32le(nonce + 4);

 ctx->keystream_pos = 64;
}

TYKID_INTERNAL void
ty_chacha20_fill(ty_chacha20_ctx_t *ctx, u8 *buf, usz len)
{
    for (usz i = 0; i < len; i++) {
        if (ctx->keystream_pos >= 64) {
            u32 block[16];
            chacha20_block(block, ctx->state);
            for (u32 j = 0; j < 16; j++)
                ty_put_u32le(ctx->keystream + j * 4, block[j]);

            ctx->state[12]++;
            if (ctx->state[12] == 0) ctx->state[13]++;
            ctx->keystream_pos = 0;

            ty_memzero_secure(block, sizeof(block));
        }
        buf[i] = ctx->keystream[ctx->keystream_pos++];
    }
}

static TYKID_ALWAYS_INL u64
xoshiro256ss(u64 s[4])
{
    u64 result = ty_rotl64(s[1] * 5, 7) * 9;
    u64 t      = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t;
    s[3]  = ty_rotl64(s[3], 45);
    return result;
}

TYKID_INTERNAL tykid_status_t
ty_entropy_seed(tykid_gate_ctx_t *ctx)
{
    ty_entropy_pool_t *pool = &ctx->entropy;
    u8 raw[TYKID_ENTROPY_POOL_BYTES];
    ty_memzero_secure(raw, sizeof(raw));

    if (__ty_unlikely(!ctx->cfg.entropy_fn)) {
        return TYKID_ERR_ENTROPY_LOW;
    }
    usz got = ctx->cfg.entropy_fn(raw, TYKID_ENTROPY_POOL_BYTES);
    if (__ty_unlikely(got < 32)) {
        return TYKID_ERR_ENTROPY_LOW;
    }

    u64 seal = __TYKID_KOBALT_EXPECTED;
    u64 token = ctx->boot_token;
    for (u32 i = 0; i < 8; i++) {
        raw[i]      ^= (u8)(seal  >> (i * 8));
        raw[i + 8]  ^= (u8)(token >> (i * 8));
    }

    u8 sip_key[16];
    ty_memcpy(sip_key, raw, 16);
    for (u32 i = 0; i < 4; i++) {
        pool->state[i] = ty_siphash24(sip_key, raw + i * 8, 8)
                       ^ ty_siphash24(sip_key, raw + (i + 4) * 8, 8);

        if (__ty_unlikely(pool->state[i] == 0)) pool->state[i] = seal ^ (u64)i;
    }

    for (u32 i = 4; i < 8; i++) {
        pool->state[i] = ty_rotl64(pool->state[i-4], 37) ^ pool->state[i & 3];
    }

    pool->counter    = 0;
    pool->seed_epoch = token ^ seal;
    pool->seeded     = TYKID_TRUE;

    ty_memzero_secure(raw, sizeof(raw));
    ty_memzero_secure(sip_key, sizeof(sip_key));
    return TYKID_OK;
}

TYKID_INTERNAL u64
ty_entropy_u64(tykid_gate_ctx_t *ctx)
{
    ty_entropy_pool_t *pool = &ctx->entropy;

    u64 r0 = xoshiro256ss(pool->state);
    u64 r1 = xoshiro256ss(pool->state + 4);
    pool->counter++;
    return r0 ^ ty_rotl64(r1, 31) ^ ty_rotl64(pool->counter * pool->seed_epoch, 17);
}

TYKID_INTERNAL void
ty_entropy_fill(tykid_gate_ctx_t *ctx, void *buf, usz len)
{
    u8 *out = (u8 *)buf;
    while (len >= 8) {
        u64 v = ty_entropy_u64(ctx);
        ty_put_u64le(out, v);
        out += 8; len -= 8;
    }
    if (len > 0) {
        u64 v = ty_entropy_u64(ctx);
        for (usz i = 0; i < len; i++, out++, v >>= 8)
            *out = (u8)(v & 0xFF);
    }
}

TYKID_INTERNAL tykid_status_t
ty_derive_session_keys(tykid_gate_ctx_t *ctx)
{
    u8 salt[8], ikm[32], prk[BLAKE2S_OUTBYTES];

    ty_put_u64le(salt, ctx->boot_token);

    ty_entropy_fill(ctx, ikm, 32);

    ty_hmac_blake2s(salt, 8, ikm, 32, prk);

    {
        static const u8 label_a[] = { 'T','Y','K','I','D','-','H','M','A','C',
                                       '-','K','E','Y', 0x01 };
        ty_hmac_blake2s(prk, BLAKE2S_OUTBYTES, label_a, sizeof(label_a),
                        ctx->hmac_key);
    }

    {
        static const u8 label_b[] = { 'T','Y','K','I','D','-','S','E','S','S',
                                       '-','K','E','Y', 0x02 };
        ty_hmac_blake2s(prk, BLAKE2S_OUTBYTES, label_b, sizeof(label_b),
                        ctx->session_key);
    }

    {
        u8 nonce[8];
        ty_put_u64le(nonce, ty_entropy_u64(ctx));
        ty_chacha20_init(&ctx->csprng, ctx->session_key, nonce);
        ty_memzero_secure(nonce, sizeof(nonce));
    }

    ty_memzero_secure(salt, sizeof(salt));
    ty_memzero_secure(ikm,  sizeof(ikm));
    ty_memzero_secure(prk,  sizeof(prk));
    return TYKID_OK;
}
