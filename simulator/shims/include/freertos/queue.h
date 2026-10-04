#pragma once
/* Simulator shim: queue.h — app_event.h only needs the handle type for a
 * declaration; the real queue is not used (app_event_recv is stubbed). */
#include "FreeRTOS.h"

typedef void *QueueHandle_t;
