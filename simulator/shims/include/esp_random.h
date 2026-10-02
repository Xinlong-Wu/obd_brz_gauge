#pragma once
/* Simulator shim: PRNG (xorshift; no libc dependency differences). */
#include <stdint.h>

uint32_t esp_random(void);
