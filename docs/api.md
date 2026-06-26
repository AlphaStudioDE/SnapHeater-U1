# SnapHeater U1 Local API

The API intentionally uses simple JSON fields close to the user-facing semantics discovered in the multi-version analysis. The implementation is new and project-owned.

## GET /api/health

```json
{"ok":true,"name":"SnapHeater U1","version":"0.2.0-dev"}
```

## GET /api/status

Returns firmware, accepted hardware pin map, settings, runtime state and printer state.

Important fields:

```json
{
  "fw_name": "SnapHeater U1",
  "fw_version": "0.2.0-dev",
  "heater_output_build_enabled": false,
  "gpio_probe_build_enabled": false,
  "hardware_pins": {
    "map_name": "panda_breath_accepted",
    "safety_state": "heater_output_locked_by_default",
    "deprecated_alias": false,
    "heater_gpio": 18,
    "fan_gpio": 3,
    "zero_cross_gpio": 7,
    "button_gpio": -1,
    "led_auto_gpio": 6,
    "led_on_gpio": 5,
    "led_off_gpio": 4,
    "chamber_adc_channel": 0,
    "ptc_adc_channel": 1,
    "heater_active_high": true,
    "fan_active_high": true,
    "heater_status": "accepted_panda_breath_map",
    "fan_status": "accepted_panda_breath_map",
    "zero_cross_status": "accepted_panda_breath_map",
    "sensor_status": "accepted_panda_breath_map"
  },
  "settings": {
    "work_on": true,
    "work_mode": 1,
    "set_temp": 50,
    "filtertemp": 30,
    "hotbedtemp": 80,
    "ptc_cutoff": 104,
    "filament_drying_mode": 1,
    "isrunning": false,
    "custom_temp": 50,
    "custom_timer": 12,
    "remaining_seconds": 0
  },
  "runtime": {
    "warehouse_temper": 42.1,
    "ptc_temp": 88.4,
    "warehouse_sensor_status": "ok",
    "ptc_sensor_status": "ok",
    "heater_requested": true,
    "heater_output_on": false,
    "fan_output_on": true,
    "ptc_heater_status": "disabled_by_build"
  },
  "printer": {
    "moonraker_connected": true,
    "print_state": "printing",
    "progress": 0.42,
    "bed_temp": 80,
    "bed_target": 80
  }
}
```

`inferred_pins` is still returned as a deprecated compatibility alias for older
tools. New clients should use `hardware_pins`.

## POST /api/settings

Accepts partial updates:

```json
{
  "work_on": true,
  "work_mode": 1,
  "set_temp": 50,
  "filtertemp": 30,
  "hotbedtemp": 80,
  "ptc_cutoff": 104,
  "filament_drying_mode": 1,
  "isrunning": true,
  "custom_temp": 55,
  "custom_timer": 8
}
```

Modes:

```txt
1 = Auto
2 = Power On / Manual
3 = Filament Drying
```

Drying modes:

```txt
1 = PLA, 55 C / 12 h
2 = PETG, 60 C / 12 h
3 = ABS, 60 C / 12 h
4 = Custom temperature / custom timer
```

## POST /api/probe

Disabled by default. Compile with:

```txt
CONFIG_SHU1_ENABLE_GPIO_PROBE=y
```

Then use only on a supervised bench:

```json
{"output":"fan","duration_ms":1000}
```

or:

```json
{"output":"heater","duration_ms":200}
```

The heater probe pulse is clamped by `CONFIG_SHU1_MAX_HEATER_PROBE_MS`. This endpoint can energize physical outputs and must not be exposed casually.

## Preheat / Hold mode

Start preheating and hold the chamber after the target temperature is reached:

```bash
curl -X POST http://snapheater.local/api/settings \
  -H 'Content-Type: application/json' \
  -d '{"preheat_running":true,"preheat_target":60,"preheat_hold_min":30}'
```

Stop preheating:

```bash
curl -X POST http://snapheater.local/api/settings \
  -H 'Content-Type: application/json' \
  -d '{"preheat_running":false}'
```

Acknowledge a completed preheat event after the Android app shows a phone notification:

```bash
curl -X POST http://snapheater.local/api/settings \
  -H 'Content-Type: application/json' \
  -d '{"ack_preheat_complete":true}'
```

Relevant status fields:

- `preheat_running` — true while preheat/hold is active.
- `preheat_target` — chamber target temperature.
- `preheat_hold_min` — hold time in minutes.
- `preheat_phase` — `0` idle, `1` heating, `2` holding, `3` complete.
- `preheat_remaining_seconds` — remaining hold time after target was reached.
- `preheat_complete_pending` — true until the app acknowledges the completion event.

## v0.5.0 additions

### Material profile

```json
{"profile":"PETG"}
```

or:

```json
{"material_profile":2}
```

Available IDs:

```text
0 custom, 1 PLA, 2 PETG, 3 ABS, 4 ASA, 5 TPU, 6 NYLON, 7 PC
```

### Session watchdog

`manual_session_max_min` limits the maximum time a user-started heater session can remain active.
`0` disables the watchdog. Default is 240 minutes.

```json
{"manual_session_max_min":120}
```

If it expires, status contains:

