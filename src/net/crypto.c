/* Copyright (c) 2026  Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <crypto.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>     /* memcpy, memset */


/* ══════════════════════════════════════════════════════════════════════════════
 * 1  Byte-order helpers
 * ══════════════════════════════════════════════════════════════════════════════
 *
 * These deliberately avoid relying on __builtin_bswap* or platform headers so
 * that the intent is unambiguous and the compiler can still fold them into
 * single MOVBE / BSWAP instructions with -O2.
 */

static inline uint32_t load_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | (uint32_t)p[1] <<  8
         | (uint32_t)p[2] << 16
         | (uint32_t)p[3] << 24;
}

static inline uint32_t load_u32_be(const uint8_t *p)
{
    return (uint32_t)p[0] << 24
         | (uint32_t)p[1] << 16
         | (uint32_t)p[2] <<  8
         | (uint32_t)p[3];
}

static inline void store_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)( v        & 0xFFU);
    p[1] = (uint8_t)((v >>  8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static inline void store_u32_be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xFFU);
    p[1] = (uint8_t)((v >> 16) & 0xFFU);
    p[2] = (uint8_t)((v >>  8) & 0xFFU);
    p[3] = (uint8_t)( v        & 0xFFU);
}

static inline void store_u64_le(uint8_t *p, uint64_t v)
{
    store_u32_le(p,     (uint32_t)( v        & 0xFFFFFFFFU));
    store_u32_le(p + 4, (uint32_t)((v >> 32) & 0xFFFFFFFFU));
}


/* ══════════════════════════════════════════════════════════════════════════════
 * 2  SHA-256   (FIPS 180-4)
 * ══════════════════════════════════════════════════════════════════════════════
 *
 * SHA-256 operates on 512-bit (64-byte) message blocks, producing a 256-bit
 * (32-byte) digest.  The algorithm maintains eight 32-bit working variables
 * (a–h) processed through 64 rounds driven by a message schedule W[0..63].
 *
 * Round function:
 *   T1 = h + Σ1(e) + Ch(e,f,g) + K[t] + W[t]
 *   T2 = Σ0(a) + Maj(a,b,c)
 *   h = g; g = f; f = e; e = d + T1;
 *   d = c; c = b; b = a; a = T1 + T2;
 *
 * Where:
 *   Ch(e,f,g)  = (e & f) ^ (~e & g)         (choice)
 *   Maj(a,b,c) = (a & b) ^ (a & c) ^ (b & c) (majority)
 *   Σ0(a)      = ROTR(a,2) ^ ROTR(a,13) ^ ROTR(a,22)
 *   Σ1(e)      = ROTR(e,6) ^ ROTR(e,11) ^ ROTR(e,25)
 *   σ0(x)      = ROTR(x,7) ^ ROTR(x,18) ^ (x >> 3)
 *   σ1(x)      = ROTR(x,17) ^ ROTR(x,19) ^ (x >> 10)
 */

/* Initial hash values H0..H7: first 32 bits of the fractional parts of the
 * square roots of the first eight prime numbers.                             */
static const uint32_t sha256_iv[8] = {
    0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
    0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U,
};

/* Round constants K[0..63]: first 32 bits of the fractional parts of the
 * cube roots of the first 64 prime numbers.                                  */
static const uint32_t sha256_k[64] = {
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
    0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
    0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
    0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
    0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
    0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
    0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
    0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
    0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
    0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
    0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
    0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
    0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
    0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
};

#define SHA256_ROTR(x, n)   (((x) >> (n)) | ((x) << (32 - (n))))

#define SHA256_CH(e,f,g)    (((e) & (f)) ^ (~(e) & (g)))
#define SHA256_MAJ(a,b,c)   (((a) & (b)) ^ ((a) & (c)) ^ ((b) & (c)))
#define SHA256_SIG0(a)      (SHA256_ROTR(a, 2) ^ SHA256_ROTR(a,13) ^ SHA256_ROTR(a,22))
#define SHA256_SIG1(e)      (SHA256_ROTR(e, 6) ^ SHA256_ROTR(e,11) ^ SHA256_ROTR(e,25))
#define SHA256_GAM0(x)      (SHA256_ROTR(x, 7) ^ SHA256_ROTR(x,18) ^ ((x) >>  3))
#define SHA256_GAM1(x)      (SHA256_ROTR(x,17) ^ SHA256_ROTR(x,19) ^ ((x) >> 10))

/*
 * sha256_compress — process one 64-byte block into the running hash state
 *
 * block must be exactly CRYPTO_SHA256_BLOCK_SIZE bytes.
 * All arithmetic is mod 2^32 per the specification.
 */
