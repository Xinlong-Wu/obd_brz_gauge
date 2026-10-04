/* Simulator shim: FreeRTOS pieces used by the compiled UI subset.
 * Single-threaded simulator → critical sections are compile-time no-ops,
 * semaphores always succeed. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* The BLE-scan page declares this extern (normally defined in app_main.c). */
SemaphoreHandle_t lvgl_mux = (SemaphoreHandle_t)1;

TickType_t xTaskGetTickCount(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (TickType_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void vTaskDelay(uint32_t ticks)
{
    usleep(ticks * portTICK_PERIOD_MS * 1000);
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout)
{
    (void)semaphore; (void)timeout;
    return pdTRUE;
}

void xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    (void)semaphore;
}
