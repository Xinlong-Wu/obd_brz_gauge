#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Theme config. The index/selectors are real now (see ui_theme.c); the two
// color fields are legacy and unused, kept only to preserve struct layout.
typedef struct {
    uint8_t  theme;         // UI theme index (0=DEFAULT, registry in ui_theme.c)
    uint32_t user_theme_domiant_color;   // legacy, unused (kept for layout)
    uint32_t user_theme_secondary_color;   // legacy, unused (kept for layout)
    uint8_t  theme_slot;    // unused — only one theme partition (theme_0) exists, kept for struct layout
    uint8_t rsv[4];         // reserved for future use
} theme_cfg_t;

/*------------------ User configuration (written only when changed) ------------------*/
typedef struct {
    uint8_t protocol;      // OBD protocol: 0=auto, 1~9=fixed
    theme_cfg_t theme_cfg;   // theme config
    char    ble_device_name[32]; // last connected BLE device name, empty = not configured
    uint8_t default_page;   // default boot page: 0=Temp, 1=Info, 2=Chart, 3=Needle, 4=Gear, 5=RPM, 6=Speed
    uint8_t brightness_day; // brightness 10-100, 0=unset (use 100)
    uint8_t vehicle_profile_idx; // vehicle profile index, 0=OBD2 Generic (full list in vehicle_profiles.c)
    uint16_t brake_temp_warn_c; // brake temp warning threshold, °C (x1)
    uint16_t oil_pressure_warn_x10; // oil pressure warning threshold, 0.1bar
    uint8_t temp_display_map[3]; // TEMP page, 3 rows display-item mapping
    uint8_t info_display_map[5]; // INFO page, 5-cell display-item mapping
    uint8_t needle_source_idx;   // needle page data source (disp_item_t value, default 0=CLT)
    uint8_t device_role;         // multi-gauge role: 0=master (reads ELM327), 1=slave (receives master data)
    uint8_t chart_source_idx;    // chart page data item (disp_item_t value, default 8=OILP)
    uint16_t rpm_warn_threshold; // RPM warning threshold (rpm), 0=unset (default 6000)
    uint8_t rpm_warn_anim_en;    // RPM warning flash enable: 0=off, 1=on
    uint8_t espnow_master_mac[6];// master MAC a slave is bound to (all-zero = unbound / accept any)
    uint8_t ble_obd_mac[6];      // paired ELM327 exact MAC (all-zero = unbound, fuzzy-match by ble_device_name).
                                 // NOTE: new fields MUST be appended at the END of this struct;
                                 // see the load_blob grow logic comment in nvs_storage.c.
    uint8_t rpm_warn_linked_en;  // multi-gauge linked flash: 0=off 1=on (gauges turn red in sequence by
                                 // position, then all flash at threshold; logic in ui.c)
    uint8_t rc_enabled;          // RaceChrono BLE service: 0=off (minimal mode), 1=on (full RC+Pair+Info+OTA)
    uint8_t obd_poll_mode;       // OBD poll default gap tier: 0=NORMAL 30ms, 1=FAST 15ms, 2=TURBO 5ms.
                                 // Only applies when neither the vehicle override nor the profile pins
                                 // poll_gap_ms (see elm327_ble_client.c gap resolution). Appended LAST
                                 // for old-device NVS compatibility.
                                 // NOTE: new fields MUST be appended at the END of this struct;
                                 // see the load_blob grow logic comment in nvs_storage.c.
} nvs_user_cfg_t;

/*------------------ OBD poll mode helpers ------------------*/

#define NVS_OBD_POLL_MODE_NORMAL 0u
#define NVS_OBD_POLL_MODE_FAST   1u
#define NVS_OBD_POLL_MODE_TURBO  2u
#define NVS_OBD_POLL_MODE_COUNT  3u

/** 全局默认轮询槽间隔(ms):按用户档位取值,越界回退 NORMAL。
 *  车型 override/profile 锁定的 poll_gap_ms 优先于此值(可快不可慢被锁车拖累)。 */
static inline uint32_t nvs_obd_poll_mode_default_gap_ms(uint8_t mode)
{
    switch (mode) {
    case NVS_OBD_POLL_MODE_FAST:   return 15u;
    case NVS_OBD_POLL_MODE_TURBO:  return 5u;
    default:                       return 30u;   // NORMAL / 越界
    }
}

/*------------------ Runtime statistics (persisted periodically) ------------------*/
typedef struct {
    uint64_t odometer_m;   // total odometer (m)
    uint64_t trip_m;       // current trip distance (m)
    uint64_t run_time_s;   // total running time (s)
    uint16_t max_speed_kmh; // max speed km/h
    uint16_t avg_speed_kmh;
    uint32_t trip_run_time_s; // current trip running time (s)
    uint8_t  rsv[2];
} nvs_stat_t;

esp_err_t nvs_storage_init(void);

/* User config accessors */
const nvs_user_cfg_t * nvs_cfg_get(void);
esp_err_t nvs_cfg_set(const nvs_user_cfg_t *cfg);

// Per-item alarm threshold for the chart page (raw units; value>=threshold alarms; 32767=off). item = disp_item_t value.
int16_t nvs_chart_alarm_get(uint8_t item);
void    nvs_chart_alarm_set(uint8_t item, int16_t raw_threshold);

// Multi-gauge boot animation: 0=OFF, 1=RACE AS ONE, 2=VIDEO. Stored as a separate blob, not in the cfg struct.
uint8_t nvs_intro_enable_get(void);           // 0=OFF 1=RACE 2=VIDEO (boot_block flashed via the phone app)
void    nvs_intro_enable_set(uint8_t en);
uint8_t nvs_device_position_get(void);        // 1/2/3
void    nvs_device_position_set(uint8_t pos);

// Boot mode: 0=default Sky Gauge animation, 1=custom boot image, 2=video animation (boot_block)
uint8_t nvs_boot_mode_get(void);
void    nvs_boot_mode_set(uint8_t mode);

/* Runtime statistics accessors */
const nvs_stat_t * nvs_stat_get(void);
void nvs_stat_reset_trip(void);
void nvs_stat_update_speed(uint8_t speed_kmh,uint32_t dt_ms);
nvs_stat_t nvs_stat_get_mileage(void);
