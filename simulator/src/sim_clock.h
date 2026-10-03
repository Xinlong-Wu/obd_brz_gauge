#pragma once
/* Simulator clock: real (SDL_GetTicks) or virtual (frame-locked).
 *
 * --clock virtual drives EVERYTHING off the frame counter — the LVGL tick
 * (via lv_conf.h's LV_TICK_CUSTOM), tour phase timing, and esp_timer (via
 * sim_esp_timer_use_clock) — so a fixed frame budget + --seed produces
 * bit-identical screenshots run to run. That is what the screenshot
 * regression (tools/sim_regress.py) relies on.
 * One virtual frame == SIM_CLOCK_FRAME_MS regardless of wall-clock cost,
 * which also lets headless runs finish faster than real time. */
#include <stdint.h>
#include <stdbool.h>

#define SIM_CLOCK_FRAME_MS 5

/* Milliseconds for LVGL's tick and general pacing. */
uint32_t sim_clock_ms(void);

/* Microseconds for esp_timer (periodic mileage task). */
int64_t sim_clock_us(void);

/* virtual=true: frame-locked (deterministic headless); false: wall clock. */
void sim_clock_set_virtual(bool virtual);
bool sim_clock_is_virtual(void);

/* Advance the virtual clock by one frame period (main loop calls this
 * once per frame AFTER lv_timer_handler(), so the frame's tick is stable
 * throughout rendering). No-op in real mode. */
void sim_clock_tick_frame(void);
