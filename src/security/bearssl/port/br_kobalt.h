#pragma once
#include <bearssl.h>
#include <stdint.h>

/* Initialise the BearSSL entropy pool.  Must be called once before any
 * TLS handshake, after the APIC timer (TSC calibration) has run.          */
int net_bearssl_entropy_init(void);

/* Return a pointer to the global seeded PRNG for use in handshake
 * contexts that need br_prng_seeded_init().                               */
const br_prng_class **net_bearssl_prng(void);