# SnapHeater U1 v0.7 smart heating rules

This document defines the corrected heating policy after user review.

## Mode priority

The firmware must not use one global rule such as “heat only when U1 is printing”.
Modes are layered:

1. **Preheat/Hold** — user-driven, independent of U1 print state and independent of bed temperature.
2. **Filament Drying** — user-driven, independent of U1 print state and independent of bed temperature.
3. **Manual / Power On** — user-driven direct chamber target hold.
4. **AUTO** — print-aware, synchronized with Snapmaker U1 through Moonraker.

## AUTO mode

AUTO may start heating only from a real print context. Bed temperature thresholds apply only to AUTO, not to Preheat/Drying/Manual.

When U1 is printing, SnapHeater may maintain the selected chamber target.

When U1 is paused or reports an error, SnapHeater does **not** hard-stop chamber heat just because of the printer state. Pause/error is a printer state, not necessarily a chamber-heater fault. Holding chamber temperature can reduce rapid material shrinkage.

Local heater faults still override everything:

- chamber sensor fault,
- PTC sensor fault,
- PTC overtemperature,
- chamber overtemperature,
- heater abnormal / no temperature rise.

## Print finish / tempering

After a print finishes, SnapHeater should not simply cut the chamber heater immediately. Instead it enters **tempering** when enabled:

- start from current/target chamber temperature,
- gradually lower the virtual chamber target to `tempering_end_temp`,
- `tempering_end_temp=0` means ramp to heater-off,
- ramp over the user-selected `tempering_duration_min`,
- then stop heating and leave fan post-run active.

This is intended to avoid rapid part shrinkage after high-temperature prints.

## Fan post-run

After heater output turns off, fan/filter output remains active for `fan_postrun_min` minutes.

Default: 5 min.

## User-facing settings

REST/BLE fields:

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
