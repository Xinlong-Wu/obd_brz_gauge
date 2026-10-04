/* Simulator shim: in-memory NVS (nvs_storage.h API).
 *
 * The real nvs_storage.c depends on nvs_flash/FreeRTOS/espnow_link/ui.h and
 * would drag in half the BSP; here we reimplement only the accessors the
 * compiled UI subset calls, with the same defaults and sanity clamps as the
 * firmware (see nvs_storage.c load_blob validation). Nothing persists —
 * settings reset on relaunch, which is fine for a preview tool. */
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/espnow_link.h"   /* ESPNOW_ROLE_* */
#include "bsp_obd_dsp/nvs_error_log_logic.h"
#include "bsp_obd_dsp/ui_dashboard_logic.h"
#include "sim_platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static nvs_user_cfg_t s_cfg;
static nvs_stat_t     s_stat;
static bool           s_cfg_valid;
static bool           s_stat_valid;
static int16_t        s_chart_alarm[12];
static bool           s_chart_alarm_valid;
static uint8_t        s_intro_enable = 2;   /* firmware default: VIDEO */
static uint8_t        s_device_position = 1;
static uint8_t        s_boot_mode = 0;
static uint64_t       s_run_ms_total;

static void ensure_defaults(void)
{
    if (s_cfg_valid) return;
    s_cfg_valid = true;

    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.protocol = 0;                          /* auto */
    s_cfg.theme_cfg.theme = 0;                   /* registry slot 0 = default */
    s_cfg.theme_cfg.user_theme_domiant_color = 0xFFFFFF;
    s_cfg.theme_cfg.user_theme_secondary_color = 0x333333;
    s_cfg.default_page = 0;                      /* Temp */
    s_cfg.brightness_day = 100;
    s_cfg.vehicle_profile_idx = 0;
    s_cfg.brake_temp_warn_c = 600;
    s_cfg.oil_pressure_warn_x10 = 80;
    s_cfg.temp_display_map[0] = 0;
    s_cfg.temp_display_map[1] = 1;
    s_cfg.temp_display_map[2] = 2;
    s_cfg.info_display_map[0] = 0;
    s_cfg.info_display_map[1] = 2;
    s_cfg.info_display_map[2] = 3;
    s_cfg.info_display_map[3] = 4;
    s_cfg.info_display_map[4] = 1;
    s_cfg.needle_source_idx = 0;                 /* CLT */
    s_cfg.device_role = ESPNOW_ROLE_STANDALONE;
    s_cfg.chart_source_idx = 8;                  /* OILP */
    s_cfg.rpm_warn_threshold = 6000;
    s_cfg.rpm_warn_anim_en = 0;
    s_cfg.rpm_warn_linked_en = 0;
    s_cfg.rc_enabled = 0;
    /* Unbound by default: the plain `obd_gauge_sim` run boots to the BLE scan
     * page, where the fake adapters can be picked and "connected" manually.
     * --bound pre-fills a saved adapter so boot lands straight on the gauges. */

    memset(&s_stat, 0, sizeof(s_stat));

    /* M4:与固件 nvs_storage_init 同款的自愈 + 老配置迁移(mock 不落盘) */
    (void)ui_dashboard_logic_sanitize(&s_cfg.dashboard);
    (void)ui_dashboard_logic_migrate_from_maps(&s_cfg.dashboard,
                                               s_cfg.temp_display_map,
                                               s_cfg.info_display_map,
                                               s_cfg.needle_source_idx,
                                               s_cfg.chart_source_idx);

    /* M4:与固件 nvs_storage_init 同款的自愈 + 老配置迁移(mock 不落盘) */
    (void)ui_dashboard_logic_sanitize(&s_cfg.dashboard);
    (void)ui_dashboard_logic_migrate_from_maps(&s_cfg.dashboard,
                                               s_cfg.temp_display_map,
                                               s_cfg.info_display_map,
                                               s_cfg.needle_source_idx,
                                               s_cfg.chart_source_idx);

    /* index = disp_item_t: CLT,IAT,OIL,LOD,TPS,RPM,SPD,BAT,OIP,BKT,BST,AFR */
    for (int i = 0; i < 12; i++) s_chart_alarm[i] = 32767; /* off */
    s_chart_alarm[8]  = 80;                      /* OIP: 8.0 bar */
    s_chart_alarm[9]  = 600;                     /* BKT: 600 C */
}

