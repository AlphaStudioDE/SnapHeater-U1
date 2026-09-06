# SnapHeater U1 Local API

> 2026-09-06: Anti-Warp, Large Print Protection, Safe Overnight, recipes and
> Showcase metadata and legacy Smart Resume have been retired. Their descriptions/payload examples below
> are historical, not supported controls. See [Current implementation](CURRENT_IMPLEMENTATION.md)
> and [pause/history update](LOCAL_HISTORY_PAUSE_UPDATE.md) for current behavior.

The API intentionally uses simple JSON fields close to the user-facing semantics discovered in the multi-version analysis. The implementation is new and project-owned.

Every mutating REST request requires `X-DragonBreath-Auth` matching the stored
`app_nvs/ctl_token` exactly. Missing/empty credentials and NVS read errors deny
access; header presence alone is never sufficient. Provision the first token
through a PIN-unlocked BLE session using the dedicated `rest_token` command.
New tokens must contain 16–64 characters. Existing nonempty credentials continue
to work. The server intentionally sends no CORS headers. Use a trusted LAN;
this bearer-token HTTP interface is not an encrypted transport.

## Connection and receive limits

The server handles one request per TCP connection and replies with
`Connection: close`. Clients must reconnect for subsequent requests. All GET
routes and `POST /api/v2/boot-inactive` reject nonempty bodies with HTTP 400,
without draining the supplied body. This also applies to early handler exits.
Header reception has a 2-second budget from connection acceptance; ordinary
body reception has a 2-second budget, OTA reception 60 seconds. A pending socket
read can take up to its separate 1-second timeout before closure. These budgets
do not depend on the client continuing to send individual bytes.

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
  "control": {
    "owner": "rest",
    "state_revision": 12,
    "lease_active": true,
    "lease_remaining_ms": 298000,
    "takeover_available": true
  },
  "hardware_pins": {
    "map_name": "panda_breath_accepted",
    "safety_state": "heater_output_build_disabled",
    "deprecated_alias": false,
    "heater_gpio": 18,
    "fan_gpio": 3,
    "zero_cross_gpio": 7,
    "button_gpio": -1,
    "button_power_gpio": 9,
    "button_auto_gpio": 8,
    "button_on_gpio": 10,
    "button_dry_gpio": 2,
    "led_auto_gpio": 6,
    "led_on_gpio": 5,
    "led_dry_gpio": 4,
    "led_power_gpio": -1,
    "chamber_adc_channel": 0,
    "ptc_adc_channel": 1,
    "heater_active_high": true,
    "fan_active_high": true,
    "fan_triac_control": true,
    "fan_drive_mode": "held_gate_zero_cross_on_immediate_off",
    "rref_strap_gpio": 19,
    "rref_kohm": 82,
    "heater_status": "dragonbreath_inferred_continuity_required",
    "fan_status": "dragonbreath_confirmed",
    "zero_cross_status": "dragonbreath_confirmed",
    "chamber_adc_status": "dragonbreath_inferred_continuity_required",
    "ptc_adc_status": "dragonbreath_inferred_continuity_required",
    "sensor_status": "dragonbreath_inferred_continuity_required"
  },
  "settings": {
    "work_on": true,
    "work_mode": 1,
    "set_temp": 50,
    "filtertemp": 30,
    "hotbedtemp": 80,
    "ptc_cutoff": 0,
    "filament_drying_mode": 1,
    "isrunning": false,
    "custom_temp": 50,
    "custom_timer": 12,
    "remaining_seconds": 0
  },
  "runtime": {
    "warehouse_temper": 42.1,
    "ptc_temp": 88.4,
    "warehouse_instant_temp": 42.3,
    "ptc_instant_temp": 89.0,
    "warehouse_temp_offset": 0.0,
    "ptc_temp_offset": 0.0,
    "warehouse_sensor_status": "ok",
    "ptc_sensor_status": "ok",
    "heater_requested": true,
    "heater_output_on": false,
    "fan_output_on": true,
    "ptc_heater_status": "disabled_by_build",
    "persistent_fault_latched": false,
    "persistent_fault": "ok"
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

## OTA and stock return

`GET /api/v2/ota` reports the running/boot slot and the bootable application in
the inactive slot. The same `ota` object is included in `/api/status`.

Authenticated `POST /update` (alias `/api/v2/update`) accepts a raw ESP-IDF
application `.bin`, never a complete 4 MB flash dump. OTA must first be enabled
in settings and the heater, fan and every heat-capable workflow must be fully
off. Firmware then:

1. selects only `esp_ota_get_next_update_partition(NULL)`;
2. rejects empty/oversized input;
3. streams the image through `esp_ota_write` while calculating SHA-256;
4. compares the received whole-file SHA-256 with the required 64-hex-digit
   `X-SnapHeater-SHA256` request header before `esp_ota_end` and boot selection;
   missing/malformed headers are rejected before flash writes, and mismatches
   abort the upload and invalidate its first sector; `esp_ota_end` then validates the ESP image;
5. accepts only project identities `SnapHeater_U1`, `dragonbreath` or stock
   `panda_breath` and erases the first sector of a rejected image;
6. selects the new boot slot only after all checks pass;
7. reboots, then marks the image valid only after the safety loop reports a
   healthy startup. Otherwise the bootloader rollback remains active.

Example:

```bash
curl -X POST http://DEVICE/api/settings \
  -H "X-DragonBreath-Auth: TOKEN" -H "Content-Type: application/json" \
  -d '{"ota_enabled":true}'
curl -X POST http://DEVICE/update \
  -H "X-DragonBreath-Auth: TOKEN" -H "Content-Type: application/octet-stream" \
  -H "X-SnapHeater-SHA256: $(sha256sum build/SnapHeater_U1.bin | cut -d ' ' -f1)" \
  --data-binary @build/SnapHeater_U1.bin
```

Clients must send this header with every OTA upload; older clients without it
receive `expected_sha256_required`. A differing hash returns `sha256_mismatch`
without selecting the uploaded image or scheduling a restart. This checks
transfer integrity, not authenticity: no signing key or signature is required.

Authenticated `POST /api/v2/boot-inactive` selects and reboots into an already
bootable inactive image only when its identity is one of the three accepted
projects. This is the Wi-Fi return-to-stock path when that slot contains the
stock `panda_breath` application. It never writes the bootloader, partition
table, NVS or SPIFFS.

## POST /api/settings

Requires `X-DragonBreath-Auth`.

Accepts partial updates:

```json
{
  "work_on": true,
  "work_mode": 1,
  "set_temp": 50,
  "filtertemp": 30,
  "hotbedtemp": 80,
  "ptc_cutoff": 0,
  "filament_drying_mode": 1,
  "isrunning": true,
  "custom_temp": 55,
  "custom_timer": 8,
  "warehouse_temp_offset": 0.0,
  "ptc_temp_offset": 0.0
}
```

`ptc_cutoff: 0` selects the DragonBreath board-specific automatic foldback
(33k: 99/96 C; 82k: 102/99 C). A positive override is clamped to 90..104 C.

Both NTC offsets are persisted and clamped to the DragonBreath range of
`-5.0..+5.0 C`. Control temperature is the 5-sample average; the two
`*_instant_temp` fields expose the unaveraged values used for hard cutoffs.

To request clearing a persisted heater fault, send
`{"clear_heater_fault":true}`. This first safe-stops all heat workflows and
does not clear the latch unless the control task sees idle operation, valid
sensors and temperatures below both hard cutoffs.
The request is a one-shot attempt, not a deferred clear: if conditions are unsafe,
send a new request after resolving the fault. Clearing does not restart heating.
Acquisition freshness failures and suspected dual-raw freezes use `sensor_fault`;
see [sensor diagnostics](SENSOR_DIAGNOSTICS.md) for detection and recovery limits.
`runtime.sensor_freeze_warning_ms` identifies a pending early warning (zero if
none); `runtime.sensor_freeze_remaining_s` reports Panda-owned time remaining.
The compact BLE status exposes the same keys at its root. A pending warning is
not a fault-clear opportunity and acknowledgement cannot alter its deadline.

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

Heater commands are deliberately rejected:

```json
{"output":"heater","duration_ms":200}
```

The only accepted probe output is the fan. Heater operation is allowed only
through the normal PID path after sensor, foldback, zero-cross, airflow, output
latch and persistent-fault checks. The fan endpoint can energize a physical
output and must not be exposed casually.

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
{"factory_reset":"factory-reset","expected_revision":13}
```


Reset must be a dedicated request with the current revision from status; mixing
it with other settings is rejected before changing state. All reset paths,
including Power+Auto, require inactive workflows, outputs OFF, no fault latch,
and both valid instantaneous NTC readings below 30°C and at most 1.5 seconds old.
Reset preserves the fault record, NTC offsets and REST credential. A failed erase
does not schedule a reboot and leaves maintenance active for deliberate recovery.

Accepted targets are normalized to the current validation ceiling of 55°C across
profiles and workflows. This is a setpoint ceiling, not an actual-temperature
guarantee. Persistence errors reject the proposed start and inhibit heat for the
remainder of the boot; individual NVS writes are not a rollback transaction.

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
  "cool_release": 40,
  "tempering_enabled": false,
  "tempering_end_temp": 0,
  "tempering_duration_min": 30,
  "ack_tempering_complete": true,
  "cancel_tempering": true
}
```

Meaning:

- `cool_release` is clamped to 30–65 °C. After a heating session, the fan starts
  unless both NTCs confirm less than `cool_release + 3`; it stops only after both
  are known below `cool_release`. An unknown sensor retains airflow.

Remote manual (`POWER_ON`) start returns a random 32-character `lease_id`.
The caller must renew that exact lease using authenticated
`POST /api/v2/heartbeat` with `{"lease_id":"..."}`. The fixed deadline is five
minutes. Expiry invalidates remote command authority and transfers an accepted
running/scheduled job to the local firmware executor (`owner: local_job`).
It does not stop the job or create a controller-link-loss fault. Local sensors,
Moonraker readiness for AUTO, task deadlines and fault/OTA governors still apply.
OFF is always available. After reconnecting, read current status before issuing
a new revision-checked takeover command; never replay an old command.

REST, BLE and the physical panel all remain usable. They do not write one active
heat session concurrently:

- the first start claims the session and increments `control.state_revision`;
- every later mutation by that remote owner includes its `lease_id` and the last
  observed `expected_revision`;
- a stale revision returns HTTP 409 and a missing/wrong owner or lease returns
  HTTP 423;
- another remote channel may send `"takeover":true`; firmware forces the heater
  output off before atomically replacing the owner and issuing a new lease;
- any channel can always request OFF, safe-stop or emergency-stop; this releases
  the owner and invalidates its lease;
- a physical button action takes local ownership and invalidates a remote lease;
- Moonraker only updates printer telemetry. AUTO remains owned by the REST, BLE
  or physical channel that started it, and Moonraker freshness is an additional
  condition for heating rather than a competing command source.

Example first start (using the revision returned by the preceding status read):

```json
{"work_on":true,"work_mode":2,"set_temp":50,"expected_revision":12}
```

Example owner update and explicit BLE/REST handover payload:

```json
{"set_temp":52,"lease_id":"<32 hex>","expected_revision":13}
{"work_on":true,"takeover":true,"expected_revision":13}
```

`POST /api/v2/token` (also `/api/token`) changes the token with
`{"token":"<16–64 characters>"}`. Empty/missing tokens are rejected. The request
must authenticate with the current token and satisfy cold/idle maintenance rules.
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

SnapHeater can infer probable chamber/top-cover opening from a sudden chamber temperature drop in every mode, including printing, preheat, drying, manual heating, tempering and idle cooldown. This is advisory only: it never stops a job or sets a heater fault. Sensor, overtemperature and zero-cross protections remain independent.

Example command:

```json
{
  "virtual_door_detection_enabled": true,
  "virtual_door_window_sec": 60,
  "virtual_door_drop_c": 4,
  "virtual_door_rate_c_per_min": 4,
  "virtual_door_min_base_temp": 35,
  "virtual_door_action": 0
}
```

Action values:

- `0`: notify only,
- Legacy values `1` and `2` are normalized to notify-only, including saved settings.

Status fields include `virtual_door_open`, `virtual_door_open_pending`, `virtual_door_last_drop_c` and `virtual_door_last_rate_c_per_min`.

Android/local UI should show a notification when `virtual_door_open_pending=true`, then acknowledge with:

```json
{"virtual_door_ack": 61000}
```

Use the exact event timestamp from `virtual_door_detected_ms` (BLE: `vdoor_ms`) in this standalone authenticated/unlocked receipt. No control lease is required; an old receipt cannot clear a newer event. Compact BLE also includes `vdoor_enabled`, `vdoor`, `vdoor_pending` and `vdoor_drop`.

The Android app displays an alert and posts a system notification when allowed. Delivery requires receiving telemetry: there is no always-running background service. The latest unacknowledged event survives phone disconnection in Panda RAM, not Panda reboot; multiple events coalesce to the latest. Default thresholds remain a 4 °C drop over a 60-second window, at least 4 °C/min, with baseline at least 35 °C. A drop is not proof of an open door.

Legacy clients can clear the latched state with an ordinary authorized settings command:

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
{"safety_score_enabled":true,"ack_setup_warning":true}
```

The REST status response includes the corresponding runtime fields such as `warmup_eta_sec`, `heat_soak_ready`, `filter_life_pct`, `heater_wear_pct`, `airflow_warning_pending`, `print_risk_score`, `print_risk_message`, `safety_score`, and `safety_message`.
# Activation without manual arming

Submit the ordinary mode/work request using the existing lease and revision
contract. No separate arm request or operator verification flags are needed.
The firmware automatically applies runtime conditions before physical heating.
Legacy `arm_output_safety_latch` is inert; `disarm_output_safety_latch` is
unconditional OFF, including when mixed with start fields. A fault still rejects
new work until cleared; clearing never resumes a stopped session.
# Moonraker connection changes

Use the dedicated authenticated `printer_setup` settings command described in
[Moonraker setup](MOONRAKER_SETUP.md). Flat Moonraker configuration writes are
rejected; Panda tests the candidate before saving it and does not require reboot.
# Transport and persistence notes (firmware 0.9.9)

Control JSON must be a complete object (maximum nesting depth 16, body 2048
bytes). The receive loop has a total 2-second budget and 1-second socket waits.
Malformed, incomplete and unauthorized requests close the connection; clients
must not automatically retry energizing commands. OFF does not require a matching
revision. Other Android job/schedule/resume commands use the revision of the
state on which the user acted, not a later background response.

`settings_pending` means active RAM settings have not yet been acknowledged by
durable storage. `settings_persist_ok=false` means storage was unavailable or the
last attempt failed. Pending storage retries when cold and idle. This status does
not imply that the heating job will resume after a reboot.

OTA requests close their connection after the response, including rejection.
`inactive_upload_not_verified` rejects boot-inactive for a slot with a pending
upload marker. `ota_marker_failed` rejects an update when its persistent admission
record cannot be written. A successful full upload/validation clears that marker;
reboot and factory settings reset do not bypass it. SHA-256 is transfer integrity,
not image authenticity; signatures remain intentionally outside this design.
