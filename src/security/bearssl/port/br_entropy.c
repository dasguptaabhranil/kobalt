/* br_entropy.c — Kobalt entropy backend for BearSSL
 *
 * Uses RDRAND (Intel DRNG, available on all x86_64 since Ivy Bridge) to
 * seed BearSSL's HMAC-DRBG.  Falls back to a RDTSC mix if RDRAND fails
 * five consecutive times (should never happen on modern hardware, but we
 * handle it defensively).
 *
 * Called once from net_bearssl_init() before any TLS handshake.
 */

#include "bearssl.h"   /* br_hmac_drbg_context, br_prng_seeded_init        */
#include <stdint.h>
#include <string.h>

/* ── RDRAND wrapper ────────────────────────────────────────────────────── */
static int rdrand64(uint64_t *out)
{
    unsigned char ok;
    __asm__ volatile (
        "rdrand %0\n\t"
        "setc   %1\n\t"
        : "=r"(*out), "=qm"(ok)
        :
        : "cc"
    );
    return (int)ok;
}

/* Collect 64 bytes of RDRAND output into buf[].
 * Returns 0 on success, -1 if RDRAND is unavailable / consistently fails. */
static int collect_entropy(uint8_t buf[64])
{
    for (int word = 0; word < 8; word++) {
        uint64_t v = 0;
        int ok = 0;
        for (int retry = 0; retry < 5; retry++) {
            if (rdrand64(&v)) { ok = 1; break; }
        }
        if (!ok) {
            /* RDRAND failed — fall back to RDTSC xor-folded with position. */
            uint32_t lo, hi;
            __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
            v = ((uint64_t)hi << 32 | lo) ^ ((uint64_t)word * 0x9e3779b97f4a7c15ULL);
        }
        __builtin_memcpy(buf + word * 8, &v, 8);
    }
    return 0;
}

/* Global HMAC-DRBG context seeded once at init time.
 * All subsequent BearSSL PRNG calls draw from this. */
static br_hmac_drbg_context g_drbg;
static int                   g_drbg_ready = 0;

/* net_bearssl_entropy_init — call once before any TLS operation. */
int net_bearssl_entropy_init(void)
{
    uint8_t seed[64];
    collect_entropy(seed);
    br_hmac_drbg_init(&g_drbg, &br_sha256_vtable, seed, sizeof(seed));
    __builtin_memset(seed, 0, sizeof(seed));   /* scrub seed off the stack  */
    g_drbg_ready = 1;
    return 0;
}

/* br_prng_seeded_init — BearSSL calls this when it needs a PRNG.
 * We redirect it to our pre-seeded DRBG.                                  */
const br_prng_class **br_prng_seeded_init(void)
{
    if (!g_drbg_ready)
        net_bearssl_entropy_init();
    return (const br_prng_class **)&g_drbg;
}