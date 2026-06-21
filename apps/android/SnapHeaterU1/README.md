# SnapHeater U1 Android App

Android companion app shell for SnapHeater U1.

Current status: SnapScreen-like UI prototype with mock firmware data. BLE integration will be added after hardware validation confirms the final GATT contract and payload details.

## Scope

- Dashboard for chamber, PTC, target, heater/fan and connectivity status.
- Mode controls for Auto, Manual Hold, Preheat, Drying, Tempering and Safe Stop.
- Safety setup view for staged unlock validation.
- Diagnostics view for event log and sensor/API status.
- Settings view for material, temperature limit, tempering duration and local-only mode.
- EN primary UI direction with PL and DE localization resources started.

## Project Structure

- `app/src/main/java/.../model`: UI state models.
- `app/src/main/java/.../data`: mock repository now, BLE-backed repository later.
- `app/src/main/java/.../ble`: provisional BLE identifiers and integration boundary.
- `app/src/main/java/.../ui`: Compose app shell, screens, components and theme.
- `docs/UI_FLOW.md`: mobile UX flow and safety boundary.

## Open In Android Studio

Open this folder:

```text
apps/android/SnapHeaterU1
```

Then let Android Studio sync Gradle.

Build setup notes are in [docs/BUILD_SETUP.md](docs/BUILD_SETUP.md).

## Safety Boundary

The UI currently uses mock data. It must not be treated as proof that heater, fan, BLE or Moonraker control is safe on real hardware.