static void sha256_compress(uint32_t state[8], const uint8_t block[64])
{
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h, t1, t2;
    unsigned int t;

    /* ── Message schedule ─────────────────────────────────────────────────── *
     * W[0..15] are the message words (big-endian from the block bytes).
     * W[16..63] are derived via the σ0/σ1 mixing functions.                  */
    for (t = 0; t < 16; t++)
        W[t] = load_u32_be(block + t * 4);

    for (t = 16; t < 64; t++)
        W[t] = SHA256_GAM1(W[t - 2])  + W[t - 7]
             + SHA256_GAM0(W[t - 15]) + W[t - 16];

    /* ── Working variables initialised from current state ────────────────── */
    a = state[0];  b = state[1];  c = state[2];  d = state[3];
    e = state[4];  f = state[5];  g = state[6];  h = state[7];

    /* ── 64 rounds ────────────────────────────────────────────────────────── */
    for (t = 0; t < 64; t++) {
        t1 = h + SHA256_SIG1(e) + SHA256_CH(e, f, g) + sha256_k[t] + W[t];
        t2 = SHA256_SIG0(a) + SHA256_MAJ(a, b, c);
        h = g;  g = f;  f = e;  e = d + t1;
        d = c;  c = b;  b = a;  a = t1 + t2;
    }

    /* ── Accumulate into running state ───────────────────────────────────── */
    state[0] += a;  state[1] += b;  state[2] += c;  state[3] += d;
    state[4] += e;  state[5] += f;  state[6] += g;  state[7] += h;
}


void crypto_sha256_init(crypto_sha256_ctx *ctx)
{
    for (int i = 0; i < 8; i++)
        ctx->state[i] = sha256_iv[i];
    ctx->count   = 0;
    ctx->buf_len = 0;
}


void crypto_sha256_update(crypto_sha256_ctx *ctx,
                          const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    ctx->count += (uint64_t)len * 8;  /* accumulate bit count before padding */

    /* ── Drain any partial block left from a prior call ─────────────────── */
    if (ctx->buf_len > 0) {
        uint32_t need = CRYPTO_SHA256_BLOCK_SIZE - ctx->buf_len;
        if (len < need) {
            memcpy(ctx->buf + ctx->buf_len, p, len);
            ctx->buf_len += (uint32_t)len;
            return;
        }
        memcpy(ctx->buf + ctx->buf_len, p, need);
        sha256_compress(ctx->state, ctx->buf);
        ctx->buf_len = 0;
        p   += need;
        len -= need;
    }

    /* ── Process full blocks directly from the caller's buffer ──────────── */
    while (len >= CRYPTO_SHA256_BLOCK_SIZE) {
        sha256_compress(ctx->state, p);
        p   += CRYPTO_SHA256_BLOCK_SIZE;
        len -= CRYPTO_SHA256_BLOCK_SIZE;
    }

    /* ── Buffer any remaining tail bytes ─────────────────────────────────── */
    if (len > 0) {
        memcpy(ctx->buf, p, len);
        ctx->buf_len = (uint32_t)len;
    }
}


void crypto_sha256_final(crypto_sha256_ctx *ctx,
                         uint8_t digest[CRYPTO_SHA256_DIGEST_SIZE])
{
    uint8_t pad[CRYPTO_SHA256_BLOCK_SIZE];
    uint32_t pad_len;

    /* ── Append the mandatory 0x80 byte ──────────────────────────────────── */
    ctx->buf[ctx->buf_len++] = 0x80U;

    /* If there is not enough room for the 8-byte length field in this block,
     * pad with zeros and compress, then start a fresh all-zero block.        */
    if (ctx->buf_len > 56U) {
        memset(ctx->buf + ctx->buf_len, 0,
               CRYPTO_SHA256_BLOCK_SIZE - ctx->buf_len);
        sha256_compress(ctx->state, ctx->buf);
        ctx->buf_len = 0;
    }

    /* ── Build the final padding block ───────────────────────────────────── */
    memset(pad, 0, sizeof(pad));
    memcpy(pad, ctx->buf, ctx->buf_len);

    /* Append the 64-bit message length in bits, big-endian (FIPS 180-4 §5.1.1). */
    pad_len = CRYPTO_SHA256_BLOCK_SIZE;
    pad[56] = (uint8_t)((ctx->count >> 56) & 0xFFU);
    pad[57] = (uint8_t)((ctx->count >> 48) & 0xFFU);
    pad[58] = (uint8_t)((ctx->count >> 40) & 0xFFU);
    pad[59] = (uint8_t)((ctx->count >> 32) & 0xFFU);
    pad[60] = (uint8_t)((ctx->count >> 24) & 0xFFU);
    pad[61] = (uint8_t)((ctx->count >> 16) & 0xFFU);
    pad[62] = (uint8_t)((ctx->count >>  8) & 0xFFU);
    pad[63] = (uint8_t)( ctx->count        & 0xFFU);
    (void)pad_len;

    sha256_compress(ctx->state, pad);

    /* ── Serialise the final state big-endian ────────────────────────────── */
    for (int i = 0; i < 8; i++)
        store_u32_be(digest + i * 4, ctx->state[i]);

    /* Scrub the working state to prevent information leakage via the stack.  */
    memset(ctx, 0, sizeof(*ctx));
}


void crypto_sha256(const void *data, size_t len,
                   uint8_t digest[CRYPTO_SHA256_DIGEST_SIZE])
{
    crypto_sha256_ctx ctx;
    crypto_sha256_init(&ctx);
    crypto_sha256_update(&ctx, data, len);
    crypto_sha256_final(&ctx, digest);
}


