# SnapHeater U1 v1.0 advanced chamber feature pack

This document describes the v1.0 feature pack added after the BLE/Android, preheat/hold, material profiles, persistence, Moonraker and tempering work.

The heater output is still disabled by default through `CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n`. All features are implemented as logic/API scaffolding first and must be tested after GPIO and sensor verification.

## 1. Material Chamber Assistant

SnapHeater can use `print_task_config.filament_type` from Snapmaker U1/Moonraker to detect the active material and apply a matching chamber profile when:

```json
{"auto_material_profile_enabled": true}
```

The runtime status exposes a human-readable `material_advice` field for Android/Web UI.

## 2. Anti-warp mode

User option:

```json
{"anti_warp_enabled": true}
```

Intended behavior:
- prefer stable chamber temperature for ABS/ASA/PETG large prints,
- keep chamber heat during printer pause/error instead of hard shutdown,
- combine well with tempering or finish conditioning.

## 3. Large Print Protection

User option:

```json
{"large_print_protection_enabled": true}
```

Current logic:
- extends fan post-run to at least 10 minutes,
- provides a place for future large-print stability heuristics,
- should be used with preheat/hold and stability score.

## 4. Chamber Stabilization Score

Firmware accumulates chamber statistics during an AUTO print context:

- min chamber temp,
- max chamber temp,
- sample count,
- percentage of samples within the stability band,
- `stability_score_pct`,
- `stability_report_pending` after print context ends.

Status fields are exposed via `/api/status` and BLE compact status as `stab`.

## 5. Smart Pause Hold

Printer pause/error is not treated as a local heater fault. User can choose what happens during a pause:

```json
{
  "pause_hold_enabled": true,
  "pause_hold_strategy": 0,
  "pause_hold_min": 60,
  "pause_lower_after_min": 30,
  "pause_lower_by_c": 5,
  "pause_stop_after_min": 180
}
```

Strategies:

```txt
0 = keep target temperature
1 = lower target after selected delay
2 = stop after selected delay
```

## 6. Chamber Dry-Out

Low/medium chamber warm-up with fan to dry the interior before a print:

```json
{"dryout_running":true,"dryout_target":45,"dryout_duration_min":20}
```

Completion sets `dryout_complete_pending=true` so Android can show a local notification.

## 7. Safe Overnight Mode

User option:

```json
{"safe_overnight_enabled": true}
```

Current logic:
- caps active target to 55°C,
- extends fan post-run to at least 15 minutes,
- keeps the existing rule that AUTO requires fresh Moonraker data.

## 8. Preheat Scheduler

Android may schedule a relative preheat start:

```json
{
  "scheduled_preheat_enabled": true,
  "scheduled_preheat_delay_min": 30,
  "scheduled_preheat_target": 55,
  "scheduled_preheat_hold_min": 20
}
```

This is intentionally local and simple. In a production Android app, the phone should also own the UI notification and confirmation flow.

## 9. Print Finish Conditioning

Selectable finish behavior:

```json
{"finish_conditioning_mode": 0}
```

Modes:

```txt
0 = fast cooldown / stop heat after print
1 = normal cooldown / stop heat + fan post-run
2 = tempering ramp, if tempering_enabled=true
3 = keep warm for selected time
```

Keep-warm fields:

```json
{"finish_conditioning_mode":3,"keep_warm_temp":35,"keep_warm_max_min":60}
```

## 10. Door / lid detection placeholder

A future hardware input can be exposed as:

```json
{"door_sensor_enabled": true}
```

The logic fields are present, but physical GPIO support must be added after PCB/hardware confirmation.

## 11. Heater Health Test

Short diagnostic cycle inspired by the vendor firmware traces from v1.0.1, but implemented as our own from-scratch logic:

```json
{"health_test_running":true,"health_test_target":45,"health_test_duration_sec":90}
```

The test checks whether PTC/chamber temperature rises enough during the test window.

Result codes:

```txt
0 = none
1 = OK
2 = weak heater / insufficient rise
3 = sensor fault
4 = aborted
```

Completion sets `health_test_complete_pending=true`.

## 12. Energy Estimate

Firmware estimates heater energy from heater ON time:

```txt
estimated Wh = heater_on_ms / 3600000 * 300 W
```

Exposed as:

```json
"estimated_energy_wh"
"session_energy_wh"
```

This is an estimate, not a calibrated power-meter reading.


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
