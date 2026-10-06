/* Simulator shim: BSP stubs — screen backlight + the BLE/ESP-NOW/pairing/
 * RS485 link APIs that the UI calls but a PC cannot provide.
 *
 * The BLE layer is *simulated*, not just stubbed: scanning discovers a few
 * fake devices over time, tapping one connects after a short delay, and the
 * gauge-pairing flow completes — so the whole "scan → pick → connect" story
 * is walkable on the PC. */
#include "bsp_obd_dsp/lcd_driver/ST77916.h"
#include "bsp_obd_dsp/elm327_ble_client.h"
#include "bsp_obd_dsp/espnow_link.h"
#include "bsp_obd_dsp/gauge_pair_ble_client.h"
#include "bsp_obd_dsp/rs485_brake_temp.h"
#include "cli.h"

#include <stdio.h>
#include <string.h>
#include "lvgl.h"

/* ---- backlight ---- */
void Set_Backlight(uint8_t Light)
{
    (void)Light; /* the OS owns brightness; nothing to do */
}

/* ================= ELM327 BLE client (simulated) ================= */

#define ELM_CONNECT_DELAY_MS 1200

static bool       s_elm_connected;
static char       s_elm_name[32] = "SIM-ELM327"; /* default canned answer */
static lv_timer_t *s_scan_timer;      /* delivers one fake device per tick */
static lv_timer_t *s_connect_timer;   /* flips connected after a delay */
static ble_scan_found_cb_t s_scan_cb; /* already a function-pointer typedef */

static const struct {
    const char *name;
    uint8_t     mac[6];
    int         rssi;
} s_fake_adapters[] = {
    { "OBDII",       { 0x2C, 0xAB, 0x33, 0x11, 0x22, 0x01 }, -52 },
    { "V-LINK",      { 0x2C, 0xAB, 0x33, 0x11, 0x22, 0x02 }, -64 },
    { "ELM327 v2.3", { 0x2C, 0xAB, 0x33, 0x11, 0x22, 0x03 }, -71 },
};

bool elm327_ble_is_connected(void)
{
    return s_elm_connected;
}

const char *elm327_ble_get_connected_name(void)
{
    return s_elm_connected ? s_elm_name : "";
}

static void fake_scan_tick(lv_timer_t *t)
{
    if (!s_scan_cb) return;
    int idx = (int)(uintptr_t)lv_timer_get_user_data(t);
    if (idx >= (int)(sizeof(s_fake_adapters) / sizeof(s_fake_adapters[0]))) {
        lv_timer_del(t);
        s_scan_timer = NULL;
        return;
    }
    ble_scan_result_t dev;
    memset(&dev, 0, sizeof(dev));
    snprintf(dev.name, sizeof(dev.name), "%s", s_fake_adapters[idx].name);
    memcpy(dev.addr, s_fake_adapters[idx].mac, 6);
    dev.rssi = s_fake_adapters[idx].rssi;
    fprintf(stderr, "[sim] scan found: %s (%d dBm)\n", dev.name, dev.rssi);
    s_scan_cb(&dev, idx + 1);
    lv_timer_set_user_data(t, (void *)(uintptr_t)(idx + 1));
}

void elm327_ble_scan_only_start(int duration_s, ble_scan_found_cb_t cb)
{
    (void)duration_s;
    elm327_ble_scan_only_stop();
    s_scan_cb = cb;
    s_scan_timer = lv_timer_create(fake_scan_tick, 700, NULL); /* one device / 700ms */
    fprintf(stderr, "[sim] elm327 scan started (%u fake adapters)\n",
            (unsigned)(sizeof(s_fake_adapters) / sizeof(s_fake_adapters[0])));
}

void elm327_ble_scan_only_stop(void)
{
    if (s_scan_timer) {
        lv_timer_del(s_scan_timer);
        s_scan_timer = NULL;
    }
    s_scan_cb = NULL;
}

static void fake_connect_done(lv_timer_t *t)
{
    lv_timer_del(t);
    s_connect_timer = NULL;
    s_elm_connected = true;
    fprintf(stderr, "[sim] ELM327 connected: %s\n", s_elm_name);
}

void elm327_ble_connect_by_addr(const uint8_t mac[6], const char *name)
{
    elm327_ble_scan_only_stop();
    if (s_connect_timer) lv_timer_del(s_connect_timer);
    if (name && *name) snprintf(s_elm_name, sizeof(s_elm_name), "%s", name);
    fprintf(stderr, "[sim] elm327 connecting to %s (%02x:%02x:%02x:%02x:%02x:%02x), "
            "ready in %dms\n", s_elm_name,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ELM_CONNECT_DELAY_MS);
    s_connect_timer = lv_timer_create(fake_connect_done, ELM_CONNECT_DELAY_MS, NULL);
}