/* ══════════════════════════════════════════════════════════════════════════════
 * 3  HMAC-SHA-256   (RFC 2104)
 * ══════════════════════════════════════════════════════════════════════════════
 *
 * HMAC(K, m) = H((K' ⊕ opad) ‖ H((K' ⊕ ipad) ‖ m))
 *
 * where K' is the key padded/truncated to the block size (64 bytes for
 * SHA-256), ipad = 0x36 repeated, opad = 0x5C repeated.
 */

void crypto_hmac_sha256_init(crypto_hmac_sha256_ctx *ctx,
                              const void *key, size_t key_len)
{
    uint8_t  k_prime[CRYPTO_SHA256_BLOCK_SIZE];
    uint8_t  ipad_key[CRYPTO_SHA256_BLOCK_SIZE];
    uint8_t  opad_key[CRYPTO_SHA256_BLOCK_SIZE];

    /* Derive K': if key > block size, hash it first; otherwise zero-extend. */
    memset(k_prime, 0, sizeof(k_prime));
    if (key_len > CRYPTO_SHA256_BLOCK_SIZE) {
        crypto_sha256(key, key_len, k_prime);
    } else {
        memcpy(k_prime, key, key_len);
    }

    /* XOR with ipad (0x36) and opad (0x5C) to form the two padded keys.     */
    for (size_t i = 0; i < CRYPTO_SHA256_BLOCK_SIZE; i++) {
        ipad_key[i] = k_prime[i] ^ 0x36U;
        opad_key[i] = k_prime[i] ^ 0x5CU;
    }

    /* Inner hash: H(K' ⊕ ipad || message) — message fed via _update().     */
    crypto_sha256_init(&ctx->inner);
    crypto_sha256_update(&ctx->inner, ipad_key, CRYPTO_SHA256_BLOCK_SIZE);

    /* Outer hash: H(K' ⊕ opad || inner_digest) — finalised in _final().    */
    crypto_sha256_init(&ctx->outer);
    crypto_sha256_update(&ctx->outer, opad_key, CRYPTO_SHA256_BLOCK_SIZE);

    memset(k_prime,  0, sizeof(k_prime));
    memset(ipad_key, 0, sizeof(ipad_key));
    memset(opad_key, 0, sizeof(opad_key));
}


void crypto_hmac_sha256_update(crypto_hmac_sha256_ctx *ctx,
                                const void *data, size_t len)
{
    crypto_sha256_update(&ctx->inner, data, len);
}


void crypto_hmac_sha256_final(crypto_hmac_sha256_ctx *ctx,
                               uint8_t tag[CRYPTO_HMAC_SHA256_TAG_SIZE])
{
    uint8_t inner_digest[CRYPTO_SHA256_DIGEST_SIZE];

    crypto_sha256_final(&ctx->inner, inner_digest);
    crypto_sha256_update(&ctx->outer, inner_digest, sizeof(inner_digest));
    crypto_sha256_final(&ctx->outer, tag);

    memset(inner_digest, 0, sizeof(inner_digest));
}


void crypto_hmac_sha256(const void *key,  size_t key_len,
                        const void *data, size_t data_len,
                        uint8_t tag[CRYPTO_HMAC_SHA256_TAG_SIZE])
{
    crypto_hmac_sha256_ctx ctx;
    crypto_hmac_sha256_init(&ctx, key, key_len);
    crypto_hmac_sha256_update(&ctx, data, data_len);
    crypto_hmac_sha256_final(&ctx, tag);
}


/* ══════════════════════════════════════════════════════════════════════════════
 * 4  ChaCha20   (RFC 8439 §2.1)
 * ══════════════════════════════════════════════════════════════════════════════
 *
 * ChaCha20 is a stream cipher operating on a 4×4 matrix of 32-bit words.
 * The initial state is:
 *
 *   state[0..3]  = constants "expa" "nd 3" "2-by" "te k"
 *   state[4..11] = 256-bit key (8 × 32-bit LE words)
 *   state[12]    = 32-bit block counter
 *   state[13..15]= 96-bit nonce (3 × 32-bit LE words)
 *
 * Each call to chacha20_block() produces 64 bytes of keystream.  The block
 * counter is incremented after each block, allowing random-access seeking.
 *
 * The quarter-round QR(a,b,c,d) is:
 *   a += b;  d ^= a;  d <<<= 16;
 *   c += d;  b ^= c;  b <<<= 12;
 *   a += b;  d ^= a;  d <<<= 8;
 *   c += d;  b ^= c;  b <<<= 7;
 *
 * Twenty rounds = 10 column rounds + 10 diagonal rounds, applied alternately.
 */

static const uint32_t chacha20_constants[4] = {
    0x61707865U,    /* "expa" */
    0x3320646EU,    /* "nd 3" */
    0x79622D32U,    /* "2-by" */
    0x6B206574U,    /* "te k" */
};

#define CHACHA20_ROTL32(v, n)   (((v) << (n)) | ((v) >> (32 - (n))))

