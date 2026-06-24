# Firmware to Android app map

This document maps the current SnapHeater U1 firmware feature surface to the Android app shell.

Status labels:

- `UI shell`: screen/control exists with mock state.
- `Pending BLE`: needs real BLE scan/connect/status/control.
- `Pending hardware`: needs validated Panda Breath-style or DIY heater hardware.

| Firmware area | Android location | Status |
|---|---|---|
| BLE scan/connect/unlock | Connect screen | UI shell, Pending BLE |
| Live chamber/PTC/target/heater/fan status | Status tab | UI shell, Pending BLE |
| Auto, Manual Hold, Preheat, Drying, Tempering, Safe Stop | Modes tab | UI shell, Pending BLE |
| Mode-specific target, duration and helper settings | Modes tab | UI shell, Pending BLE |
| First Setup / commissioning checklist | Safety tab | UI shell, Pending hardware |
| Output Safety Latch readiness and arming boundary | Safety tab | UI shell, Pending hardware |
| GPIO probe status | Safety tab | UI shell, Pending hardware |
| Material profile assistant | Settings tab | UI shell, Pending BLE |
| Material mismatch warning | Settings tab | UI shell, Pending BLE |
| PLA protection | Settings tab | UI shell, Pending BLE |
| Anti-warp and large-print protection | Settings tab | UI shell, Pending BLE |
| Heat soak and chamber stability lock | Status + Settings tabs | UI shell, Pending BLE |
| Safe Overnight Mode | Settings tab | UI shell, Pending BLE |
| Pause Hold and Smart Resume | Settings tab | UI shell, Pending BLE |
| Preheat Scheduler | Settings tab | UI shell, Pending BLE |
| Virtual Door / Open Lid Detection | Status + Settings tabs | UI shell, Pending BLE |
| Airflow / blocked filter detection | Diagnostics + Settings tabs | UI shell, Pending hardware |
| Filter life and heater wear | Status + Diagnostics tabs | UI shell, Pending hardware |
| Temperature history | Diagnostics + Settings tabs | UI shell, Pending BLE |
| Incident report / fault snapshot | Diagnostics + Settings tabs | UI shell, Pending BLE |
| Event log | Diagnostics tab | UI shell, Pending BLE |
| Energy estimate | Status tab | UI shell, Pending BLE |
| Local recipes | Settings tab | UI shell, Pending BLE |
| Export/import settings | Planned in Settings/Diagnostics flow | UI shell placeholder |
| Demo / Showcase Mode | Settings tab | UI shell |
| Local-only Mode | Settings tab | UI shell |
| U1 Symbiont Mode | Settings tab | UI shell, Pending U1 test |
| OTA / rollback placeholder | Diagnostics tab | UI shell placeholder |

## Current boundary

The Android app now has a UI shell for the firmware feature set. It still uses mock state. It must not be treated as a real controller until BLE scan/connect, status parsing, control writes, safety acknowledgements and hardware validation are implemented and tested.

## Next integration phase

1. Replace mock state with a BLE-backed repository.
2. Parse compact BLE status into `HeaterSnapshot`.
3. Convert UI changes into JSON writes for the BLE Control characteristic.
4. Add notification handling for pending events.
5. Gate safety-sensitive writes behind explicit confirmation dialogs.
