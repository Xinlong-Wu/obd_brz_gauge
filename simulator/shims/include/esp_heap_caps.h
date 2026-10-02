#pragma once
/* Simulator shim: heap_caps_* → libc malloc family. Capability masks exist
 * only so MALLOC_CAP_SPIRAM etc. compile; they are ignored. */
#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_EXEC        (1 << 0)
#define MALLOC_CAP_32BIT       (1 << 1)
#define MALLOC_CAP_8BIT        (1 << 2)
#define MALLOC_CAP_DMA         (1 << 3)
#define MALLOC_CAP_PID2        (1 << 4)
#define MALLOC_CAP_PID3        (1 << 5)
#define MALLOC_CAP_PID4        (1 << 6)
#define MALLOC_CAP_PID5        (1 << 7)
#define MALLOC_CAP_PID6        (1 << 8)
#define MALLOC_CAP_PID7        (1 << 9)
#define MALLOC_CAP_SPIRAM      (1 << 10)
#define MALLOC_CAP_INTERNAL    (1 << 11)
#define MALLOC_CAP_DEFAULT     (1 << 12)
#define MALLOC_CAP_INVALID     (1 << 31)

void *heap_caps_malloc(size_t size, uint32_t caps);
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void *heap_caps_realloc(void *ptr, size_t size, uint32_t caps);
void heap_caps_free(void *ptr);