#define CHACHA20_QR(a, b, c, d)         \
    do {                                \
        (a) += (b);  (d) ^= (a);  (d) = CHACHA20_ROTL32((d), 16); \
        (c) += (d);  (b) ^= (c);  (b) = CHACHA20_ROTL32((b), 12); \
        (a) += (b);  (d) ^= (a);  (d) = CHACHA20_ROTL32((d),  8); \
        (c) += (d);  (b) ^= (c);  (b) = CHACHA20_ROTL32((b),  7); \
    } while (0)

/*
 * chacha20_block — generate one 64-byte keystream block into out[]
 *
 * Runs 20 rounds (10 double-rounds) on a copy of the state matrix, then
 * adds the original state to the scrambled copy (feed-forward).  The block
 * counter field (state[12]) is incremented by the caller after this returns.
 */
static void chacha20_block(const uint32_t state[16], uint8_t out[64])
{
    uint32_t x[16];
    int i;

    for (i = 0; i < 16; i++)
        x[i] = state[i];

    /* 10 double-rounds: each double-round is one column round + one diagonal. */
    for (i = 0; i < 10; i++) {
        /* Column rounds */
        CHACHA20_QR(x[0], x[4], x[ 8], x[12]);
        CHACHA20_QR(x[1], x[5], x[ 9], x[13]);
        CHACHA20_QR(x[2], x[6], x[10], x[14]);
        CHACHA20_QR(x[3], x[7], x[11], x[15]);
        /* Diagonal rounds */
        CHACHA20_QR(x[0], x[5], x[10], x[15]);
        CHACHA20_QR(x[1], x[6], x[11], x[12]);
        CHACHA20_QR(x[2], x[7], x[ 8], x[13]);
        CHACHA20_QR(x[3], x[4], x[ 9], x[14]);
    }

    /* Feed-forward addition. */
    for (i = 0; i < 16; i++)
        x[i] += state[i];

    /* Serialise as little-endian bytes. */
    for (i = 0; i < 16; i++)
        store_u32_le(out + i * 4, x[i]);
}


void crypto_chacha20_init(crypto_chacha20_ctx *ctx,
                          const uint8_t key[CRYPTO_CHACHA20_KEY_SIZE],
                          const uint8_t nonce[CRYPTO_CHACHA20_NONCE_SIZE],
                          uint32_t counter)
{
    /* constants */
    ctx->state[0]  = chacha20_constants[0];
    ctx->state[1]  = chacha20_constants[1];
    ctx->state[2]  = chacha20_constants[2];
    ctx->state[3]  = chacha20_constants[3];

    /* 256-bit key as 8 × 32-bit LE words */
    for (int i = 0; i < 8; i++)
        ctx->state[4 + i] = load_u32_le(key + i * 4);

    /* 32-bit counter */
    ctx->state[12] = counter;

    /* 96-bit nonce as 3 × 32-bit LE words */
    ctx->state[13] = load_u32_le(nonce);
    ctx->state[14] = load_u32_le(nonce + 4);
    ctx->state[15] = load_u32_le(nonce + 8);

    /* Keystream buffer starts empty — first byte requested triggers a block. */
    ctx->keystream_pos = CRYPTO_CHACHA20_BLOCK_SIZE;
}


void crypto_chacha20_xor(crypto_chacha20_ctx *ctx,
                         const void *in, void *out, size_t len)
{
    const uint8_t *src = (const uint8_t *)in;
    uint8_t       *dst = (uint8_t       *)out;

    while (len > 0) {
        /* Refill the keystream buffer when exhausted. */
        if (ctx->keystream_pos >= CRYPTO_CHACHA20_BLOCK_SIZE) {
            chacha20_block(ctx->state, ctx->keystream);
            ctx->state[12]++;           /* advance block counter */
            ctx->keystream_pos = 0;
        }

        size_t avail = CRYPTO_CHACHA20_BLOCK_SIZE - ctx->keystream_pos;
        size_t chunk = (len < avail) ? len : avail;

        for (size_t i = 0; i < chunk; i++)
            dst[i] = src[i] ^ ctx->keystream[ctx->keystream_pos + i];

        ctx->keystream_pos += (uint32_t)chunk;
        src += chunk;
        dst += chunk;
        len -= chunk;
    }
}


/* ══════════════════════════════════════════════════════════════════════════════
 * 5  Poly1305   (RFC 8439 §2.5)
 * ══════════════════════════════════════════════════════════════════════════════
 *
 * Poly1305 is a one-time MAC operating over GF(2^130 - 5).
 *
 * Key structure (32 bytes):
 *   r = key[ 0..15] clamped per RFC 8439 §2.5.1
 *   s = key[16..31] (additive constant, untransformed)
 *
 * Accumulator: h = 0; for each 16-byte block m:
 *   h = (h + m + 2^(8·block_len)) · r  mod  (2^130 - 5)
 * Final tag:  (h + s) mod 2^128, serialised little-endian.
 *
 * Representation
 * ──────────────
 * We represent 130-bit integers as five 32-bit limbs, each holding at most
 * 26 significant bits.  The five limbs cover bits [25:0], [51:26], [77:52],
 * [103:78], and [129:104] respectively.
 *
 *   value = h[0]
 *         + h[1] * 2^26
 *         + h[2] * 2^52
 *         + h[3] * 2^78
 *         + h[4] * 2^104
 *
 * The product h * r uses 64-bit intermediate values (maximum per-limb product
 * is 2^26 × 2^26 = 2^52 << 2^63) and relies on the identity
 *   2^130 ≡ 5  (mod 2^130 - 5)
 * so that the contribution of limb products whose combined exponent ≥ 130 can
 * be folded back into the low limbs by multiplication by 5.
 */

