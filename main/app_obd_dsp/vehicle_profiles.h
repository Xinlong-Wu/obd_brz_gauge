#pragma once

#include <stdint.h>
#include "obd_data_cache.h"
#include "vehicle_custom_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define VEHICLE_MAX_GEARS 9   // accommodates index0 + up to 8 forward gears (e.g. ZF 8HP)

// Oil temp query mode priority
typedef enum {
    OIL_TEMP_MODE_NONE = 0xFF,
    OIL_TEMP_MODE_PID_5C = 0,       // Standard PID 01 5C
    OIL_TEMP_MODE_UDS_22_10_17 = 1, // UDS Mode 22 10 17
    OIL_TEMP_MODE_TOYOTA_21_01 = 2, // Toyota Mode 21 01
    OIL_TEMP_MODE_MAZDA_22_111F = 3, // Mazda Skyactiv Mode 22 PID 111F, single byte A-50
    OIL_TEMP_MODE_MAZDA_22_1310 = 4, // Mazda Skyactiv Mode 22 PID 1310, two bytes (A*256+B)/100-40
    OIL_TEMP_MODE_MINI_22_5822 = 5,    // MINI/BMW Mode 22 PID 5822, single byte °C = A-60 (°F=A*9/5-76)
    OIL_TEMP_MODE_BMW_22_4402 = 6,     // BMW F-series (F48 etc., B38/B48) Mode 22 PID 4402, second byte °C = B-64
    OIL_TEMP_MODE_BMW_22_03F3 = 7,     // BMW G-series Mode 22 PID 03F3, single byte °C = A-40
    OIL_TEMP_MODE_BMW_G_22_4402 = 8,   // BMW G-series Mode 22 PID 4402, two bytes °C = (A*256+B)*191.25/255-48
    OIL_TEMP_MODE_BMW_22_D002 = 9,    // BMW G-series Mode 22 PID D002, two bytes °C = (A*256+B)*191.25/255-48
    OIL_TEMP_MODE_BMW_22_111F = 10,    // BMW Mode 22 PID 111F (Header 7E0), single byte °C = A-50
} oil_temp_query_mode_t;

// Vehicle gear ratio ranges (used for gear detection)
typedef struct {
    float min_ratio;
    float max_ratio;
    enGear gear;
} gear_ratio_range_t;

// Tachometer range/zone configuration (per vehicle; 0 = fall back to the
// MX-5 NC defaults below). The red zone start is also the RPM critical
// threshold (entering the red band trips the alarm), by construction.
typedef struct {
    uint16_t rpm_max;     // full scale (dial end)
    uint16_t amber_rpm;   // amber (warning) zone start
    uint16_t red_rpm;     // red zone start = RPM critical threshold
    uint16_t shift_rpm;   // shift prompt (advisory), must be below red
} tach_cfg_t;

// MX-5 NC defaults (also the fallback for profiles that leave tach zeroed).
// Sanity-checked at compile time.
#define VEHICLE_TACH_RPM_MAX_DEFAULT   8000
#define VEHICLE_TACH_AMBER_RPM_DEFAULT 5700
#define VEHICLE_TACH_RED_RPM_DEFAULT   7000
#define VEHICLE_TACH_SHIFT_RPM_DEFAULT 6800
#define VEHICLE_TACH_CFG_DEFAULT                                              \
    {                                                                         \
        VEHICLE_TACH_RPM_MAX_DEFAULT, VEHICLE_TACH_AMBER_RPM_DEFAULT,         \
        VEHICLE_TACH_RED_RPM_DEFAULT, VEHICLE_TACH_SHIFT_RPM_DEFAULT,         \
    }
_Static_assert(VEHICLE_TACH_AMBER_RPM_DEFAULT < VEHICLE_TACH_RED_RPM_DEFAULT,
               "tach amber zone must precede red zone");
_Static_assert(VEHICLE_TACH_RED_RPM_DEFAULT <= VEHICLE_TACH_RPM_MAX_DEFAULT,
               "tach red zone must not exceed full scale");
_Static_assert(VEHICLE_TACH_SHIFT_RPM_DEFAULT < VEHICLE_TACH_RED_RPM_DEFAULT,
               "tach shift prompt must precede the red zone");

