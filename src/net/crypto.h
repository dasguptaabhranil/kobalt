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

#pragma once

#include <stdint.h>
#include <stddef.h>

/* ── Return codes ─────────────────────────────────────────────────────────── */
#define CRYPTO_OK           (0)
#define CRYPTO_ERR_AUTH     (-1)    /* Poly1305 tag mismatch (AEAD decrypt)   */

/* ── SHA-256 ──────────────────────────────────────────────────────────────── */

#define CRYPTO_SHA256_BLOCK_SIZE    (64U)
#define CRYPTO_SHA256_DIGEST_SIZE   (32U)

typedef struct {
    uint32_t state[8];      /* H0..H7 (running hash state)                   */
    uint64_t count;         /* total bits processed (for padding length)      */
    uint8_t  buf[CRYPTO_SHA256_BLOCK_SIZE];  /* partial-block buffer          */
    uint32_t buf_len;       /* bytes currently in buf                         */
} crypto_sha256_ctx;

void crypto_sha256_init  (crypto_sha256_ctx *ctx);
void crypto_sha256_update(crypto_sha256_ctx *ctx,
                          const void *data, size_t len);
void crypto_sha256_final (crypto_sha256_ctx *ctx,
                          uint8_t digest[CRYPTO_SHA256_DIGEST_SIZE]);

/* Single-call convenience wrapper. */
void crypto_sha256(const void *data, size_t len,
                   uint8_t digest[CRYPTO_SHA256_DIGEST_SIZE]);

/* ── HMAC-SHA-256 ─────────────────────────────────────────────────────────── */

#define CRYPTO_HMAC_SHA256_TAG_SIZE     CRYPTO_SHA256_DIGEST_SIZE

typedef struct {
    crypto_sha256_ctx inner;
    crypto_sha256_ctx outer;
} crypto_hmac_sha256_ctx;

void crypto_hmac_sha256_init  (crypto_hmac_sha256_ctx *ctx,
                                const void *key, size_t key_len);
void crypto_hmac_sha256_update(crypto_hmac_sha256_ctx *ctx,
                                const void *data, size_t len);
void crypto_hmac_sha256_final (crypto_hmac_sha256_ctx *ctx,
                                uint8_t tag[CRYPTO_HMAC_SHA256_TAG_SIZE]);

/* Single-call convenience wrapper. */
void crypto_hmac_sha256(const void *key,  size_t key_len,
                        const void *data, size_t data_len,
                        uint8_t tag[CRYPTO_HMAC_SHA256_TAG_SIZE]);

/* ── ChaCha20 ─────────────────────────────────────────────────────────────── */

#define CRYPTO_CHACHA20_KEY_SIZE    (32U)   /* 256-bit key                   */
#define CRYPTO_CHACHA20_NONCE_SIZE  (12U)   /* 96-bit nonce (RFC 8439)        */
#define CRYPTO_CHACHA20_BLOCK_SIZE  (64U)   /* keystream block                */

typedef struct {
    uint32_t state[16];     /* initial state matrix (key, counter, nonce)     */
    uint8_t  keystream[CRYPTO_CHACHA20_BLOCK_SIZE];  /* current block buffer  */
    uint32_t keystream_pos; /* bytes consumed from keystream[]                */
} crypto_chacha20_ctx;

/* Initialise ChaCha20 state.  counter is the initial block counter
 * (RFC 8439 §2.4: use 0 for Poly1305 key generation, 1 for encryption).    */
void crypto_chacha20_init(crypto_chacha20_ctx *ctx,
                          const uint8_t key[CRYPTO_CHACHA20_KEY_SIZE],
                          const uint8_t nonce[CRYPTO_CHACHA20_NONCE_SIZE],
                          uint32_t counter);

/* XOR len bytes at in with the ChaCha20 keystream, write to out.
 * in and out may alias (same pointer = in-place encryption/decryption).     */
void crypto_chacha20_xor(crypto_chacha20_ctx *ctx,
                         const void *in, void *out, size_t len);

/* ── Poly1305 ─────────────────────────────────────────────────────────────── */

#define CRYPTO_POLY1305_KEY_SIZE    (32U)   /* one-time key: r (16 B) + s (16 B) */
#define CRYPTO_POLY1305_TAG_SIZE    (16U)   /* authentication tag                 */

typedef struct {
    uint32_t r[5];          /* clamped r key, 26-bit limbs                   */
    uint32_t rr[5];         /* precomputed: rr[i] = r[i] * 5                 */
    uint32_t h[5];          /* running accumulator, 26-bit limbs             */
    uint32_t s[4];          /* s key as four LE 32-bit words                 */
    uint8_t  buf[16];       /* partial-block buffer                          */
    uint32_t buf_len;       /* bytes in buf                                  */
    int      finalised;     /* 1 after crypto_poly1305_final()               */
} crypto_poly1305_ctx;

void crypto_poly1305_init  (crypto_poly1305_ctx *ctx,
                             const uint8_t key[CRYPTO_POLY1305_KEY_SIZE]);
void crypto_poly1305_update(crypto_poly1305_ctx *ctx,
                             const void *data, size_t len);
void crypto_poly1305_final (crypto_poly1305_ctx *ctx,
                             uint8_t tag[CRYPTO_POLY1305_TAG_SIZE]);

/* ── ChaCha20-Poly1305 AEAD (RFC 8439 §2.8) ─────────────────────────────── *
 *
 * Authenticated Encryption with Additional Data.
 *
 *   encrypt: plaintext → ciphertext (same length) + 16-byte tag
 *   decrypt: ciphertext + tag → plaintext, or CRYPTO_ERR_AUTH on mismatch
 *
 * Both functions accept a NULL aad pointer when aad_len == 0.
 *
 * tag_out (encrypt) and tag_in (decrypt) must point to 16-byte buffers.
 * ciphertext and plaintext may alias their respective input buffers for
 * in-place operation, provided the pointers are identical (not just
 * overlapping at a different offset).
 */
int  crypto_chacha20poly1305_encrypt(
        const uint8_t key[CRYPTO_CHACHA20_KEY_SIZE],
        const uint8_t nonce[CRYPTO_CHACHA20_NONCE_SIZE],
        const void   *aad,         size_t aad_len,
        const void   *plaintext,   size_t pt_len,
        void         *ciphertext,
        uint8_t       tag_out[CRYPTO_POLY1305_TAG_SIZE]);

int  crypto_chacha20poly1305_decrypt(
        const uint8_t key[CRYPTO_CHACHA20_KEY_SIZE],
        const uint8_t nonce[CRYPTO_CHACHA20_NONCE_SIZE],
        const void   *aad,         size_t aad_len,
        const void   *ciphertext,  size_t ct_len,
        void         *plaintext,
        const uint8_t tag_in[CRYPTO_POLY1305_TAG_SIZE]);