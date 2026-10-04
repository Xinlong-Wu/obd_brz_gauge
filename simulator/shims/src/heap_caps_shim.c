/* Simulator shim: heap_caps_* → libc malloc family (capabilities ignored). */
#include "esp_heap_caps.h"
#include <stdlib.h>

void *heap_caps_malloc(size_t size, uint32_t caps)
{
    (void)caps;
    return malloc(size);
}

void *heap_caps_calloc(size_t n, size_t size, uint32_t caps)
{
    (void)caps;
    return calloc(n, size);
}

void *heap_caps_realloc(void *ptr, size_t size, uint32_t caps)
{
    (void)caps;
    return realloc(ptr, size);
}

void heap_caps_free(void *ptr)
{
    free(ptr);
}
