#pragma once
/* Simulator shim: semphr.h — the BLE-scan page guards UI updates with
 * lvgl_mux (normally defined in app_main.c, which is not compiled here;
 * freertos_stubs.c provides it). Pass-through take/give. */
#include "FreeRTOS.h"

typedef void *SemaphoreHandle_t;

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout);
void xSemaphoreGive(SemaphoreHandle_t semaphore);