// Boost/vacuum gauge configuration (per vehicle; 0 = fall back to the
// MX-5 NC defaults below). Values in 0.1 bar (e.g. 30 = 3.0 bar).
// red_x10 doubles as the BOOST HIGH alarm threshold; dim_x10 is where
// the scale ticks dim down approaching the red zone.
typedef struct {
    int16_t min_x10;   // dial low end (-10 = -1.0 bar vacuum)
    int16_t max_x10;   // full scale (30 = 3.0 bar)
    int16_t dim_x10;   // dimmed tick zone start (13 = 1.3 bar)
    int16_t red_x10;   // red tick zone start = BOOST HIGH threshold (24 = 2.4 bar)
} boost_cfg_t;

#define VEHICLE_BOOST_MIN_DEFAULT   (-10)
#define VEHICLE_BOOST_MAX_DEFAULT   30
#define VEHICLE_BOOST_DIM_DEFAULT   13
#define VEHICLE_BOOST_RED_DEFAULT   24
#define VEHICLE_BOOST_CFG_DEFAULT { -10, 30, 13, 24 }
_Static_assert(VEHICLE_BOOST_MIN_DEFAULT < VEHICLE_BOOST_DIM_DEFAULT,
               "boost dim zone must follow the dial low end");
_Static_assert(VEHICLE_BOOST_DIM_DEFAULT < VEHICLE_BOOST_RED_DEFAULT,
               "boost dim zone must precede the red zone");
_Static_assert(VEHICLE_BOOST_RED_DEFAULT <= VEHICLE_BOOST_MAX_DEFAULT,
               "boost red zone must not exceed full scale");

// Coolant temperature gauge configuration (per vehicle; 0 = fall back to
// the MX-5 NC defaults below). Values in deg C. red_c doubles as the
// overheat alarm threshold.
typedef struct {
    int16_t min_c;     // dial low end (60)
    int16_t max_c;     // full scale (120)
    uint16_t red_c;    // red zone start = overheat alarm threshold (110)
} clt_cfg_t;

#define VEHICLE_CLT_MIN_DEFAULT   60
#define VEHICLE_CLT_MAX_DEFAULT   120
#define VEHICLE_CLT_RED_DEFAULT   110
#define VEHICLE_CLT_CFG_DEFAULT { 60, 120, 110 }
_Static_assert(VEHICLE_CLT_MIN_DEFAULT < VEHICLE_CLT_RED_DEFAULT,
               "coolant red zone must follow the dial low end");
_Static_assert(VEHICLE_CLT_RED_DEFAULT <= VEHICLE_CLT_MAX_DEFAULT,
               "coolant red zone must not exceed full scale");

// Engine oil temperature gauge configuration (per vehicle; 0 = fall back to
// the MX-5 NC defaults below). Values in deg C. red_c doubles as the
// oil temp alarm threshold and the dial's red tick zone start.
typedef struct {
    int16_t min_c;     // dial low end (40)
    int16_t max_c;     // full scale (140)
    uint16_t red_c;    // red zone start = oil temp alarm threshold (130)
} oil_cfg_t;

#define VEHICLE_OIL_MIN_DEFAULT   40
#define VEHICLE_OIL_MAX_DEFAULT   140
#define VEHICLE_OIL_RED_DEFAULT   130
#define VEHICLE_OIL_CFG_DEFAULT { 40, 140, 130 }
_Static_assert(VEHICLE_OIL_MIN_DEFAULT < VEHICLE_OIL_RED_DEFAULT,
               "oil red zone must follow the dial low end");
_Static_assert(VEHICLE_OIL_RED_DEFAULT <= VEHICLE_OIL_MAX_DEFAULT,
               "oil red zone must not exceed full scale");

// Oil pressure gauge configuration (per vehicle; 0 = fall back to the
// MX-5 NC defaults below). Values in 0.1 bar. low_x10 is the low pressure
// alarm threshold (needle/arc turn red at or below it).
typedef struct {
    uint16_t min_x10;   // dial low end (0)
    uint16_t max_x10;   // full scale (60 = 6 bar)
    uint16_t low_x10;   // low pressure alarm threshold (10 = 1.0 bar)
} oilp_cfg_t;

