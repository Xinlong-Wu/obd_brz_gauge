#pragma once
/* Simulator shim: FreeRTOS.h — tick type + pd* constants. The simulator is
 * single-threaded, so critical sections and semaphores are pass-through. */
#include <stdint.h>
#include "portmacro.h"

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;

#define pdTRUE   1
#define pdFALSE  0
#define pdPASS   pdTRUE
#define pdFAIL   0

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

TickType_t xTaskGetTickCount(void);
