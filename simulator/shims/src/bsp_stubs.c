/* Simulator shim: BSP stubs — screen backlight + the BLE/ESP-NOW/pairing/
 * RS485 link APIs that the UI calls but a PC cannot provide. All no-ops or
 * benign canned answers, so every page renders with plausible state. */
#include "bsp_obd_dsp/lcd_driver/ST77916.h"
#include "bsp_obd_dsp/elm327_ble_client.h"
#include "bsp_obd_dsp/espnow_link.h"
#include "bsp_obd_dsp/gauge_pair_ble_client.h"
#include "bsp_obd_dsp/rs485_brake_temp.h"
#include "cli.h"

#include <stdio.h>
#include <string.h>

/* ---- backlight ---- */
void Set_Backlight(uint8_t Light)
{
    (void)Light; /* the OS owns brightness; nothing to do */
}

/* ---- ELM327 BLE client ----
 * Pretend an adapter named SIM-ELM327 is connected unless --disconnected. */
static bool s_elm_connected = true;

bool elm327_ble_is_connected(void)
{
    return s_elm_connected;
}

const char *elm327_ble_get_connected_name(void)
{
    return s_elm_connected ? "SIM-ELM327" : "";
}

void elm327_ble_scan_only_start(int duration_s, ble_scan_found_cb_t cb)
{
    (void)duration_s;
    (void)cb; /* no fake scan results: the list stays empty */
    fprintf(stderr, "[sim] elm327_ble_scan_only_start (no results in simulator)\n");
}

void elm327_ble_scan_only_stop(void) {}

void elm327_ble_connect_by_addr(const uint8_t mac[6], const char *name)
{
    (void)mac;
    fprintf(stderr, "[sim] elm327_ble_connect_by_addr(%s) -> connected\n", name ? name : "?");
    s_elm_connected = true;
}

void elm327_ble_disconnect(void)
{
    s_elm_connected = false;
}

void elm327_ble_pause_for_ota(void) {}

/* ---- ESP-NOW link (standalone simulator: nothing online) ---- */
bool espnow_link_slave_has_data(void) { return false; }

const char *espnow_link_get_master_name(void) { return ""; }

const uint8_t *espnow_link_get_bound_master_mac(void)
{
    static const uint8_t zero_mac[6] = {0};
    return zero_mac;
}

void espnow_link_bind_master(const uint8_t mac[6])   { (void)mac; }
void espnow_link_unbind_master(void)                 {}
void espnow_link_pause_for_ota(void)                 {}
void espnow_link_broadcast_threshold(uint16_t thresh){ (void)thresh; }
void espnow_link_apply_synced_threshold(uint16_t thresh) { (void)thresh; }
void espnow_link_trigger_linked_test(void)
{
    fprintf(stderr, "[sim] linked test requested (no-op in simulator)\n");
}

uint8_t espnow_master_online_slaves(void) { return 0; }

bool espnow_link_linked_en(void) { return false; }
bool espnow_link_linktest_active(void) { return false; }

/* ---- gauge pairing (slave side) ---- */
void gauge_pair_ble_scan_start(int duration_s, gauge_pair_scan_cb_t cb)
{
    (void)duration_s;
    (void)cb; /* no SkyGauge masters around */
    fprintf(stderr, "[sim] gauge_pair scan started (no results in simulator)\n");
}

void gauge_pair_ble_scan_stop(void) {}

void gauge_pair_ble_connect(const uint8_t addr[6], const char *name, gauge_pair_result_cb_t cb)
{
    (void)addr;
    fprintf(stderr, "[sim] gauge_pair connect(%s) -> fail (simulator)\n", name ? name : "?");
    if (cb) cb(false, "", (const uint8_t *)"\0\0\0\0\0\0");
}

/* ---- RS485 brake temperature module ---- */
void rs485_brake_temp_pause(void) {}
void rs485_brake_temp_resume(void) {}

/* Keep the CLI flag wired in (called from main). */
void sim_bsp_apply_opts(const sim_opts_t *opts)
{
    if (opts && opts->disconnected) s_elm_connected = false;
}