```json
{"session_timeout_pending":true}
```

Acknowledge from app:

```json
{"ack_session_timeout":true}
```

### Device provisioning

```json
{
  "wifi_ssid":"MyWiFi",
  "wifi_password":"secret",
  "moonraker_host":"192.168.1.100",
  "moonraker_port":7125
}
```

Reboot after changing Wi-Fi/Moonraker config.

### Event log

```text
GET /api/events
```

### Factory reset

```json
{"factory_reset":true}
```


## v0.6.0 U1 printer fields

`GET /api/status` now includes extended Snapmaker U1 fields under `printer`, including:

```json
{
  "moonraker_connected": true,
  "klippy_ready": true,
  "subscribed": true,
  "autodetect_done": true,
  "normalized_state": "printing",
  "active_tool": 2,
  "active_tool_object": "extruder2",
  "active_tool_temp": 218.4,
  "active_material": "PETG",
  "active_color_rgba": "#FF8800FF",
  "u1_chamber_object": "temperature_sensor cavity",
  "u1_chamber_temp": 41.2
}
```

The local heater safety loop still uses SnapHeater's own local chamber/PTC sensors. U1 chamber data is used as printer telemetry/diagnostics unless explicitly reused in later features.

## v0.8.0 smart heating / tempering fields

These fields implement the corrected heating policy:

```json
{
  "fan_postrun_min": 5,
  "tempering_enabled": false,
  "tempering_end_temp": 0,
  "tempering_duration_min": 30,
  "ack_tempering_complete": true,
  "cancel_tempering": true
}
```

Meaning:

- `fan_postrun_min` keeps fan/filter output active after heater output turns off.
- `tempering_enabled` is a user/app option. When true, SnapHeater performs gradual chamber target reduction after AUTO print completion. When false, AUTO stops heating normally and uses fan post-run/cooldown only.
- `tempering_end_temp` is the final virtual chamber target after print finish. `0` means ramp down to heater-off.
- `tempering_duration_min` is selected by the user in the Android app and defines how long the ramp is stretched.
- `ack_tempering_complete` lets Android clear the pending notification flag.
- `cancel_tempering` aborts active tempering.

Mode priority:

- Preheat/Hold ignores printer state and hotbed threshold.
- Drying ignores printer state and hotbed threshold.
- Manual/Power On ignores printer state and hotbed threshold.
- AUTO is the only print-aware mode.
- U1 pause/error does not hard-stop chamber hold; local heater/sensor faults still override everything.


## v0.9.0 Android-selected tempering duration

Tempering is controlled by the Android/user setting. The recommended app behavior is:

1. User enables a **Tempering** checkbox.
2. App opens a minute picker, e.g. 15 / 30 / 45 / 60 min.
3. App writes:

```json
{"tempering_enabled":true,"tempering_duration_min":30,"tempering_end_temp":0}
```

`tempering_end_temp=0` means "ramp to heater-off". After AUTO print completion SnapHeater measures/chooses the start temperature from the final chamber/target temperature and calculates a linear virtual target:

```txt
current_target = start_temp + (end_temp - start_temp) * elapsed / duration
```

Example: if the chamber/target at print finish is 60°C and the user selected 30 minutes, the virtual chamber target is lowered evenly over 30 minutes until heating is completely disabled.

Status fields added/clarified:

```json
{
  "tempering_start_temp": 60,
  "tempering_current_target": 42,
  "tempering_end_temp": 0,
  "tempering_duration_min": 30,
  "tempering_remaining_seconds": 1200,
  "tempering_progress_pct": 33,
  "tempering_ramp_to_off": true
}
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

## v1.3 extended intelligence fields

The following fields can be sent through `POST /api/settings` and BLE Control:

```json
{"warmup_prediction_enabled":true}
```

```json
{"heat_soak_enabled":true,"heat_soak_min":15,"heat_soak_band_c":2}
```

```json
{"filter_life_counter_enabled":true,"filter_life_limit_h":120,"ack_filter_life_warning":true}
```

```json
{"heater_wear_tracking_enabled":true,"heater_wear_warning_pct":35,"ack_heater_wear_warning":true}
```

```json
{"airflow_detection_enabled":true,"ack_airflow_warning":true}
```

```json
{"pla_protection_enabled":true,"pla_protection_confirmed":true,"ack_pla_protection":true}
```

```json
{"smart_resume_enabled":true,"resume_recover_min":5}
```

```json
{"post_print_pickup_mode":1,"pickup_keep_warm_min":60}
```

```json
{"print_risk_enabled":true,"ack_print_risk_warning":true,"start_print_warning_enabled":true,"ack_start_print_warning":true}
```

```json
{"local_recipes_enabled":true,"active_recipe_slot":2,"active_recipe_name":"ASA Large Print"}
```

```json
{"demo_mode_enabled":true}
```

```json
{"safety_score_enabled":true,"ack_setup_warning":true}
```

The REST status response includes the corresponding runtime fields such as `warmup_eta_sec`, `heat_soak_ready`, `filter_life_pct`, `heater_wear_pct`, `airflow_warning_pending`, `print_risk_score`, `print_risk_message`, `safety_score`, and `safety_message`.
