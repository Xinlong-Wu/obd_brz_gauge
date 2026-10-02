/* Fake vehicle-data scenarios, feeding the REAL obd_data_cache setters from
 * an lv_timer (same thread as the UI — no locking concerns on the simulator).
 *
 * "drive": ignition → warm-up idle → repeated accelerate/cruise/WOT/brake
 *          cycles. Gear is SET explicitly (as a CAN gear-read would be), so
 *          the display does not depend on per-vehicle ratio tables.
 * "idle":  indefinite warm-up idle.
 */
#include "fake_data.h"

#include "app_obd_dsp/obd_data_cache.h"

#include "esp_random.h"
#include "lvgl.h"

#include <string.h>

typedef enum {
    PH_IGNITION,   /* battery only, engine off */
    PH_WARMUP,     /* idle, temps climbing */
    PH_ACCEL,      /* 0 -> 90 km/h */
    PH_CRUISE,     /* steady 90 */
    PH_WOT,        /* wide-open-throttle burst */
    PH_BRAKE,      /* 90 -> 35, brake temp spike */
    PH_COOL,       /* low speed, temps drift back down */
} phase_t;

typedef struct {
    phase_t phase;
    int32_t phase_ms;      /* ms elapsed inside the current phase */
    int32_t coolant;       /* deg C; starts at the -40 invalid sentinel */
    int32_t oil;           /* deg C; starts at the -100 invalid sentinel */
    int32_t brake_x10;     /* 0.1 deg C */
    sim_opts_t opts;
} fake_state_t;

static fake_state_t s_st;

static const int32_t PHASE_MS[] = {
    [PH_IGNITION] = 2000,
    [PH_WARMUP]   = 12000,
    [PH_ACCEL]    = 13000,
    [PH_CRUISE]   = 9000,
    [PH_WOT]      = 5000,
    [PH_BRAKE]    = 6000,
    [PH_COOL]     = 5000,
};

static int32_t jitter(int32_t amp)
{
    return (int32_t)(esp_random() % (uint32_t)(2 * amp + 1)) - amp;
}

static int32_t lerp(int32_t a, int32_t b, int32_t num, int32_t den)
{
    if (den <= 0) den = 1;
    if (num < 0) num = 0;
    if (num > den) num = den;
    return a + (b - a) * num / den;
}

static void next_phase(void)
{
    if (s_st.phase == PH_IGNITION) {
        s_st.phase = PH_WARMUP;
    } else if (s_st.phase == PH_WARMUP) {
        s_st.phase = PH_ACCEL;
    } else if (s_st.phase == PH_COOL) {
        s_st.phase = PH_ACCEL;   /* loop the driving cycle */
    } else {
        s_st.phase++;
    }
    s_st.phase_ms = 0;
}

/* Idle segment shared by warm-up and the "idle" scenario. */
static void run_idle(int32_t ramp_ms)
{
    obd_data_set_rpm((uint16_t)(850 + jitter(40)));
    obd_data_set_speed(0);
    obd_data_set_gear(0); /* N */

    s_st.coolant = lerp(-40, 88, s_st.phase_ms, ramp_ms);
    obd_data_set_coolant_temp((int16_t)s_st.coolant);
    obd_data_set_intake_temp((int16_t)lerp(25, 40, s_st.phase_ms, ramp_ms));

    /* Oil stays invalid until the coolant has warmed, then climbs from 25 C. */
    if (s_st.coolant > 20) {
        s_st.oil = lerp(25, 88, s_st.phase_ms - 6000, ramp_ms);
        obd_data_set_oil_temp((int16_t)s_st.oil);
    }

    obd_data_set_bat_mv(14000 + jitter(150));
    obd_data_set_tps((int16_t)(1 + jitter(1)));
    obd_data_set_load_pct((int16_t)(10 + jitter(3)));
    obd_data_set_oil_pressure_x10((int16_t)(17 + jitter(2)));   /* 1.7 bar */
    obd_data_set_boost_x10(0);
    obd_data_set_afr_x100(1470 + jitter(15));
    obd_data_set_brake_temp_x10((int16_t)s_st.brake_x10);
}