/* ── r clamping ──────────────────────────────────────────────────────────────
 *
 * RFC 8439 §2.5.1 mandates clearing specific bits of r to ensure r is
 * a valid GF(2^130-5) multiplier.  Bit positions to clear (0-indexed from
 * LSB of the 128-bit LE integer):
 *   Bits 28,60,92,124  (top 4 bits of bytes 3, 7, 11, 15)
 *   Bits 26,27,58,59,90,91,122,123  (bottom 2 bits of bytes 4, 8, 12)
 *
 * Equivalently, applied as byte-level masks:
 *   byte[3]  &= 0x0F    byte[7]  &= 0x0F
 *   byte[11] &= 0x0F    byte[15] &= 0x0F
 *   byte[4]  &= 0xFC    byte[8]  &= 0xFC
 *   byte[12] &= 0xFC
 */
static void poly1305_clamp_r(uint8_t r[16])
{
    r[ 3] &= 0x0FU;
    r[ 7] &= 0x0FU;
    r[11] &= 0x0FU;
    r[15] &= 0x0FU;
    r[ 4] &= 0xFCU;
    r[ 8] &= 0xFCU;
    r[12] &= 0xFCU;
}

/* ── Load a 16-byte block into five 26-bit limbs ─────────────────────────── */
static void poly1305_load_block(const uint8_t *p, uint32_t padbit,
                                uint32_t m[5])
{
    uint32_t c0 = load_u32_le(p);
    uint32_t c1 = load_u32_le(p +  4);
    uint32_t c2 = load_u32_le(p +  8);
    uint32_t c3 = load_u32_le(p + 12);

    m[0] =  c0                               & 0x3FFFFFFU;
    m[1] = ((c0 >> 26) | (c1 <<  6))         & 0x3FFFFFFU;
    m[2] = ((c1 >> 20) | (c2 << 12))         & 0x3FFFFFFU;
    m[3] = ((c2 >> 14) | (c3 << 18))         & 0x3FFFFFFU;
    m[4] =  (c3 >>  8) | (padbit << 24);
    /* m[4] = bits [127:104] of the block (24 bits) | padbit at bit-128.
     * padbit=1 for full 16-byte blocks (sets 2^128), padbit=0 for the final
     * partial block (where 0x01 padding encodes the high bit explicitly).    */
}

/* ── Core multiply-and-reduce step ───────────────────────────────────────── *
 *
 * Computes h = h * r mod (2^130 - 5).
 *
 * Each d[i] accumulates five 26-bit × 26-bit products, maximum ≈ 5 × 2^52
 * ≈ 2^54.3, well within uint64_t range.  After the multiply we propagate
 * carries to normalise back to 26-bit limbs.
 */
static void poly1305_multiply(uint32_t h[5],
                              const uint32_t r[5], const uint32_t rr[5])
{
    /* rr[i] = 5 * r[i]; used for the 2^130 ≡ 5 reduction.                   */
    uint64_t d[5];

    d[0] = (uint64_t)h[0]*r[0]  + (uint64_t)h[1]*rr[4]
         + (uint64_t)h[2]*rr[3] + (uint64_t)h[3]*rr[2]
         + (uint64_t)h[4]*rr[1];

    d[1] = (uint64_t)h[0]*r[1]  + (uint64_t)h[1]*r[0]
         + (uint64_t)h[2]*rr[4] + (uint64_t)h[3]*rr[3]
         + (uint64_t)h[4]*rr[2];

    d[2] = (uint64_t)h[0]*r[2]  + (uint64_t)h[1]*r[1]
         + (uint64_t)h[2]*r[0]  + (uint64_t)h[3]*rr[4]
         + (uint64_t)h[4]*rr[3];

    d[3] = (uint64_t)h[0]*r[3]  + (uint64_t)h[1]*r[2]
         + (uint64_t)h[2]*r[1]  + (uint64_t)h[3]*r[0]
         + (uint64_t)h[4]*rr[4];

    d[4] = (uint64_t)h[0]*r[4]  + (uint64_t)h[1]*r[3]
         + (uint64_t)h[2]*r[2]  + (uint64_t)h[3]*r[1]
         + (uint64_t)h[4]*r[0];

    /* ── Carry propagation ───────────────────────────────────────────────── */
    d[1] += d[0] >> 26;  h[0] = (uint32_t)(d[0] & 0x3FFFFFFU);
    d[2] += d[1] >> 26;  h[1] = (uint32_t)(d[1] & 0x3FFFFFFU);
    d[3] += d[2] >> 26;  h[2] = (uint32_t)(d[2] & 0x3FFFFFFU);
    d[4] += d[3] >> 26;  h[3] = (uint32_t)(d[3] & 0x3FFFFFFU);

    /* Carry out of limb 4 wraps around with factor 5 (2^130 ≡ 5 mod p). */
    uint64_t c = (d[4] >> 26) * 5U;
    h[4] = (uint32_t)(d[4] & 0x3FFFFFFU);

    /* Propagate the wrapped carry through limbs 0 and 1. */
    c   += h[0];
    h[0] = (uint32_t)(c & 0x3FFFFFFU);
    h[1] += (uint32_t)(c >> 26);
}