#define VEHICLE_OILP_MAX_DEFAULT   60
#define VEHICLE_OILP_LOW_DEFAULT   10
#define VEHICLE_OILP_CFG_DEFAULT { 0, 60, 10 }
_Static_assert(VEHICLE_OILP_LOW_DEFAULT <= VEHICLE_OILP_MAX_DEFAULT,
               "oil pressure low threshold must not exceed full scale");

// Battery voltage alarm thresholds (per vehicle; 0 = fall back to the
// MX-5 NC defaults below). Values in mV.
typedef struct {
    uint16_t low_mv;    // low voltage alarm (10800 = 10.8 V)
    uint16_t high_mv;   // high voltage alarm (16400 = 16.4 V)
} bat_cfg_t;

#define VEHICLE_BAT_LOW_DEFAULT   10800
#define VEHICLE_BAT_HIGH_DEFAULT  16400
#define VEHICLE_BAT_CFG_DEFAULT { 10800, 16400 }
_Static_assert(VEHICLE_BAT_LOW_DEFAULT < VEHICLE_BAT_HIGH_DEFAULT,
               "battery low threshold must precede high");
_Static_assert(VEHICLE_OILP_LOW_DEFAULT > 0, "oil pressure low threshold must be positive");

// Tire pressure/temperature alarm configuration (per vehicle; 0 = fall
// back to the MX-5 NC defaults below). Pressures in 0.1 bar, temp in deg C.
typedef struct {
    uint16_t press_low_x10;   // low pressure alarm (5 = 0.5 bar)
    uint16_t press_high_x10;  // high pressure alarm (34 = 3.4 bar)
    uint16_t temp_high_c;     // tire temp alarm (85 deg C)
} tyre_cfg_t;

#define VEHICLE_TYRE_PRESS_LOW_DEFAULT   5
#define VEHICLE_TYRE_PRESS_HIGH_DEFAULT  34
#define VEHICLE_TYRE_TEMP_HIGH_DEFAULT   85
#define VEHICLE_TYRE_CFG_DEFAULT { 5, 34, 85 }
_Static_assert(VEHICLE_TYRE_PRESS_LOW_DEFAULT < VEHICLE_TYRE_PRESS_HIGH_DEFAULT,
               "tyre low threshold must precede high");

// Oil temp query strategy
typedef struct {
    oil_temp_query_mode_t primary;     // preferred query mode
    oil_temp_query_mode_t secondary;   // backup 1
    oil_temp_query_mode_t tertiary;    // backup 2
    oil_temp_query_mode_t quaternary;  // backup 3 (final fallback)
    // Reserved coefficients for legacy special formulas. When can_den=0, the parser falls back to its built-in default.
    int16_t can_num;
    int16_t can_den;
    int16_t can_off;
} oil_temp_strategy_t;

