# Vehicle Profiles

English | [简体中文](VEHICLES.md)

Profiles are switched with the **VEHICLE** roller on the Settings page,
applying immediately. Adding a vehicle is **data-driven**: edit 2 files,
no parser changes.

```
Default behavior = standard OBD2 (SAE J1979)
Only vehicles declared in vehicle_custom_config.h get custom CAN rules
or oil-temp formulas.
```

| File | Purpose |
|------|---------|
| [main/app_obd_dsp/vehicle_profiles.c](../main/app_obd_dsp/vehicle_profiles.c) | Base parameters (gear ratios / tire / oil strategy / protocol lock)|
| [main/app_obd_dsp/vehicle_custom_config.h](../main/app_obd_dsp/vehicle_custom_config.h) | Custom overrides (CAN rules / oil formulas / UDS header switching / gear DID)|
| [main/app_obd_dsp/vehicle_profiles.h](../main/app_obd_dsp/vehicle_profiles.h) | Struct definitions + API (authoritative field reference for this page)|

## Built-in profile table

From `s_profiles[]` in `vehicle_profiles.c` (shown in this order on the
Settings page):

| # | Name | Protocol | PID addressing | Oil temp | Gear | Boost | Notes |
|---|------|----------|----------------|----------|------|-------|-------|
| 0 | `OBD2 Generic` | Auto | Physical 7E0 | `01 5C` | Ratio estimate | — | Generic SAE J1979; 6MT placeholder ratios |
| 1 | `ZN/C6 CAN` | 6 locked | Physical 7E0 | Toyota `21 01` | **0x141 direct read** (R=-1) | — | **The only CAN ATMA profile**: 0x140 throttle, 0x360 oil/coolant, 0x141 direct gear, 0x0D0 g-force, 0x6E2 TPMS (auto unit detection); RPM stays on OBD. Fully verified on a real car |
| 2 | `ZN/C6 PID` | 6 locked | Physical 7E0 | Toyota `21 01` | Ratio estimate | — | Pure-OBD fallback for cheap adapters without ATMA |
| 3 | `ZD8 OBD` | 6 locked | Physical 7E0 | `01 5C` | Ratio estimate | — | BRZ Gen2 FA24, OBD-only |
| 4 | `ZD8` | 6 locked | Physical 7E0 | `01 5C` | Ratio estimate | — | Same config as #3, standard-PID fallback variant |
| 5 | `MX-5 ND` | Auto | Physical 7E0 | `22 13 10` 2-byte → `22 11 1F` fallback | Ratio estimate | — | 40 ms timeout (Mazda CAN answers fast)|
| 6 | `BMW F/G` | 6 locked | Functional 7DF | `01 5C` | **Direct DID `DA2E`** (EGS) | ✓ | G20/G21/G22 B48/B58 + ZF 8HP; gear via 6F1 extended-address request / 618 receive filter |
| 7 | `Supra A90` | 6 locked | Functional 7DF | `22 44 02` (°C=raw×0.75−48) → `01 5C` fallback | Ratio estimate (placeholder) | ✓ | B58/B48; **OBD oil-pressure DID `4436`** (hPa), replaces ADS1115 |
| 8 | `BMW G OBD` | **7 locked** | Functional 7DF | `01 5C` → `22 44 02` fallback | Ratio estimate | ✓ | G-series auto-detect is unstable; locked to 7 |
| 9 | `BMW E` | 6 locked | Functional 7DF | `22 44 02` (6F1 header) → `22 58 22` fallback | Ratio estimate | — | E9x/E46/E39; **OBD oil-pressure DID `586F`** (6F1 header, verified on N55)|
| 10 | `JCW F56` | Auto | Physical 7E0 | MINI `22 58 22` (A−60) → `01 5C` fallback | Ratio estimate | ✓ | B48, FWD |
| 11 | `MINI R55` | 6 locked | Functional 7DF | MINI `22 58 22` → `01 5C` fallback | Ratio estimate | ✓ | N14/N18/N16 |
| 12 | `POS 997.2` | 6 locked | Physical 7E0 | `01 5C` | Ratio estimate (PDK 7-speed) | — | 987.2/997.2 DFI |
| 13 | `POS 997.1` | 6 locked | Physical 7E0 | `01 5C` | Ratio estimate | — | 987.1/997.1 M96/M97; Gen2 placeholder ratios |
| 14 | `GIULIA 2.0T` | 7 locked | **29-bit functional `18DB33F1`** | `22 13 02` @ `18DA10F1` | Ratio estimate | ✓ | Giorgio platform; slow UDS responses, 50 ms poll gap |
| 15 | `jeep` | 7 locked | 29-bit functional `18DB33F1` | `01 5C` | Ratio estimate (placeholder) | — | Generic placeholder; ratios/tire to be refined |
| 16 | `Honda Integra` | 7 locked | 29-bit functional `18DB33F1` | `01 5C` attempted | **Disabled** (CVT, `gear_count=0`) | ✓ | 11th-gen Civic platform L15C7; non-Type-R may lack a physical oil-temp sensor |
| 17 | `MX-5 NC` | auto | physical 7E0 | `01 5C` (factory oil-temp CAN path unconfirmed) | ratio estimate (placeholder ratios, refine per car) | — | **the JDM gauge's own car**: tach range 0-8000 / amber 5700 / red 7000-8000 (entering trips the alarm) / shift prompt 6800 |

