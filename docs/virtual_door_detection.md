# Virtual Door / Open Lid Detection

SnapHeater U1 v1.1 adds a software-only chamber-open detector. It does **not** require a physical door sensor. Instead, it watches the chamber temperature trend after a print has finished, especially during tempering, keep-warm or post-run cooldown.

The initial default rule is deliberately conservative and user-calibratable:

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

Meaning: if the chamber temperature drops by at least 4 °C within a 60-second window, and the calculated drop rate is at least 4 °C/min, while the chamber was at least 35 °C at the start of the window, SnapHeater marks a probable chamber/top-cover opening.

## Actions

`virtual_door_action` values:

- `0` = notify only, do not change conditioning.
- `1` = stop post-print conditioning: cancel tempering/keep-warm and stop heating. This is the default.
- `2` = stop heater/conditioning. Reserved for stronger safety behavior; currently handled like action 1.

After detection the status exposes:

```json
{
  "virtual_door_open": true,
  "virtual_door_open_pending": true,
  "virtual_door_last_drop_c": 6.2,
  "virtual_door_last_rate_c_per_min": 6.2
}
```

Android should show a local notification when `virtual_door_open_pending=true`.

Acknowledge the notification:

```json
{"ack_virtual_door_open":true}
```

Clear the latched state:

```json
{"clear_virtual_door_open":true}
```

## Calibration plan

The thresholds are placeholders until real Snapmaker U1 tests are collected. During live testing, record chamber temperature every 1-2 seconds for scenarios like:

- print finished, door closed, natural cooldown,
- print finished, door opened fully,
- print finished, top cover opened,
- small short opening,
- cold room vs warm room.

The measured drop curves should replace the defaults so the detector becomes close to practical door-open inference without extra hardware.
