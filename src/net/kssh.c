/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <kssh.h>
#include <crypto.h>
#include <sched.h>
#include <apic_timer.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <kernel.h>

#define KSSH_INTEGRITY_VECTOR     "Kobalt Integrity Vector"
#define KSSH_INTEGRITY_VECTOR_LEN (27U)

#define KSSH_INTERVAL_TICKS       (1000U)

volatile kssh_digest_hook_fn kssh_integrity_hook = NULL;

static sched_thread_t *g_crypto_thread = NULL;

static volatile uint64_t g_crypto_wake_tick = 0;

void kssh_tick_notify(uint64_t now_ticks)
{
    sched_thread_t *t = g_crypto_thread;
    if (!t) return;
    if (now_ticks >= g_crypto_wake_tick)
        sched_unblock(t);
}

static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void crypto_daemon(void *arg)
{
    (void)arg;

    g_crypto_thread = sched_current();

    uint8_t digest  [CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t baseline[CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t tsc_buf [8];

    int      baseline_set = 0;
    uint64_t iteration    = 0;

    for (;;) {
        uint64_t now = apic_timer_ticks();
        g_crypto_wake_tick = now + KSSH_INTERVAL_TICKS;
        __asm__ volatile ("" ::: "memory");

        uint64_t rflags;
        __asm__ volatile ("pushfq; pop %0; cli" : "=rm"(rflags) :: "memory");
        sched_block();
        __asm__ volatile ("push %0; popfq" :: "rm"(rflags) : "memory", "cc");

        uint64_t tsc = rdtsc();
        __builtin_memcpy(tsc_buf, &tsc, 8);

        crypto_sha256_ctx ctx;
        crypto_sha256_init  (&ctx);
        crypto_sha256_update(&ctx, KSSH_INTEGRITY_VECTOR, KSSH_INTEGRITY_VECTOR_LEN);
        crypto_sha256_update(&ctx, tsc_buf, sizeof(tsc_buf));
        crypto_sha256_final (&ctx, digest);

        if (!baseline_set) {
            __builtin_memcpy(baseline, digest, CRYPTO_SHA256_DIGEST_SIZE);
            baseline_set = 1;
        } else {
            (void)baseline;
        }

        kssh_digest_hook_fn hook = kssh_integrity_hook;
        if (hook)
            hook(digest, iteration);

        __builtin_memset(tsc_buf, 0, sizeof(tsc_buf));
        iteration++;
    }
}