Notes:

- **Gear column**: direct CAN / Mode 22 DID reads win when available
  (e.g. BMW F/G's DA2E); otherwise gear is estimated from
  RPM ÷ speed → gear ratio; `gear_count=0` (CVT) disables the estimate
- **Oil temp** has a four-level fallback chain (primary → secondary →
  tertiary → quaternary); consecutive primary failures switch automatically
- `OBD2 Generic`, `Supra A90`, `MINI R55`, `jeep` and `Honda Integra` are not
  in the override table — they rely on the enum strategy baked into their
  profile, which is equally data-driven

## Adding a vehicle

### Step 1: base parameters (append to `s_profiles[]` in `vehicle_profiles.c`)

```c
{
    .name = "My Car",                    // display name; also the override match key
    .final_drive_ratio = 3.73f,          // final drive ratio
    .tire_rolling_radius_m = 0.310f,     // tire rolling radius (m)
    .gear_count = 6,                     // forward gears; 0 for CVT (disables estimation)
    .gear_ratios = {0, 3.63f, 2.38f, 1.56f, 1.18f, 1.00f, 0.81f},
    .gear_tolerance = 0.15f,             // gear detection tolerance (±15%)
    // Optional below; omit = pure OBD2 standard + auto protocol
    .oil_temp_strategy = { ... },        // 4-level fallback chain (enum in vehicle_profiles.h)
    .has_boost = true,                   // turbo car: show boost (standard PID 01 0B)
    .forced_protocol = 6,                // lock ELM327 protocol (0=auto; lock where detection is unstable)
    .obd_functional_addr = true,         // true = ATSH7DF functional addressing (most German cars)
    .obd_29bit_functional = true,        // 29-bit functional broadcast ATSH18DB33F1 (Honda 11th gen / FCA)
    .obd_oil_pressure_did = 0x4436,      // Mode 22 oil-pressure DID (hPa); replaces ADS1115 if present
    .obd_gear_did = 0xDA2E,              // Mode 22 gear DID; replaces ratio estimation if present
    .obd_timeout = 0x0A,                 // ATST timeout (0=default 0x19; lower for fast cars)
    .poll_gap_ms = 1,                    // poll gap ms (0=default 30 ms)
    .speed_scale = 1.0f,                 // speed correction factor (0/omitted=1.0)
    .can_broadcast_mode = false,         // true = ATMA CAN monitoring (currently ZN/C6 CAN only)
    .tach = { 8000, 5700, 7000, 6800 },  // tach: full scale / amber / red (=alarm) / shift prompt
    .boost = { -10, 30, 13, 24 },        // boost gauge: low end / full scale / dim zone / red zone (=alarm), 0.1 bar
    .clt = { 60, 120, 110 },             // coolant gauge: low end / full scale / red zone (=alarm), deg C
    .oilp = { 0, 60, 10 },               // oil pressure gauge: low end / full scale / low-pressure alarm line (<= trips), 0.1 bar
    .bat = { 10800, 16400 },             // battery alarm: low / high, mV
    .tyre = { 5, 34, 85 },               // tyre: low/high pressure (0.1 bar) and temp-high (deg C) alarm lines
    .speed = { 100, 110 },               // speed page: cruise set point (SET) / overspeed alarm line (trips above), km/h
    // Tachometer range/zones (vehicle-specific; 0 = fall back to the MX-5 NC
    // defaults). The red-zone start doubles as the RPM critical alarm line
    // (entering the red band trips the alarm); keep the shift prompt below it:
    .tach = { 8000, 5700, 7000, 6800 },  // full scale / amber / red / shift prompt
},
```

**If the car only needs standard OBD2, you are done here.**

### Step 2 (optional): custom overrides (`vehicle_custom_config.h`)

Only needed for CAN broadcast decoding, non-standard UDS headers or special
oil-temp formulas.

#### 2a. CAN broadcast decode rules

```c
static const can_rule_t can_rules_mycar[] = {
    // CAN_ID  bit_off  bit_len  scale       offset  channel
    { 0x140,   16,      14,      1.0f,       0.0f,   CH_RPM },
    { 0x360,   16,       8,      1.0f,      -40.0f,  CH_OIL_TEMP },
};
```

- `bit_off` counts from the LSB (SAE J1939 style): byte0 bit0 = 0, byte1
  bit0 = 8
- Final value = raw × scale + offset
- Channels: `CH_RPM` `CH_SPEED` `CH_OIL_TEMP` `CH_COOLANT` `CH_TPS` `CH_LOAD`
  `CH_INTAKE` `CH_BOOST` `CH_GEAR`

#### 2b. Oil-temp formulas

```c
static const oil_formula_t oil_std = {
    OIL_STD_PID, {0x5C}, 1, 0, 1, 1.0f, -40.0f, 0   // standard 01 5C
};
static const oil_formula_t oil_uds = {
    OIL_UDS_22, {0x13,0x10}, 2, 0, 2, 0.01f, -40.0f, 0  // UDS 22, 2 bytes big-endian
};
```

| Type | Description | Command |
|------|-------------|---------|
| `OIL_STD_PID` | Standard Mode 01 | `01 XX\r` |
| `OIL_UDS_22` | UDS Mode 22 (1–2 bytes) | `22 XX XX\r` |
| `OIL_SPECIAL` | Special parsing (Toyota Mode 21 legacy; needs dedicated code) | custom |

#### 2c. Register the override

```c
{
    .match_name      = "My Car",        // must exactly match the name in vehicle_profiles.c
    .can_rules       = can_rules_mycar, // NULL = no CAN
    .can_rule_count  = 2,
    .oil_primary     = &oil_uds,        // primary formula (NULL = standard 01 5C)
    .oil_secondary   = &oil_std,        // fallback (switches after 5 consecutive failures)
    .forced_protocol = 6,               // 0 = auto
    .functional_addr = true,            // true = ATSH7DF
    .obd_timeout     = 0x0F,            // ATST (0 = default)
    .has_boost       = true,
    .poll_gap_ms     = 1,               // 0 = default 30 ms
    // UDS / gear-query header switching (BMW, FCA and other extended-addressing cars):
    .uds_header_cmd         = "ATSH18DA10F1\r", // header switched in for oil-temp queries, restored after
    .obd_gear_header_cmd    = "ATSH6F1\r",      // header for the gear DID (NULL = uds_header_cmd, then 7E0)
    .obd_gear_rx_filter_cmd = "ATCRA618\r",     // gear-reply filter (NULL = ELM327 default filter)
    .obd_gear_raw_frame     = "18 03 22 DA 2E\r", // raw frame sent with ATCAF0 (target byte + ISO-TP PCI)
},
```

## Runtime lookup

```
vehicle_profile_get_override()
    ├── override found  → custom CAN rules / oil formulas / UDS headers
    └── returns NULL    → pure standard OBD2 (01 0C/0D/05/5C/0F/04/11/42)
```

## CAN rule parser API

Defined in `vehicle_custom_config.h` (static inline, no separate .c):

```c
bool can_extract_bits_le(const uint8_t data[8], uint8_t bit_off,
                         uint8_t bit_len, uint32_t *out);
void can_apply_rules(const can_rule_t *rules, uint8_t count,
                     uint16_t can_id, const uint8_t data[8],
                     float channels[CH_COUNT]);
const char *oil_formula_build_cmd(const oil_formula_t *f, char *buf, size_t buflen);
int16_t oil_formula_parse_resp(const oil_formula_t *f,
                               const uint32_t *resp_data, uint8_t resp_len);
```

## Notes

1. **Names must match exactly** — if the profile `name` and the override
   `match_name` differ, the override silently never applies
2. **Gear DID vs ratio estimation** — `obd_gear_did` wins when it answers;
   on failure it falls back to ratio estimation; extended-addressing cars
   also need the three `obd_gear_*` fields (see the BMW F/G entry)
3. **Two-byte formulas** combine big-endian:
   `value = data[byte] * 256 + data[byte+1]`
4. **Migration status**: the legacy switch/case in `elm327_ble_client.c` is
   still active, bridged with the data tables via
   `vehicle_profile_get_override()`; migration to the generic parsers is
   incremental
5. To discover private PIDs for an unknown car, use the
   [fake-ELM327 hunting workflow in the tools](DEVELOPMENT.en.md#tool-scripts) —
   no car needed

## Appendix: private-DID mining record (Mazda MX-5 NC, not verified on the car)

Result of reverse-engineering the Car Scanner **Mazda profile** on 2026-10-12
with `tools/one_shot.py` (`fake_elm327.py --probe access`). The semantics and
decode formulas below are what the **profile defines**; whether the MX-5 NC
actually supports these DIDs is unverified — hook up a real adapter, send
`ATSH7E0` then each `22 XX XX` to check, and only after verification may any
of this go into profile code. The raw mining CSVs (probe log / app export)
are session artifacts and are not committed.

Responder header `7E0` (engine ECU, physical addressing); formula notation
matches the rest of the doc: A = 1st data byte of the response, B = 2nd.
Units are the profile's display units.

| DID | Profile meaning | Decode | Notes |
|-----|-----------------|--------|-------|
| `22 11 77` | MAF Volts (MAF sensor voltage) | `(A*256+B)/4096` V | |
| `22 13 40` | Actual Pedal Pos (accelerator pedal) | `A*100/256` % | Same DID range as the ND-verified oil-temp DID `13 10` |
| `22 14 10` | Injection pulse width | `(A*256+B)/125` ms | |
| `22 16 E1` | Wastegate Duty Cycle | `A*100/256` % | Turbo-Mazda (Speed3/MPS) semantics; the NA NC most likely lacks this sensor |
| `22 17 23` | Boosted Air Temperature | `A*0.625-39.4` °C | Same; coefficients fitted from 26 points, the profile's original form is probably rounder |
| `22 17 46` | Knock Retard | `A/10` ° | |
| `22 17 4F` | Actual AFR | `A*0.1148` | NA cars have no wideband; the NC most likely doesn't support it |
| `22 11 BD` | Transmission temperature | `(A*256+B)/16` °C | Mirrored on `7E1` (transmission ECU); only meaningful on AT cars |
| `22 17 B3` | Transmission temperature | `A*0.9333-49.4` °C | Coefficients fitted from 21 points (≈14/15); profile's original form unverified; answered on both 7E0 and 7E1 |
| `22 1E 1C` | Transmission temperature | `(A*256+B)/16` °C | Same |

Notes:

1. The profile has no private **engine** oil temp — it uses standard `01 5C`,
   consistent with the existing generic/BRZ profiles; for the NC, compare
   against the ND profile's verified `22 13 10` on the real car.
2. The `Wastegate`/`Boosted Air` entries come from Car Scanner's Mazda
   database for turbo models (Speed3/MPS); the NC is naturally aspirated and
   will most likely answer NO DATA — listed for completeness only.
3. The many `010C1`-style requests the app sends are legal ELM syntax
   ("trailing digit = expected response count"); `fake_elm327.py` now
   normalizes them (response and injection both fold into the base PID).
