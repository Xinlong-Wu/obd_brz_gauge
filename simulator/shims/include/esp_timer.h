#pragma once
/* Simulator shim: esp_timer. Real periodic timers are emulated cooperatively —
 * sim_esp_timer_poll() is called once per simulator frame (see esp_stubs.c).
 * esp_timer_get_time() is a real monotonic microsecond clock. */
#include <stdint.h>
#include "esp_err.h"
/* IDF's esp_timer.h transitively exposes the task types (ui.c relies on
 * TaskHandle_t after including esp_timer.h). */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct esp_timer_s *esp_timer_handle_t;

typedef struct {
    void (*callback)(void *arg);
    void *arg;
    const char *name;
} esp_timer_create_args_t;

int64_t esp_timer_get_time(void);
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);

/* Simulator-only: fire any due periodic callbacks (called from the main loop). */
void sim_esp_timer_poll(void);

/* Simulator-only: redirect the esp_timer clock (screenshot determinism).
 * NULL restores the default CLOCK_MONOTONIC source. The simulator's
 * --clock virtual mode registers its frame-locked clock here so mileage
 * statistics render identically run to run; unit tests never call this. */
void sim_esp_timer_use_clock(int64_t (*us)(void));