/* ── Block accumulation ──────────────────────────────────────────────────── */
static void poly1305_block(crypto_poly1305_ctx *ctx,
                            const uint8_t *data, uint32_t padbit)
{
    uint32_t m[5];
    poly1305_load_block(data, padbit, m);

    /* h += m */
    ctx->h[0] += m[0];
    ctx->h[1] += m[1];
    ctx->h[2] += m[2];
    ctx->h[3] += m[3];
    ctx->h[4] += m[4];

    poly1305_multiply(ctx->h, ctx->r, ctx->rr);
}


void crypto_poly1305_init(crypto_poly1305_ctx *ctx,
                           const uint8_t key[CRYPTO_POLY1305_KEY_SIZE])
{
    uint8_t r_bytes[16];
    memcpy(r_bytes, key, 16);
    poly1305_clamp_r(r_bytes);

    /* Load r into five 26-bit limbs. */
    uint32_t c0 = load_u32_le(r_bytes);
    uint32_t c1 = load_u32_le(r_bytes +  4);
    uint32_t c2 = load_u32_le(r_bytes +  8);
    uint32_t c3 = load_u32_le(r_bytes + 12);

    ctx->r[0] =  c0                               & 0x3FFFFFFU;
    ctx->r[1] = ((c0 >> 26) | (c1 <<  6))         & 0x3FFFFFFU;
    ctx->r[2] = ((c1 >> 20) | (c2 << 12))         & 0x3FFFFFFU;
    ctx->r[3] = ((c2 >> 14) | (c3 << 18))         & 0x3FFFFFFU;
    ctx->r[4] =  (c3 >>  8)                        & 0x3FFFFFFU;

    /* Precompute rr[i] = 5 * r[i] for the reduction step. */
    for (int i = 0; i < 5; i++)
        ctx->rr[i] = ctx->r[i] * 5U;

    /* Initialise accumulator and s. */
    for (int i = 0; i < 5; i++)
        ctx->h[i] = 0;

    for (int i = 0; i < 4; i++)
        ctx->s[i] = load_u32_le(key + 16 + i * 4);

    ctx->buf_len   = 0;
    ctx->finalised = 0;
}


void crypto_poly1305_update(crypto_poly1305_ctx *ctx,
                             const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    /* Drain partial buffer. */
    if (ctx->buf_len > 0) {
        uint32_t need = 16U - ctx->buf_len;
        if (len < need) {
            memcpy(ctx->buf + ctx->buf_len, p, len);
            ctx->buf_len += (uint32_t)len;
            return;
        }
        memcpy(ctx->buf + ctx->buf_len, p, need);
        poly1305_block(ctx, ctx->buf, 1U);  /* full block: padbit = 1 */
        ctx->buf_len = 0;
        p   += need;
        len -= need;
    }

    /* Full blocks directly from caller's buffer. */
    while (len >= 16U) {
        poly1305_block(ctx, p, 1U);
        p   += 16U;
        len -= 16U;
    }

    /* Buffer trailing bytes. */
    if (len > 0) {
        memcpy(ctx->buf, p, len);
        ctx->buf_len = (uint32_t)len;
    }
}


