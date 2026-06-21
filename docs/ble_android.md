# SnapHeater U1 BLE / Android control

Version: 0.4.0-dev

This firmware adds a local Bluetooth Low Energy control layer for an Android app.
BLE is intended for near-field control, emergency access and later Wi-Fi provisioning.
Normal Snapmaker U1 synchronization still uses Wi-Fi + Moonraker.

## BLE role

SnapHeater U1 acts as a BLE peripheral and advertises as:

```txt
SnapHeater U1
```

The advertised name is configurable in `idf.py menuconfig`:

```txt
SnapHeater U1 -> BLE / Android control -> BLE advertised device name
```

## Safety model

BLE reads are allowed without unlock:

- status read,
- diagnostics read,
- status notifications.

Control writes are locked by default. The app must first write:

```json
{"unlock":"123456"}
```

Change the development PIN before real use:

```txt
CONFIG_SHU1_BLE_CONTROL_PIN
```

This is an application-level lock for development. A production Android app should also use BLE pairing/bonding or an app-specific provisioning flow.

The normal heater output is still blocked unless this is explicitly enabled:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=y
```

So BLE can change desired settings, but it cannot energize the physical heater when the build-level heater lock is disabled.

## GATT service

Private service UUID:

```txt
01103155-4853-4291-7c4a-5ba100001011
```

Characteristics:

| Purpose | UUID | Properties |
|---|---|---|
| Status | `01103155-4853-4291-7c4a-5ba101001011` | Read + Notify |
| Control | `01103155-4853-4291-7c4a-5ba102001011` | Write + Write Without Response |
| Diagnostics | `01103155-4853-4291-7c4a-5ba103001011` | Read |

Note: NimBLE stores 128-bit UUIDs internally in little-endian order. Use the displayed UUIDs in Android tools only after verifying them with a scanner such as nRF Connect, because some apps display vendor UUIDs byte-reordered.

## Status characteristic

Short JSON designed for notifications:

```json
{
  "v":"0.4.0-dev",
  "on":true,
  "m":4,
  "set":60,
  "tc":60.1,
  "tp":96.0,
  "h":false,
  "f":true,
  "err":"ok",
  "dry":false,
  "rem":0,
  "ph":2,
  "pr":1720,
  "pd":false
}
```

Fields:

| Field | Meaning |
|---|---|
| `v` | firmware version |
| `on` | work_on setting |
| `m` | mode: 1 auto, 2 power/manual, 3 drying, 4 preheat/hold |
| `set` | chamber target temperature |
| `dry` | drying timer running |
| `rem` | drying remaining seconds |
| `tc` | chamber temperature |
| `tp` | PTC local temperature |
| `h` | physical heater output state |
| `f` | fan output state |
| `err` | heater/safety fault |
| `ph` | preheat phase: 0 idle, 1 heating, 2 holding, 3 complete |
| `pr` | preheat remaining seconds during hold |
| `pd` | preheat done pending; Android should show local notification |

## Control characteristic

Unlock first:

```json
{"unlock":"123456"}
```

Set manual mode to 45 C:

```json
{"work_on":true,"work_mode":2,"set_temp":45}
```

Set auto mode:

```json
{"work_on":true,"work_mode":1,"set_temp":50,"filtertemp":30,"hotbedtemp":80}
```

Start PLA drying:

```json
{"filament_drying_mode":1,"isrunning":true}
```

Start custom drying at 50 C for 8 hours:

```json
{"filament_drying_mode":4,"custom_temp":50,"custom_timer":8,"isrunning":true}
```

Stop everything:

```json
{"work_on":false,"isrunning":false}
```

The firmware clamps unsafe values:

- `set_temp`: 0 to `CONFIG_SHU1_MAX_TARGET_TEMP_C`, default max 60 C,
- `custom_temp`: 40 to max target,
- `ptc_cutoff`: 60 to hard cutoff,
- `custom_timer`: 1 to 99 h.

## Android app minimum flow

1. Scan for BLE device name `SnapHeater U1`.
2. Connect.
3. Discover services.
4. Read Diagnostics once.
5. Subscribe to Status notifications.
6. Write unlock JSON.
7. Write control JSON.
8. Show live status from notifications.

## Suggested Android UI

Main screen:

- big status: Off / Auto / Manual / Drying / Error,
- chamber temperature,
- PTC temperature,
- target temperature slider, max 60 C,
- Auto / Manual / Drying buttons,
- ON/OFF switch,
- error banner,
- Moonraker state.

Drying screen:

- PLA 55 C / 12 h,
- PETG 60 C / 12 h,
- ABS 60 C / 12 h,
- Custom 40-60 C / 1-99 h.

Diagnostics screen:

- raw ADC chamber,
- raw ADC PTC,
- sensor status,
- heater fault,
- inferred GPIO map,
- heater build lock state,
- firmware version.


## Preheat / Hold command

Start preheat to 60 C and hold the chamber for 30 minutes after target is reached:

```json
{"preheat_running":true,"preheat_target":60,"preheat_hold_min":30}
```

When the hold time expires, BLE status contains `pd=true`. The Android app should create a local phone notification and then acknowledge it:

```json
{"ack_preheat_complete":true}
```

See `docs/preheat_hold_android.md` for phase details.

## v0.5.0 BLE additions

The BLE Control characteristic also accepts:

```json
{"profile":"ASA"}
```

```json
{"material_profile":4}
```

```json
{"manual_session_max_min":120}
```

```json
{
  "wifi_ssid":"MyWiFi",
  "wifi_password":"secret",
  "moonraker_host":"192.168.1.100",
  "moonraker_port":7125
}
```

The compact BLE status notification now includes:

- `prof` / `prof_name` — selected material profile
- `sess_to` — session timeout pending; Android should show a local notification

Example:

```json
{"prof":2,"prof_name":"PETG","sess_to":false}
```

## v0.7 Android notification additions

Compact BLE status now includes:

```json
{
  "tmph": 1,
  "tmpt": 42,
  "tmpr": 1200,
  "tmpd": false
}
```

Where:

- `tmph` = tempering phase (`0` idle, `1` active, `2` complete),
- `tmps` = tempering start temperature,
- `tmpt` = current tempering target,
- `tmpe` = tempering end temperature (`0` means heater-off target),
- `tmpr` = remaining seconds,
- `tmpp` = tempering progress in percent,
- `tmpd` = tempering-complete pending notification.

The Android app should show a local notification when `tmpd=true`, then send:

```json
{"ack_tempering_complete":true}
```

The app may configure the policy with:

```json
{"tempering_enabled":true,"tempering_duration_min":30,"tempering_end_temp":0,"fan_postrun_min":5}
```


### Android UI rule for tempering duration

Tempering should not be a hidden firmware rule. In the Android app:

- show a **Tempering** checkbox in AUTO/print settings,
- when checked, open a duration picker,
- send the chosen duration to firmware,
- keep `tempering_end_temp=0` by default to mean "ramp to heater-off".

Recommended BLE control write:

```json
{"tempering_enabled":true,"tempering_duration_min":30,"tempering_end_temp":0}
```

If the user unchecks Tempering:

```json
{"tempering_enabled":false}
```

Compact BLE status includes:

```json
{"tmph":1,"tmps":60,"tmpt":42,"tmpe":0,"tmpr":1200,"tmpp":33,"tmpd":false}
```


## v1.1 Virtual Door / Open Lid Detection

SnapHeater can infer probable chamber/top-cover opening from a sudden chamber temperature drop after print finish, during tempering, keep-warm or post-run cooldown.

Example command:

```json
{
  "virtual_door_detection_enabled": true,
  "virtual_door_window_sec": 60,
  "virtual_door_drop_c": 4,
  "virtual_door_rate_c_per_min": 4,
  "virtual_door_min_base_temp": 35,
  "virtual_door_action": 1
}
```

Action values:

- `0`: notify only,
- `1`: stop post-print conditioning,
- `2`: stop heater/conditioning.

Status fields include `virtual_door_open`, `virtual_door_open_pending`, `virtual_door_last_drop_c` and `virtual_door_last_rate_c_per_min`.

Android/local UI should show a notification when `virtual_door_open_pending=true`, then acknowledge with:

```json
{"ack_virtual_door_open": true}
```

Clear the latched state with:

```json
{"clear_virtual_door_open": true}
```

## v1.3 Android UI sections

The Android app should expose the following optional panels:

- Warm-up prediction and heat soak: `heat_soak_enabled`, `heat_soak_min`, `heat_soak_band_c`.
- Filter/service counters: `filter_life_counter_enabled`, `filter_life_limit_h`.
- PLA protection warning: `pla_protection_enabled`, `pla_protection_confirmed`.
- Airflow warning: `airflow_detection_enabled`.
- Print risk and start warning: `print_risk_enabled`, `start_print_warning_enabled`.
- Recipes: `local_recipes_enabled`, `active_recipe_slot`, `active_recipe_name`.
- Demo mode: `demo_mode_enabled` for videos and app tests without live heating.
- Setup safety checklist: read `safety_score`, `setup_validation_passed`, `safety_message`.
