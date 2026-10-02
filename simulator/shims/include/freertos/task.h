#pragma once
/* Simulator shim: task.h — vTaskDelay blocks the loop briefly (only used by
 * the OTA-mode page, 150ms, harmless on PC). TaskHandle_t exists because
 * ui.c defines g_lvgl_task_handle (priority-boost hook that is never driven
 * in the simulator). */

typedef void *TaskHandle_t;

void vTaskDelay(uint32_t ticks);