void crypto_poly1305_final(crypto_poly1305_ctx *ctx,
                            uint8_t tag[CRYPTO_POLY1305_TAG_SIZE])
{
    /* ── Process any remaining partial block ─────────────────────────────── *
     * For the final block (len < 16), we pad with a single 0x01 byte at
     * position buf_len and zeroes after, then set padbit = 0 (the 0x01 IS
     * the required high bit — we do NOT add 2^128 in addition).              */
    if (ctx->buf_len > 0) {
        ctx->buf[ctx->buf_len] = 0x01U;
        memset(ctx->buf + ctx->buf_len + 1, 0, 16U - ctx->buf_len - 1U);
        poly1305_block(ctx, ctx->buf, 0U);
    }

    /* ── Fully reduce h mod (2^130 - 5) ─────────────────────────────────── *
     *
     * After accumulation, limbs may be slightly outside [0, 2^26).  Two
     * passes of carry propagation normalise them.                            */
    uint64_t c;

    c = ctx->h[1] >> 26;  ctx->h[1] &= 0x3FFFFFFU;
    ctx->h[2] += (uint32_t)c;
    c = ctx->h[2] >> 26;  ctx->h[2] &= 0x3FFFFFFU;
    ctx->h[3] += (uint32_t)c;
    c = ctx->h[3] >> 26;  ctx->h[3] &= 0x3FFFFFFU;
    ctx->h[4] += (uint32_t)c;
    c = ctx->h[4] >> 26;  ctx->h[4] &= 0x3FFFFFFU;
    ctx->h[0] += (uint32_t)(c * 5U);
    c = ctx->h[0] >> 26;  ctx->h[0] &= 0x3FFFFFFU;
    ctx->h[1] += (uint32_t)c;

    /* ── Conditional reduction: if h >= p, compute h - p ────────────────── *
     *
     * Add 5 to the accumulator.  If the result overflows bit 130, h ≥ p and
     * we use the reduced value; otherwise we keep h.  The mask is derived
     * from whether g[4] overflows its 26-bit limb (i.e. g = h + 5 ≥ 2^130).
     * This comparison is data-independent (no branch on secret values).      */
    uint32_t g[5];
    g[0] = ctx->h[0] + 5U;
    c    = g[0] >> 26;  g[0] &= 0x3FFFFFFU;
    g[1] = ctx->h[1] + (uint32_t)c;
    c    = g[1] >> 26;  g[1] &= 0x3FFFFFFU;
    g[2] = ctx->h[2] + (uint32_t)c;
    c    = g[2] >> 26;  g[2] &= 0x3FFFFFFU;
    g[3] = ctx->h[3] + (uint32_t)c;
    c    = g[3] >> 26;  g[3] &= 0x3FFFFFFU;
    g[4] = ctx->h[4] + (uint32_t)c;

    /* mask = 0xFFFFFFFF if g[4] overflowed (h >= p), 0x00000000 otherwise.  */
    uint32_t mask = (uint32_t)(-(g[4] >> 26));   /* arithmetic right-shift   */
    uint32_t nmask = ~mask;
    for (int i = 0; i < 5; i++)
        ctx->h[i] = (ctx->h[i] & nmask) | (g[i] & mask);

    /* ── Reconstruct h as two 64-bit words (reassemble from 26-bit limbs) ── */
    uint64_t h0 = (uint64_t)ctx->h[0]
                | ((uint64_t)ctx->h[1] << 26)
                | ((uint64_t)ctx->h[2] << 52);

    uint64_t h1 = (ctx->h[2] >> 12)               /* high 14 bits of h2     */
                | ((uint64_t)ctx->h[3] << 14)
                | ((uint64_t)ctx->h[4] << 40);

    /* ── tag = (h + s) mod 2^128, little-endian ─────────────────────────── */
    uint64_t s0 = (uint64_t)ctx->s[0] | ((uint64_t)ctx->s[1] << 32);
    uint64_t s1 = (uint64_t)ctx->s[2] | ((uint64_t)ctx->s[3] << 32);

    h0 += s0;
    h1 += s1 + (h0 < s0 ? 1ULL : 0ULL);  /* carry */

    store_u64_le(tag,     h0);
    store_u64_le(tag + 8, h1);

    memset(ctx, 0, sizeof(*ctx));
    ctx->finalised = 1;
}


/* ══════════════════════════════════════════════════════════════════════════════
 * 6  ChaCha20-Poly1305 AEAD   (RFC 8439 §2.8)
 * ══════════════════════════════════════════════════════════════════════════════
 *
 * Authenticated Encryption with Associated Data.
 *
 * Encryption:
 *   1. poly1305_key = chacha20_block(key, nonce, counter=0)[0..31]
 *   2. ciphertext   = chacha20(key, nonce, counter=1) XOR plaintext
 *   3. tag_data     = aad ‖ pad16(aad) ‖ ciphertext ‖ pad16(ciphertext)
 *                     ‖ LE64(len(aad)) ‖ LE64(len(ciphertext))
 *   4. tag          = poly1305(poly1305_key, tag_data)
 *
 * pad16(x) is zero-padding to the next 16-byte boundary (0 bytes if already
 * aligned).
 *
 * Decryption verifies the tag over the ciphertext BEFORE decrypting,
 * preventing oracle attacks on the decryption step.
 *
 * Tag comparison
 * ──────────────
 * crypto_tag_equal() is a constant-time 16-byte comparison.  It avoids
 * early return on mismatch to prevent timing oracles that could be exploited
 * in a network context.
 */

/* Constant-time 16-byte tag comparison.  Returns 1 if equal, 0 otherwise.
 *
 * Accumulates XOR differences with OR so that any mismatch sets at least one
 * bit in diff.  The final conversion to 0/1 is branchless: if diff == 0 then
 * (diff - 1) wraps to 0xFFFFFFFF and the masked shift yields 1; otherwise the
 * subtraction produces a value whose high byte is 0xFF only if diff < 256
 * (guaranteed), and bit 0 of the result is 0.
 */
static int poly1305_tag_eq(const uint8_t a[16], const uint8_t b[16])
{
    volatile uint8_t diff = 0;
    for (int i = 0; i < 16; i++)
        diff |= a[i] ^ b[i];
    /* diff == 0 iff tags match; map 0→1, non-zero→0 without branching. */
    return (int)(((unsigned int)diff - 1U) >> 8 & 1U);
}

