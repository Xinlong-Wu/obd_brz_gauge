#pragma once
/* Fake vehicle-data scenario engine.
 *
 * Refactored into a pure calculator: fake_data_compute() advances the
 * scenario and returns the values it wants in the data cache. Consumers
 * (main.c --no-panel fallback, control_panel.c) decide how to apply them,
 * so the panel can gate channels individually while the user drags. */
#include "cli.h"

#include <stdbool.h>

/* All values in obd_data_cache units. */
typedef struct {
    int  rpm;        /* 0..8000 */
    int  speed;      /* 0..240 km/h */
    int  gear;       /* -1=R 0=N 1..8 */
    int  coolant;    /* deg C; -40 = invalid sentinel */
    int  oil;        /* deg C; valid only when oil_valid */
    bool oil_valid;
    int  intake;     /* deg C */
    int  bat_mv;     /* mV */
    int  oilp_x10;   /* 0.1 bar */
    int  boost_x10;  /* 0.1 bar */
    int  brake_x10;  /* 0.1 deg C */
    int  tps;        /* % */
    int  load;       /* % */
    int  afr_x100;   /* x100 (1470 = 14.7:1) */
    bool brake_ok;   /* RS485 link status OK */
} fake_values_t;

/* Seed scenario state from CLI options. */
void fake_data_init(const sim_opts_t *opts);

/* Advance the scenario by dt_ms and produce the target values. */
void fake_data_compute(int dt_ms, fake_values_t *out);