void sim_nvs_mock_configure(const sim_opts_t *opts)
{
    ensure_defaults();
    if (!opts) return;

    if (opts->profile >= 0)     s_cfg.vehicle_profile_idx = (uint8_t)opts->profile;
    if (opts->role >= 0)        s_cfg.device_role = (uint8_t)opts->role;
    if (opts->theme_slot >= 0)  s_cfg.theme_cfg.theme = (uint8_t)opts->theme_slot;
    if (opts->no_boot)          s_intro_enable = 0;   /* OFF */
    if (opts->bound) {
        /* Pre-saved adapter: boot skips the BLE scan page. */
        snprintf(s_cfg.ble_device_name, sizeof(s_cfg.ble_device_name), "SIM-ELM327");
        const uint8_t fake_mac[6] = {0x02, 0x42, 0x53, 0x49, 0x4D, 0x01}; /* locally administered, "SIM" */
        memcpy(s_cfg.ble_obd_mac, fake_mac, 6);
    }
}

esp_err_t nvs_storage_init(void)
{
    ensure_defaults();
    return 0; /* ESP_OK */
}

const nvs_user_cfg_t *nvs_cfg_get(void)
{
    ensure_defaults();
    return &s_cfg;
}

esp_err_t nvs_cfg_set(const nvs_user_cfg_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    ensure_defaults();
    s_cfg = *cfg;
    return 0;
}

int16_t nvs_chart_alarm_get(uint8_t item)
{
    ensure_defaults();
    if (item >= 12) return 32767;
    return s_chart_alarm[item];
}

void nvs_chart_alarm_set(uint8_t item, int16_t raw_threshold)
{
    ensure_defaults();
    if (item >= 12) return;
    s_chart_alarm[item] = raw_threshold;
}

uint8_t nvs_intro_enable_get(void)      { ensure_defaults(); return s_intro_enable; }
void    nvs_intro_enable_set(uint8_t en){ ensure_defaults(); s_intro_enable = en; }
uint8_t nvs_device_position_get(void)   { ensure_defaults(); return s_device_position; }
void    nvs_device_position_set(uint8_t pos) { ensure_defaults(); s_device_position = pos; }
uint8_t nvs_boot_mode_get(void)         { ensure_defaults(); return s_boot_mode; }
void    nvs_boot_mode_set(uint8_t mode) { ensure_defaults(); s_boot_mode = mode; }

const nvs_stat_t *nvs_stat_get(void)
{
    ensure_defaults();
    s_stat_valid = true;
    return &s_stat;
}

void nvs_stat_reset_trip(void)
{
    ensure_defaults();
    s_stat.trip_m = 0;
    s_stat.trip_run_time_s = 0;
}

void nvs_stat_update_speed(uint8_t speed_kmh, uint32_t dt_ms)
{
    ensure_defaults();
    uint64_t meters = (uint64_t)speed_kmh * dt_ms / 3600; /* kmh * ms / 3600 = m */
    s_stat.odometer_m += meters;
    s_stat.trip_m += meters;
    s_run_ms_total += dt_ms;
    s_stat.run_time_s = (uint32_t)(s_run_ms_total / 1000);
    s_stat.trip_run_time_s = s_stat.run_time_s; /* trip == total in the sim */
    if (speed_kmh > s_stat.max_speed_kmh) s_stat.max_speed_kmh = speed_kmh;
    if (s_run_ms_total > 0) {
        uint64_t avg = (uint64_t)((double)s_stat.odometer_m / ((double)s_run_ms_total / 3600000.0) / 1000.0);
        s_stat.avg_speed_kmh = (uint16_t)avg;
    }
    (void)s_stat_valid;
}

nvs_stat_t nvs_stat_get_mileage(void)
{
    ensure_defaults();
    return s_stat;
}

/* ---- Diagnostics error log: in-memory ring, same logic as the firmware ---- */

static nvs_error_log_t s_errlog = { .version = NVS_ERROR_LOG_VERSION };

void nvs_error_log_record(const char *tag, esp_err_t err, const char *message)
{
    nvs_error_log_logic_append(&s_errlog, 0 /* sim: uptime not tracked */, (int32_t)err, tag, message);
}

void nvs_error_log_recordf(const char *tag, esp_err_t err, const char *fmt, ...)
{
    char buf[NVS_ERROR_MSG_LEN];
    va_list ap;
    if (fmt == NULL) { nvs_error_log_record(tag, err, NULL); return; }
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    nvs_error_log_record(tag, err, buf);
}

uint8_t nvs_error_log_count(void)
{
    nvs_error_log_logic_sanitize(&s_errlog);
    return s_errlog.count;
}

void nvs_error_log_copy(nvs_error_log_t *out)
{
    if (out == NULL) return;
    *out = s_errlog;
}