void elm327_ble_disconnect(void)
{
    if (s_connect_timer) {
        lv_timer_del(s_connect_timer);
        s_connect_timer = NULL;
    }
    s_elm_connected = false;
}

void elm327_ble_pause_for_ota(void) {}

/* ================= ESP-NOW link (simulated) ================= */

static uint8_t s_bound_master[6]; /* all-zero = unbound */

bool espnow_link_slave_has_data(void)
{
    /* "Master is broadcasting" once a gauge has been paired this session. */
    return s_bound_master[0] != 0;
}

const char *espnow_link_get_master_name(void)
{
    return s_bound_master[0] ? "SkyGauge-SIM" : "";
}

const uint8_t *espnow_link_get_bound_master_mac(void)
{
    return s_bound_master;
}

void espnow_link_bind_master(const uint8_t mac[6])
{
    memcpy(s_bound_master, mac, 6);
    fprintf(stderr, "[sim] master bound (%02x:%02x:%02x:%02x:%02x:%02x), "
            "fake ESP-NOW data flowing\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void espnow_link_unbind_master(void)
{
    memset(s_bound_master, 0, sizeof(s_bound_master));
}

void espnow_link_pause_for_ota(void) {}
void espnow_link_broadcast_threshold(uint16_t thresh) { (void)thresh; }
void espnow_link_apply_synced_threshold(uint16_t thresh) { (void)thresh; }

void espnow_link_trigger_linked_test(void)
{
    fprintf(stderr, "[sim] linked test requested (no-op in simulator)\n");
}

uint8_t espnow_master_online_slaves(void) { return s_bound_master[0] ? 1 : 0; }

bool espnow_link_linked_en(void) { return false; }
bool espnow_link_linktest_active(void) { return false; }

/* ================= Gauge pairing (slave side, simulated) ================= */

#define GAUGE_PAIR_DELAY_MS 1000

static lv_timer_t *s_pair_timer;
static gauge_pair_result_cb_t s_pair_cb;
static char s_pair_name[32];
static uint8_t s_pair_mac[6];

static const struct {
    const char *name;
    uint8_t     mac[6];
    int         rssi;
} s_fake_masters[] = {
    { "SkyGauge-4F2A", { 0x02, 0x42, 0x53, 0x4B, 0x59, 0x2A }, -58 },
    { "SkyGauge-91C3", { 0x02, 0x42, 0x53, 0x4B, 0x59, 0xC3 }, -67 },
};

void gauge_pair_ble_scan_start(int duration_s, gauge_pair_scan_cb_t cb)
{
    (void)duration_s;
    /* Deliver both fake masters immediately; the page dedups by name. */
    for (unsigned i = 0; i < sizeof(s_fake_masters) / sizeof(s_fake_masters[0]); i++) {
        gauge_pair_scan_result_t dev;
        memset(&dev, 0, sizeof(dev));
        snprintf(dev.name, sizeof(dev.name), "%s", s_fake_masters[i].name);
        memcpy(dev.addr, s_fake_masters[i].mac, 6);
        dev.rssi = s_fake_masters[i].rssi;
        fprintf(stderr, "[sim] gauge scan found: %s (%d dBm)\n", dev.name, dev.rssi);
        cb(&dev, (int)(i + 1));
    }
}

void gauge_pair_ble_scan_stop(void) {}

static void fake_pair_done(lv_timer_t *t)
{
    lv_timer_del(t);
    s_pair_timer = NULL;
    fprintf(stderr, "[sim] gauge pairing succeeded: %s\n", s_pair_name);
    if (s_pair_cb) {
        s_pair_cb(true, s_pair_name, s_pair_mac);
        s_pair_cb = NULL;
    }
}

void gauge_pair_ble_connect(const uint8_t addr[6], const char *name, gauge_pair_result_cb_t cb)
{
    if (s_pair_timer) lv_timer_del(s_pair_timer);
    s_pair_cb = cb;
    snprintf(s_pair_name, sizeof(s_pair_name), "%s", name ? name : "SkyGauge");
    memcpy(s_pair_mac, addr, 6);
    fprintf(stderr, "[sim] gauge pairing with %s, done in %dms\n",
            s_pair_name, GAUGE_PAIR_DELAY_MS);
    s_pair_timer = lv_timer_create(fake_pair_done, GAUGE_PAIR_DELAY_MS, NULL);
}

/* ---- RS485 brake temperature module ---- */
void rs485_brake_temp_pause(void) {}
void rs485_brake_temp_resume(void) {}

/* Keep the CLI flags wired in (called from main): --disconnected starts with
 * the adapter offline; otherwise it only comes online via the simulated
 * connect flow (or immediately when --bound skips the scan page). */
void sim_bsp_apply_opts(const sim_opts_t *opts)
{
    s_elm_connected = opts ? !opts->disconnected : true;
}
