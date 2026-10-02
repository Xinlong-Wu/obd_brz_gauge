#pragma once
/* Fake vehicle-data scenario engine: drives obd_data_cache setters from an
 * lv_timer so the UI renders as if a car were driving. */
#include "cli.h"

void fake_data_start(const sim_opts_t *opts);
