/* Fake vehicle-data scenarios — pure calculator (no timers, no cache writes;
 * callers apply values, see fake_data.h).
 *
 * "drive": ignition → warm-up idle → repeated accelerate/cruise/WOT/brake
 *          cycles. Gear is SET explicitly (as a CAN gear-read would be), so
 *          the display does not depend on per-vehicle ratio tables.
 * "idle":  indefinite warm-up idle.
 */
#include "fake_data.h"

#include "esp_random.h"

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

void fake_data_init(const sim_opts_t *opts)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.opts = *opts;
    s_st.phase = PH_IGNITION;
    s_st.coolant = -40;  /* invalid sentinel, same as the cache default */
    s_st.oil = -100;     /* invalid sentinel */
    s_st.brake_x10 = 2500;
}

/* Idle segment shared by warm-up and the "idle" scenario. */
static void compute_idle(fake_values_t *v, int32_t ramp_ms)
{
    v->rpm = 850 + jitter(40);
    v->speed = 0;
    v->gear = 0; /* N */

    s_st.coolant = lerp(-40, 88, s_st.phase_ms, ramp_ms);
    v->coolant = s_st.coolant;
    v->intake = lerp(25, 40, s_st.phase_ms, ramp_ms);

    /* Oil stays invalid until the coolant has warmed, then climbs from 25 C. */
    if (s_st.coolant > 20) {
        s_st.oil = lerp(25, 88, s_st.phase_ms - 6000, ramp_ms);
        v->oil = s_st.oil;
        v->oil_valid = true;
    }

    v->bat_mv = 14000 + jitter(150);
    v->tps = 1 + jitter(1);
    v->load = 10 + jitter(3);
    v->oilp_x10 = 17 + jitter(2);   /* 1.7 bar */
    v->boost_x10 = 0;
    v->afr_x100 = 1470 + jitter(15);
    v->brake_x10 = s_st.brake_x10;
}

static void compute_drive_segment(fake_values_t *v)
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

    v->speed = speed;
    v->gear = gear;
    v->rpm = rpm;
    v->tps = tps;
    v->load = tps + jitter(4);
    v->boost_x10 = boost;
    v->afr_x100 = afr;
    v->oilp_x10 = 10 + rpm / 125 + jitter(3); /* 0.1 bar: ~1.7 idle, ~5.0 at 5k rpm */
    v->bat_mv = 13900 + jitter(200);

    /* Temps keep creeping while driving. */
    if (s_st.coolant < 92) s_st.coolant += 1;
    if (s_st.oil < 105) s_st.oil += 1;
    v->coolant = s_st.coolant;
    v->oil = s_st.oil;
    v->oil_valid = true;
    v->intake = 38 + jitter(6);
    v->brake_x10 = s_st.brake_x10;
}

void fake_data_compute(int dt_ms, fake_values_t *out)
{
    memset(out, 0, sizeof(*out));
    out->brake_ok = true;

    s_st.phase_ms += dt_ms;

    if (s_st.opts.scenario && strcmp(s_st.opts.scenario, "idle") == 0) {
        compute_idle(out, 8000);
        return;
    }

    if (s_st.phase_ms >= PHASE_MS[s_st.phase]) next_phase();

    if (s_st.phase == PH_IGNITION) {
        out->rpm = 0;
        out->speed = 0;
        out->gear = 0;
        out->bat_mv = 12200 + jitter(80);
        out->coolant = s_st.coolant; /* stays -40 = invalid */
    } else if (s_st.phase == PH_WARMUP) {
        compute_idle(out, PHASE_MS[PH_WARMUP]);
    } else {
        compute_drive_segment(out);
    }
}
