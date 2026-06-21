# Tempering is a user/app option

Tempering is not a hard firmware rule. It is a user-controlled feature intended to be enabled or disabled from the Android app, BLE, or REST API.

## Behavior

- `tempering_enabled=false` is the factory default.
- If a print finishes in AUTO mode and `tempering_enabled=false`, SnapHeater stops chamber heating and runs the configured fan post-run/cooldown.
- If a print finishes in AUTO mode and `tempering_enabled=true`, SnapHeater starts a gradual target reduction from the current chamber target toward `tempering_end_temp` over the Android/user-selected `tempering_duration_min`. `tempering_end_temp=0` means ramp to heater-off, then disable heating completely.
- The app can disable it at any time by writing `{"tempering_enabled":false}`.
- The app can cancel an active ramp by writing `{"cancel_tempering":true}`.

## Android BLE examples

Enable tempering before or during a print:

```json
{"tempering_enabled":true,"tempering_duration_min":30,"tempering_end_temp":0}
```

Disable tempering:

```json
{"tempering_enabled":false}
```

Cancel active tempering:

```json
{"cancel_tempering":true}
```

Acknowledge completion notification:

```json
{"ack_tempering_complete":true}
```

## Design note

This separation is intentional: print-aware AUTO mode decides when a print has ended, but the user's Android/app preference decides whether controlled post-print tempering should run.


## v0.9.0 duration picker behavior

The Android app should treat Tempering as a two-step setting:

1. User enables Tempering.
2. App opens a minute picker and writes the selected duration to firmware.

For example, if the final print/chamber temperature is 60°C and the user chooses 30 minutes, firmware calculates a linear ramp from 60°C to the configured end target. With the default `tempering_end_temp=0`, this means a controlled ramp to heater-off over 30 minutes.

The firmware does **not** require the app to know the start temperature. It captures the start temperature automatically when AUTO print completion is detected.
