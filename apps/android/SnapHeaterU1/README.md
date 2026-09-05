# SnapHeater U1 Android App

Android/iOS-oriented companion app shell for SnapHeater U1.

Current status: SnapScreen-like UI connected to the SnapHeater BLE/REST contract.
The firmware target is original Panda Breath electronics using the recovered
DragonBreath-derived hardware layer.

## Scope

- Scan and connect to devices running SnapHeater firmware.
- Use the SnapHeater BLE and authenticated REST command contracts.
- Dashboard for chamber, target, heater/fan and connectivity status exposed by SnapHeater firmware.
- SnapHeater workflows guarded by firmware leases, revisions and safety interlocks.
- Snapmaker U1 / Moonraker-aware automation from the app layer, read-only first.
- Diagnostics view for connection, Panda hardware mapping and app-side events.
- Settings view for material, temperature limits, tempering duration and local-only mode.
- EN primary UI direction with PL and DE localization resources started.

## Project Structure

- `app/src/main/java/.../model`: UI state models.
- `app/src/main/java/.../data`: app repositories for BLE and REST sources; no demo repository.
- `app/src/main/java/.../ble`: BLE scanner/client for the SnapHeater service.
- `app/src/main/java/.../ui`: Compose app shell, screens, components and theme.
- `docs/UI_FLOW.md`: mobile UX flow and safety boundary.
- `docs/FIRMWARE_APP_MAP.md`: mapping between firmware features and Android UI coverage.

## Open In Android Studio

Open this folder:

```text
apps/android/SnapHeaterU1
```

Then let Android Studio sync Gradle.

Build setup notes are in [docs/BUILD_SETUP.md](docs/BUILD_SETUP.md).

## Safety Boundary

Physical output actions remain hidden or disabled until the SnapHeater firmware
reports valid sensors, a verified zero-cross/fan path and an explicitly armed
Output Safety Latch. The app cannot override firmware interlocks.
