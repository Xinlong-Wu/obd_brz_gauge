#pragma once
/* Simulator shim: PRNG (xorshift; no libc dependency differences). */
#include <stdint.h>
#include <stdbool.h>

uint32_t esp_random(void);

/* Screenshot-regression determinism: pin the PRNG to a fixed state so
 * fake-data jitter renders identically run to run. Must be called before
 * the first esp_random(); returns false if the PRNG was already seeded. */
bool sim_esp_random_seed(uint64_t seed);
