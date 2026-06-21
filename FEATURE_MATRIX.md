# SnapHeater U1 Feature Matrix

This matrix tracks the project skeleton status.  It is intentionally honest: a
feature can already exist in the firmware architecture/API while still needing
build fixes, PCB verification, Android implementation or real printer testing.

Current baseline: the project builds for ESP32-C3 with ESP-IDF v5.3.5. The
remaining "Needs hardware" and "Needs U1 test" entries still require physical
devices.

Legend:

- **Skeleton**: code/API/configuration exists as a development scaffold.
- **Build verified**: compiled successfully in ESP-IDF.
- **Needs hardware**: requires Panda Breath PCB / real heater tests.
- **Needs U1 test**: requires a real Snapmaker U1 Moonraker session.
- **Needs Android**: firmware/API exists but the phone app still has to be built.

| Feature | Firmware skeleton | BLE/API | Build verified | Needs hardware | Needs U1 test | Needs Android | Notes |
|---|---:|---:|---:|---:|---:|---:|---|
| ESP32-C3 / ESP-IDF project base | Yes | N/A | Yes | No | No | No | From-scratch project skeleton. |
| Panda Breath board target | Yes | Status | Yes | Yes | No | No | Core pinout inferred; physical verification required. |
| DIY reference hardware path | Docs | N/A | N/A | Yes | No | Optional | BOM, wiring diagram and validation checklist for low-voltage DC DIY builds. |
| Heater output control | Yes | Yes | Yes | Yes | No | Optional | Locked by `CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n` and Output Safety Latch. |
| Fan / filter output control | Yes | Yes | Yes | Yes | No | Optional | Also used for cooldown/post-run. |
| Chamber NTC reading | Yes | Status | Yes | Yes | No | No | ADC calibration still required. |
| PTC NTC reading | Yes | Status | Yes | Yes | No | No | Required for safety and heater health detection. |
| GPIO Probe mode | Yes | REST/BLE hooks | Yes | Yes | No | Optional | Short controlled pulses only; no normal heating automation. |
| Physical buttons | Yes | Status | Yes | Yes | No | No | AUTO/ON/OFF/generic placeholders. |
| Indicator/backlight LEDs | Yes | Status | Yes | Yes | No | No | GPIOs are placeholders `-1` until known. |
| Wi-Fi STA | Yes | Config | Yes | No | No | Optional | Credentials can be provisioned later. |
| Local REST API | Yes | REST | Yes | No | No | No | Status/settings/events/probe foundation. |
| BLE GATT control | Yes | BLE | Yes | No | No | Yes | Android app not yet implemented. |
| BLE PIN unlock | Yes | BLE | Yes | No | No | Yes | App-level lock; not final production pairing. |
| Snapmaker U1 Moonraker read integration | Yes | Status | Yes | No | Yes | Optional | Uses U1-specific fields from coroNET reference. |
| U1 Symbiont Mode | Yes | Settings | Yes | Depends | Yes | Yes | Cooperative mode; optional safe climate/ventilation writes later. |
| Safe Moonraker write whitelist | Placeholder | Settings | Yes | No | Yes | Yes | Printer-critical actions remain blocked. |
| Preheat / Hold | Yes | BLE/REST | Yes | Yes | No | Yes | Timer starts after target reached. |
| Heat Soak | Yes | BLE/REST | Yes | Yes | No | Yes | Requires stable temperature behavior for validation. |
| Chamber Stability Lock | Yes | BLE/REST | Yes | Yes | No | Yes | Readiness requires stable temp window. |
| Manual / Power-On mode | Yes | BLE/REST | Yes | Yes | No | Yes | Still protected by safety latch. |
| Filament Drying | Yes | BLE/REST | Yes | Yes | No | Yes | PLA/PETG/ABS/custom profiles. |
| Chamber Dry-Out | Yes | BLE/REST | Yes | Yes | No | Yes | Low/medium chamber drying cycle. |
| Tempering | Yes | BLE/REST | Yes | Yes | Yes | Yes | User-selected duration; ramp to target or heater off. |
| Print Finish Conditioning | Yes | BLE/REST | Yes | Yes | Yes | Yes | Cooldown/tempering/keep warm after print. |
| Keep Warm after Print | Yes | BLE/REST | Yes | Yes | Yes | Yes | Can stop after virtual door/open event. |
| Virtual Door/Open Lid Detection | Yes | BLE/REST | Yes | Yes | Yes | Yes | Needs real U1 temperature-drop calibration. |
| Material Chamber Assistant | Yes | BLE/REST | Yes | No | Yes | Yes | Uses profile/material state. |
| Material/Profile Mismatch Warning | Yes | BLE/REST | Yes | No | Yes | Yes | Warning only; does not stop heating. |
| PLA Protection Mode | Yes | BLE/REST | Yes | No | Yes | Yes | Warn/confirm for high chamber target with PLA. |
| Anti-Warp Mode | Yes | BLE/REST | Yes | Yes | Yes | Yes | Preheat + stable chamber + optional tempering. |
| Large Print Protection | Yes | BLE/REST | Yes | Yes | Yes | Yes | Long-print conservative behavior and reporting. |
| Smart Pause Hold | Yes | BLE/REST | Yes | Yes | Yes | Yes | Pause does not hard-stop chamber heat. |
| Smart Resume After Pause | Yes | BLE/REST | Yes | Yes | Yes | Yes | Recovery after long pause. |
| Safe Overnight Mode | Yes | BLE/REST | Yes | Yes | Yes | Yes | Stricter timeouts and critical warnings. |
| Preheat Scheduler | Yes | BLE/REST | Yes | Yes | Optional | Yes | Local scheduler; safety limits required. |
| Start Print Warning | Yes | Status event | Yes | No | Yes | Yes | Warns if print starts before chamber ready. |
| Print Risk Score | Yes | Status | Yes | No | Yes | Yes | Material/profile/settings advisory. |
| Chamber Warm-Up Prediction | Yes | Status | Yes | Yes | Optional | Yes | Learns from previous warm-up rate. |
| Chamber Stabilization Score | Yes | Status | Yes | Yes | Optional | Yes | Summary score after print/session. |
| Temperature History Buffer | Yes | REST/status | Yes | No | No | Yes | RAM buffer for charts/reports. |
| Event Log | Yes | REST | Yes | No | No | Optional | In-RAM event timeline. |
| Incident Report / Fault Snapshot | Yes | REST/status | Yes | Yes | Optional | Yes | Diagnostic package after critical faults. |
| Heater Health Test | Yes | BLE/REST | Yes | Yes | No | Yes | Based on temperature rise behavior. |
| Heater Wear / Aging Trend | Yes | Status | Yes | Yes | No | Yes | Uses future health-test history. |
| Airflow / Blocked Filter Detection | Yes | Status event | Yes | Yes | No | Yes | PTC rises fast while chamber rises slowly. |
| Filter Life Counter | Yes | Status | Yes | Yes | No | Yes | Runtime-based filter maintenance. |
| Energy Estimate | Yes | Status | Yes | Yes | No | Yes | Uses heater on-time and nominal wattage. |
| Local Recipes | Yes | BLE/REST | Yes | No | Optional | Yes | Recipe metadata and selection. |
| Export / Import Settings | Yes | REST/BLE concept | Yes | No | No | Yes | JSON settings exchange. |
| Firmware Demo Mode | Yes | Settings | Yes | No | No | Optional | Useful for contest video and app testing. |
| Contest / Showcase Mode | Yes | Settings | Yes | No | No | Optional | Guided simulation scenes. |
| First Setup Wizard | Yes | BLE/REST state | Yes | Yes | Yes | Yes | Commissioning checklist. |
| Output Safety Latch | Yes | Internal/API | Yes | Yes | No | Yes | Runtime lock even if heater output compile flag is enabled. |
| Safety Score / Setup Validation | Yes | Status | Yes | Yes | Yes | Yes | Shows what is verified vs unsafe. |
| Notification Levels | Yes | Event codes | Yes | No | No | Yes | info/warning/critical/action_required. |
| Language/Event Codes | Yes | Event codes | Yes | No | No | Yes | App translates local strings. |
| Local-only Mode | Yes | Setting | Yes | No | No | Yes | Local BLE/Wi-Fi/Moonraker operation. |
| OTA / Rollback placeholders | Placeholder | Status | Yes | No | No | Yes | Future safe update flow. |
| PROJECT_HIGHLIGHTS.md | Yes | N/A | No | No | No | No | GitHub-facing project differentiators. |

## Current project boundary

The Android app is intentionally out of scope for the next firmware-only step.
The firmware already exposes the data and commands that the app will later use.

## Next engineering phase

1. Keep the public repository synchronized with the build-verified skeleton.
2. Flash with heater output disabled only after hardware arrives and backup is verified.
3. Validate Wi-Fi, REST, BLE status, event log and settings storage.
4. Use GPIO Probe only after reviewing the board and power path.
5. Enable normal heater output only after sensor, fan, polarity and safety-latch validation.