// Vehicle parameter configuration
typedef struct {
    const char *name;                    // display name (e.g. "BRZ ZC6")
    float final_drive_ratio;             // final drive ratio
    float tire_rolling_radius_m;         // tire rolling radius (m)
    uint8_t gear_count;                  // number of forward gears (5 or 6)
    float gear_ratios[VEHICLE_MAX_GEARS]; // per-gear ratios, index 0 unused, 1~gear_count valid
    float gear_tolerance;                // gear detection tolerance (e.g. 0.15 = ±15%)
    oil_temp_strategy_t oil_temp_strategy; // oil temp query strategy
    bool has_boost;                      // whether turbocharged (decides whether to query/display boost pressure)
    uint16_t obd_oil_pressure_did;       // 0=off; else Mode 22 DID for OBD oil pressure (4436=B58 hPa, 586F=N55 hPa); supersedes the ADS1115 ADC
    uint16_t obd_gear_did;               // 0=off; else Mode 22 DID for direct gear read (D031=BMW ZF 8HP current gear); supersedes ratio-based gear calc
    uint8_t forced_protocol;             // forced ELM327 protocol number (ATSP), 0=auto-detect; lock to 6 for cars like BMW where auto-detect is unstable
    bool obd_functional_addr;            // true=standard PIDs use functional addressing (ATSH 7DF, same as phone apps); false=physical addressing (ATSH 7E0, Subaru etc.)
    bool obd_29bit_functional;           // true=29-bit CAN functional broadcast (ATSH 18DB33F1) for standard PIDs; only valid when obd_functional_addr=true and forced_protocol=7/9; Honda Integra/Civic 11th gen need this
    float speed_scale;                   // speed correction factor (read value × this factor), 0 or unset = 1.0 (no correction)
    uint8_t obd_timeout;                 // ATST timeout value (ELM327 units, 0=default 0x19); for BMW G OBD fast responses, set 0x0F to reduce NO DATA waits
    uint8_t poll_gap_ms;                 // poll slot interval (ms), 0=use the default OBD_POLL_SLOT_GAP_MS(30ms)
    bool can_broadcast_mode;             // true=read data by listening to CAN broadcast frames via ATMA (currently ZN/C6 CAN only), replacing standard OBD PID polling
    tach_cfg_t tach;                     // tachometer range/zones; zero fields fall back to VEHICLE_TACH_CFG_DEFAULT (MX-5 NC)
    boost_cfg_t boost;                   // boost/vacuum gauge range/zones (0.1 bar); zero fields fall back to VEHICLE_BOOST_CFG_DEFAULT
    clt_cfg_t clt;                       // coolant gauge range/red zone (deg C); zero fields fall back to VEHICLE_CLT_CFG_DEFAULT
    oil_cfg_t oil;                       // oil temp gauge range/red zone (deg C); zero fields fall back to VEHICLE_OIL_CFG_DEFAULT
    oilp_cfg_t oilp;                     // oil pressure gauge range/low alarm (0.1 bar); zero fields fall back to VEHICLE_OILP_CFG_DEFAULT
    bat_cfg_t bat;                       // battery voltage alarm (mV); zero fields fall back to VEHICLE_BAT_CFG_DEFAULT
    tyre_cfg_t tyre;                     // tyre pressure/temp alarms; zero fields fall back to VEHICLE_TYRE_CFG_DEFAULT
} vehicle_profile_t;

// Get all predefined vehicle profiles
const vehicle_profile_t *vehicle_profile_get_all(uint8_t *count);

// Get the vehicle profile at the given index
const vehicle_profile_t *vehicle_profile_get(uint8_t index);

// Get the currently active vehicle profile
const vehicle_profile_t *vehicle_profile_get_active(void);

// Set the active vehicle profile (also saved to NVS)
void vehicle_profile_set_active(uint8_t index);

// Calculate the speed constant: 1 / (final_drive * 0.377 * tire_radius)
float vehicle_profile_calc_constant(const vehicle_profile_t *p);

// Generate the gear range array from the currently active vehicle profile
// Returns the range array pointer; count outputs the number of valid elements
const gear_ratio_range_t *vehicle_profile_get_gear_ranges(uint8_t *count);

// Get the oil temp query strategy of the currently active vehicle
const oil_temp_strategy_t *vehicle_profile_get_oil_temp_strategy(void);

// Get the override configuration of the currently active vehicle (NULL = pure OBD2 standard)
const vehicle_override_t *vehicle_profile_get_override(void);

// Get the tachometer range/zone config of the currently active vehicle
// (zero fields backfilled from the MX-5 NC defaults; relations clamped)
const tach_cfg_t *vehicle_profile_get_tach_cfg(void);

// Get the boost/vacuum gauge config of the currently active vehicle
// (zero fields backfilled from the MX-5 NC defaults; relations clamped)
const boost_cfg_t *vehicle_profile_get_boost_cfg(void);

// Get the coolant gauge config of the currently active vehicle
// (zero fields backfilled from the MX-5 NC defaults; relations clamped)
const clt_cfg_t *vehicle_profile_get_clt_cfg(void);

// Get the oil temp gauge config of the currently active vehicle
// (zero fields backfilled from the MX-5 NC defaults; relations clamped)
const oil_cfg_t *vehicle_profile_get_oil_cfg(void);

// Get the oil pressure gauge config of the currently active vehicle
// (zero fields backfilled from the MX-5 NC defaults)
const oilp_cfg_t *vehicle_profile_get_oilp_cfg(void);

// Get the battery alarm thresholds of the currently active vehicle
// (zero fields backfilled from the MX-5 NC defaults)
const bat_cfg_t *vehicle_profile_get_bat_cfg(void);

// Get the tyre alarm config of the currently active vehicle
// (zero fields backfilled from the MX-5 NC defaults)
const tyre_cfg_t *vehicle_profile_get_tyre_cfg(void);

#ifdef __cplusplus
}
#endif