/* Feed aad or ciphertext into the Poly1305 state, followed by up to 15 zero
 * padding bytes to reach the next 16-byte boundary.                         */
static void aead_poly1305_feed(crypto_poly1305_ctx *mac,
                                const void *data, size_t len)
{
    static const uint8_t zero16[16] = { 0 };
    size_t pad = (16U - (len & 15U)) & 15U;

    if (len > 0)
        crypto_poly1305_update(mac, data, len);
    if (pad > 0)
        crypto_poly1305_update(mac, zero16, pad);
}

/* Append a 64-bit little-endian length field to the MAC. */
static void aead_poly1305_len(crypto_poly1305_ctx *mac, uint64_t v)
{
    uint8_t buf[8];
    store_u64_le(buf, v);
    crypto_poly1305_update(mac, buf, 8);
}


int crypto_chacha20poly1305_encrypt(
        const uint8_t key[CRYPTO_CHACHA20_KEY_SIZE],
        const uint8_t nonce[CRYPTO_CHACHA20_NONCE_SIZE],
        const void   *aad,       size_t aad_len,
        const void   *plaintext, size_t pt_len,
        void         *ciphertext,
        uint8_t       tag_out[CRYPTO_POLY1305_TAG_SIZE])
{
    crypto_chacha20_ctx  cipher;
    crypto_poly1305_ctx  mac;
    uint8_t              poly_key[CRYPTO_POLY1305_KEY_SIZE];

    /* ── Step 1: Derive the one-time Poly1305 key ────────────────────────── */
    /* Block counter = 0; first 32 bytes of the 64-byte keystream are used.  */
    crypto_chacha20_init(&cipher, key, nonce, 0);
    memset(poly_key, 0, sizeof(poly_key));
    crypto_chacha20_xor(&cipher, poly_key, poly_key, sizeof(poly_key));

    /* ── Step 2: Encrypt ─────────────────────────────────────────────────── */
    /* Block counter starts at 1 (counter 0 was consumed by key derivation). */
    crypto_chacha20_init(&cipher, key, nonce, 1);
    crypto_chacha20_xor(&cipher, plaintext, ciphertext, pt_len);

    /* ── Step 3: Authenticate ────────────────────────────────────────────── */
    crypto_poly1305_init(&mac, poly_key);
    aead_poly1305_feed(&mac, aad,        aad_len);
    aead_poly1305_feed(&mac, ciphertext, pt_len);
    aead_poly1305_len (&mac, (uint64_t)aad_len);
    aead_poly1305_len (&mac, (uint64_t)pt_len);
    crypto_poly1305_final(&mac, tag_out);

    memset(poly_key, 0, sizeof(poly_key));
    return CRYPTO_OK;
}


int crypto_chacha20poly1305_decrypt(
        const uint8_t key[CRYPTO_CHACHA20_KEY_SIZE],
        const uint8_t nonce[CRYPTO_CHACHA20_NONCE_SIZE],
        const void   *aad,        size_t aad_len,
        const void   *ciphertext, size_t ct_len,
        void         *plaintext,
        const uint8_t tag_in[CRYPTO_POLY1305_TAG_SIZE])
{
    crypto_chacha20_ctx  cipher;
    crypto_poly1305_ctx  mac;
    uint8_t              poly_key[CRYPTO_POLY1305_KEY_SIZE];
    uint8_t              expected_tag[CRYPTO_POLY1305_TAG_SIZE];

    /* ── Step 1: Derive the one-time Poly1305 key ────────────────────────── */
    crypto_chacha20_init(&cipher, key, nonce, 0);
    memset(poly_key, 0, sizeof(poly_key));
    crypto_chacha20_xor(&cipher, poly_key, poly_key, sizeof(poly_key));

    /* ── Step 2: Verify tag over the CIPHERTEXT (authenticate-then-decrypt) ─ *
     * Decryption must not begin before the tag is verified; otherwise a      *
     * padding-oracle or decryption-oracle attack becomes possible.           */
    crypto_poly1305_init(&mac, poly_key);
    aead_poly1305_feed(&mac, aad,        aad_len);
    aead_poly1305_feed(&mac, ciphertext, ct_len);
    aead_poly1305_len (&mac, (uint64_t)aad_len);
    aead_poly1305_len (&mac, (uint64_t)ct_len);
    crypto_poly1305_final(&mac, expected_tag);

    memset(poly_key, 0, sizeof(poly_key));

    if (!poly1305_tag_eq(expected_tag, tag_in)) {
        memset(expected_tag, 0, sizeof(expected_tag));
        return CRYPTO_ERR_AUTH;
    }

    /* ── Step 3: Decrypt ─────────────────────────────────────────────────── */
    crypto_chacha20_init(&cipher, key, nonce, 1);
    crypto_chacha20_xor(&cipher, ciphertext, plaintext, ct_len);

    memset(expected_tag, 0, sizeof(expected_tag));
    return CRYPTO_OK;
}