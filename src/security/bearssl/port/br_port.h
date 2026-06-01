/* kobalt_br_port.h — freestanding overrides for BearSSL on Kobalt
 *
 * Included before any BearSSL translation unit via -include (see Makefile).
 * Redirects the handful of libc symbols BearSSL uses to Kobalt's own
 * implementations so we never link against a hosted runtime.
 */
#pragma once

/* ── Standard types ────────────────────────────────────────────────────── */
#include <stddef.h>   /* size_t, NULL                                       */
#include <stdint.h>   /* uint8_t … uint64_t                                 */

/* ── String primitives ─────────────────────────────────────────────────── *
 * Kobalt's src/inc/string.h already declares memcpy/memset/memmove/memcmp/
 * strlen.  Including it here satisfies every BearSSL TU that does
 * #include <string.h> through inner.h.                                     */
#include <string.h>

/* ── Disable BearSSL's optional OS hooks ───────────────────────────────── *
 * BearSSL probes for POSIX / Windows entropy sources at compile time.
 * Define these guards so it falls back to the br_prng_seeded_init() path
 * that we override with our own RDRAND-based source (see br_entropy.c).   */
#define BR_USE_URANDOM   0
#define BR_USE_WIN32_RAND 0

/* ── No float / no printf ──────────────────────────────────────────────── */
#define BR_POWER_ASM_MACROS 0