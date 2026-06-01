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

#include "../../inc/random.h"
#include "../../inc/rtc.h"
#include "../../inc/spinlock.h"
#include "../../inc/waitqueue.h"
#include "../../inc/kernel.h"
#include "../../arch/x86_64/cpuid.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define POOL_WORDS   16u
#define SEED_THRESH  128u

#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

#define QR(a, b, c, d)                      \
    a += b; d ^= a; d = ROTL32(d, 16);     \
    c += d; b ^= c; b = ROTL32(b, 12);     \
    a += b; d ^= a; d = ROTL32(d,  8);     \
    c += d; b ^= c; b = ROTL32(b,  7)

static void chacha20_block(uint32_t out[16], const uint32_t in[16])
{
    uint32_t x[16];
    memcpy(x, in, 64);
    for (int i = 0; i < 10; i++) {
        QR(x[ 0], x[ 4], x[ 8], x[12]);
        QR(x[ 1], x[ 5], x[ 9], x[13]);
        QR(x[ 2], x[ 6], x[10], x[14]);
        QR(x[ 3], x[ 7], x[11], x[15]);
        QR(x[ 0], x[ 5], x[10], x[15]);
        QR(x[ 1], x[ 6], x[11], x[12]);
        QR(x[ 2], x[ 7], x[ 8], x[13]);
        QR(x[ 3], x[ 4], x[ 9], x[14]);
    }
    for (int i = 0; i < 16; i++)
        out[i] = x[i] + in[i];
}

static uint32_t   rng_state[16];
static spinlock_t rng_lock = SPINLOCK_INIT;

static const uint32_t sigma[4] = {
    0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u
};

static void rng_set_key(const uint32_t key[8], const uint32_t nonce[3])
{
    rng_state[0] = sigma[0]; rng_state[1] = sigma[1];
    rng_state[2] = sigma[2]; rng_state[3] = sigma[3];
    for (int i = 0; i < 8; i++) rng_state[4 + i] = key[i];
    rng_state[12] = 0;
    rng_state[13] = nonce[0];
    rng_state[14] = nonce[1];
    rng_state[15] = nonce[2];
}

static void rng_fill(void *buf, size_t len)
{
    uint8_t *p = buf;
    while (len) {
        uint32_t block[16];
        chacha20_block(block, rng_state);
        if (!++rng_state[12]) ++rng_state[13];
        size_t n = len < 64u ? len : 64u;
        memcpy(p, block, n);
        p   += n;
        len -= n;
    }
}

static uint32_t   pool[POOL_WORDS];
static unsigned   entropy_bits;
static spinlock_t pool_lock = SPINLOCK_INIT;

static wait_queue_t seed_wq = WAIT_QUEUE_INIT;

static uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static int rdrand64(uint64_t *out)
{
    if (!cpuid_has_rdrand())
        return 0;
    unsigned char ok;
    __asm__ volatile(
        "rdrand %0\n\t"
        "setc   %1"
        : "=r"(*out), "=qm"(ok) :: "cc"
    );
    return ok;
}

static void pool_stir(const uint32_t *words, unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        unsigned idx = (entropy_bits + i) % POOL_WORDS;
        pool[idx] ^= words[i];
        pool[idx] ^= ROTL32(pool[(idx +  3) % POOL_WORDS],  7);
        pool[idx] ^= ROTL32(pool[(idx + 13) % POOL_WORDS], 19);
    }
}

void random_add_entropy(const void *buf, size_t len, unsigned bits)
{
    uint64_t f = spin_lock_irqsave(&pool_lock);

    const uint8_t *p = buf;
    while (len >= 4) {
        uint32_t w; memcpy(&w, p, 4);
        pool_stir(&w, 1);
        p += 4; len -= 4;
    }
    if (len) {
        uint32_t w = 0; memcpy(&w, p, len);
        pool_stir(&w, 1);
    }

    unsigned prev  = entropy_bits;
    entropy_bits  += bits;
    if (entropy_bits > 256u) entropy_bits = 256u;

    spin_unlock_irqrestore(&pool_lock, f);

    if (prev < SEED_THRESH && entropy_bits >= SEED_THRESH)
        wq_wake_all(&seed_wq);
}

static void reseed(void)
{
    uint64_t f = spin_lock_irqsave(&pool_lock);
    uint32_t key[8];
    for (int i = 0; i < 8; i++)
        key[i] = pool[i] ^ pool[i + 8];
    uint64_t ts = rdtsc();
    uint32_t nonce[3] = {
        (uint32_t)ts,
        (uint32_t)(ts >> 32),
        pool[15] ^ pool[0],
    };
    spin_unlock_irqrestore(&pool_lock, f);

    uint64_t g = spin_lock_irqsave(&rng_lock);
    rng_set_key(key, nonce);
    spin_unlock_irqrestore(&rng_lock, g);
}

static void seed_from_hw(void)
{
    uint64_t r;
    uint32_t words[8];

    for (int i = 0; i < 4; i++) {
        if (rdrand64(&r)) {
            words[i*2]     = (uint32_t)r;
            words[i*2 + 1] = (uint32_t)(r >> 32);
        } else {
            uint64_t t = rdtsc();
            words[i*2]     = (uint32_t)t;
            words[i*2 + 1] = (uint32_t)(t >> 32);
        }
    }
    random_add_entropy(words, sizeof(words), 64u);

    rtc_time_t rt;
    rtc_read(&rt);
    uint32_t rtc_words[3] = {
        (uint32_t)rt.sec  | ((uint32_t)rt.min  << 8)
                          | ((uint32_t)rt.hour << 16),
        (uint32_t)rt.mday | ((uint32_t)rt.mon  << 8),
        (uint32_t)rt.year,
    };
    random_add_entropy(rtc_words, sizeof(rtc_words), 16u);

    for (int i = 0; i < 8; i++) {
        uint64_t t = rdtsc();
        random_add_entropy(&t, sizeof(t), 2u);
    }
}

void random_init(void)
{
    memset(pool, 0, sizeof(pool));
    entropy_bits = 0;
    seed_from_hw();
    if (entropy_bits >= SEED_THRESH)
        reseed();
}

int random_seeded(void)
{
    return entropy_bits >= SEED_THRESH;
}

void urandom_read(void *buf, size_t len)
{
    if (!random_seeded())
        seed_from_hw();
    reseed();
    uint64_t f = spin_lock_irqsave(&rng_lock);
    rng_fill(buf, len);
    spin_unlock_irqrestore(&rng_lock, f);
}

void random_read(void *buf, size_t len)
{
    while (!random_seeded())
        wq_wait(&seed_wq);
    reseed();
    uint64_t f = spin_lock_irqsave(&rng_lock);
    rng_fill(buf, len);
    spin_unlock_irqrestore(&rng_lock, f);
}
