/* Simulator shim: ESP-IDF system calls. */
#include "esp_system.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "sim_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- esp_system ---- */
void esp_restart(void)
{
    fprintf(stderr, "[sim] esp_restart() requested — ignored. Quit and rerun to 'reboot'.\n");
}

/* ---- esp_random: xorshift64, seeded from the wall clock ---- */
static uint64_t s_prng_state = 0;

static void prng_seed_once(void)
{
    if (s_prng_state == 0) {
        s_prng_state = (uint64_t)time(NULL) * 6364136223846793005ULL + 1442695040888963407ULL;
        if (s_prng_state == 0) s_prng_state = 0x9E3779B97F4A7C15ULL;
    }
}

uint32_t esp_random(void)
{
    prng_seed_once();
    s_prng_state ^= s_prng_state << 13;
    s_prng_state ^= s_prng_state >> 7;
    s_prng_state ^= s_prng_state << 17;
    return (uint32_t)(s_prng_state >> 16);
}

/* ---- monotonic clocks (POSIX; no SDL dependency here) ---- */
static int64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

int64_t esp_timer_get_time(void)
{
    return now_us();
}

/* ---- cooperative periodic timers (mimics esp_timer for the mileage task) ---- */
struct esp_timer_s {
    const char *name;
    void (*callback)(void *arg);
    void *arg;
    uint64_t period_us;
    int64_t next_fire_us;
    bool running;
};

#define SIM_MAX_TIMERS 8
static struct esp_timer_s s_timers[SIM_MAX_TIMERS];

esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out)
{
    if (!args || !out) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < SIM_MAX_TIMERS; i++) {
        if (s_timers[i].callback == NULL) {
            memset(&s_timers[i], 0, sizeof(s_timers[i]));
            s_timers[i].name = args->name;
            s_timers[i].callback = args->callback;
            s_timers[i].arg = args->arg;
            *out = &s_timers[i];
            return ESP_OK;
        }
    }
    fprintf(stderr, "[sim] esp_timer_create: out of slots\n");
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us)
{
    if (!timer) return ESP_ERR_INVALID_ARG;
    timer->period_us = period_us;
    timer->next_fire_us = now_us() + (int64_t)period_us;
    timer->running = true;
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t timer)
{
    if (!timer) return ESP_ERR_INVALID_ARG;
    timer->running = false;
    return ESP_OK;
}

esp_err_t esp_timer_delete(esp_timer_handle_t timer)
{
    if (!timer) return ESP_ERR_INVALID_ARG;
    memset(timer, 0, sizeof(*timer));
    return ESP_OK;
}

void sim_esp_timer_poll(void)
{
    int64_t now = now_us();
    for (int i = 0; i < SIM_MAX_TIMERS; i++) {
        struct esp_timer_s *t = &s_timers[i];
        if (t->callback && t->running && now >= t->next_fire_us) {
            t->next_fire_us += t->period_us;
            if (t->next_fire_us < now) t->next_fire_us = now + t->period_us;
            t->callback(t->arg);
        }
    }
}

/* ---- esp_err ---- */
const char *esp_err_to_name(esp_err_t code)
{
    switch (code) {
    case ESP_OK:                return "ESP_OK";
    case ESP_FAIL:              return "ESP_FAIL";
    case ESP_ERR_NO_MEM:        return "ESP_ERR_NO_MEM";
    case ESP_ERR_INVALID_ARG:   return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
    case ESP_ERR_INVALID_SIZE:  return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_NOT_FOUND:     return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_NOT_SUPPORTED: return "ESP_ERR_NOT_SUPPORTED";
    case ESP_ERR_TIMEOUT:       return "ESP_ERR_TIMEOUT";
    default:                    return "ESP_ERR_SIM_UNKNOWN";
    }
}
