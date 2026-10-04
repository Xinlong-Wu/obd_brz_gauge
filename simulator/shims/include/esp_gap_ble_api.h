#pragma once
/* Simulator shim: the BLE GAP types that racechrono_ble_diy.h references in
 * a function declaration. No BLE exists in the simulator; the declaration
 * just needs to compile (nothing calls it there). */

typedef int esp_gap_ble_cb_event_t;
typedef struct esp_ble_gap_cb_param_s esp_ble_gap_cb_param_t;
