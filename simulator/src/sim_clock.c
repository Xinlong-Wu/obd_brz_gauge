/* sim_clock.c — see sim_clock.h. SDL is the real-mode source; the header
 * stays SDL-free so LVGL sources can include it without SDL paths. */
#include "sim_clock.h"

#include <SDL.h>

static bool     s_virtual;
static uint32_t s_frames;

uint32_t sim_clock_ms(void)
{
    if (s_virtual) return s_frames * SIM_CLOCK_FRAME_MS;
    return SDL_GetTicks();
}

int64_t sim_clock_us(void)
{
    return (int64_t)sim_clock_ms() * 1000;
}

void sim_clock_set_virtual(bool virtual)
{
    s_virtual = virtual;
    s_frames = 0;
}

bool sim_clock_is_virtual(void)
{
    return s_virtual;
}

void sim_clock_tick_frame(void)
{
    if (s_virtual) s_frames++;
}