static void run_drive_segment(void)
{
    int32_t p = s_st.phase_ms;
    int32_t d = PHASE_MS[s_st.phase];
    int32_t speed = 0;
    int32_t gear = 0;
    int32_t tps = 0;
    int32_t boost = 0;
    int32_t afr = 1470;
    int32_t rpm = 900;

    switch (s_st.phase) {
    case PH_ACCEL:
        speed = lerp(0, 90, p, d);
        gear = speed < 20 ? 1 : speed < 45 ? 2 : speed < 65 ? 3 : speed < 80 ? 4 : 5;
        rpm = lerp(1100, 5600, p, d) + jitter(60);
        tps = lerp(12, 85, p, d);
        boost = lerp(2, 12, p, d);
        afr = lerp(1460, 1270, p, d);
        break;
    case PH_CRUISE:
        speed = 90 + jitter(1);
        gear = 5;
        rpm = 2800 + jitter(60);
        tps = 18;
        boost = 3;
        afr = 1470 + jitter(10);
        break;
    case PH_WOT:
        speed = 90;
        gear = 4;
        rpm = 4600 + jitter(100);
        tps = 100;
        boost = 13;
        afr = 1270 + jitter(15);
        break;
    case PH_BRAKE:
        speed = lerp(90, 35, p, d);
        gear = speed < 50 ? 2 : 3;
        rpm = 950 + speed * 22 + jitter(60);
        tps = 0;
        boost = 0;
        afr = 1490;
        s_st.brake_x10 = lerp(2500, 3300, p, d);   /* brake disc heats up */
        break;
    case PH_COOL:
        speed = 35;
        gear = 2;
        rpm = 1300 + jitter(50);
        tps = 8;
        boost = 1;
        afr = 1470;
        s_st.brake_x10 = lerp(3300, 2600, p, d);   /* discs cool back down */
        break;
    default:
        break;
    }

    obd_data_set_speed((uint8_t)speed);
    obd_data_set_gear((int8_t)gear);
    obd_data_set_rpm((uint16_t)rpm);
    obd_data_set_tps((int16_t)tps);
    obd_data_set_load_pct((int16_t)(tps + jitter(4)));
    obd_data_set_boost_x10((int16_t)boost);
    obd_data_set_afr_x100((int16_t)afr);
    obd_data_set_oil_pressure_x10((int16_t)(100 + (rpm * 8) / 1000 + jitter(3))); /* 0.1 bar */
    obd_data_set_bat_mv(13900 + jitter(200));

    /* Temps keep creeping while driving. */
    if (s_st.coolant < 92) s_st.coolant += 1;
    if (s_st.oil < 105) s_st.oil += 1;
    obd_data_set_coolant_temp((int16_t)s_st.coolant);
    obd_data_set_oil_temp((int16_t)s_st.oil);
    obd_data_set_intake_temp((int16_t)(38 + jitter(6)));
    obd_data_set_brake_temp_x10((int16_t)s_st.brake_x10);
}

static void fake_tick(lv_timer_t *t)
{
    (void)t;
    s_st.phase_ms += 100;

    obd_data_set_brake_rs485_status(BRAKE_RS485_OK);

    if (s_st.opts.scenario && strcmp(s_st.opts.scenario, "idle") == 0) {
        run_idle(8000);
        return;
    }

    if (s_st.phase_ms >= PHASE_MS[s_st.phase]) next_phase();

    if (s_st.phase == PH_IGNITION) {
        obd_data_set_rpm(0);
        obd_data_set_speed(0);
        obd_data_set_gear(0);
        obd_data_set_bat_mv(12200 + jitter(80));
        obd_data_set_coolant_temp((int16_t)s_st.coolant); /* stays -40 = invalid */
    } else if (s_st.phase == PH_WARMUP) {
        run_idle(PHASE_MS[PH_WARMUP]);
    } else {
        run_drive_segment();
    }
}

void fake_data_start(const sim_opts_t *opts)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.opts = *opts;
    s_st.phase = PH_IGNITION;
    s_st.coolant = -40;  /* invalid sentinel, same as the cache default */
    s_st.oil = -100;     /* invalid sentinel */
    s_st.brake_x10 = 2500;

    lv_timer_create(fake_tick, 100, NULL);
}
