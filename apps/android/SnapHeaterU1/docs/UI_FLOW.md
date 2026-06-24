# SnapHeater U1 Mobile UI Flow

The mobile app follows a SnapScreen-like control layout: direct status first, bottom navigation for primary areas, compact operational panels, and no cloud dependency.

The visual palette is intentionally restrained: black background, white text, neutral dark panels and orange heat/control accents.

Status and temperature colors remain semantic: green means OK, red means fault/danger, white means normal and orange means warning or action.

## Primary Navigation

- Connect: search for a SnapHeater U1 device or enter Demo mode.
- Status: chamber state, target, PTC temperature, fan, BLE and Moonraker status.
- Modes: Auto standby, Manual hold, Preheat, Drying, Tempering and Safe stop.
- Safety: checklist mirror for staged firmware and hardware validation.
- Diagnostics: BLE contract identifiers, mock firmware channel and event log.
- Settings: target temperature, material profile and local-only preferences.

## Language Plan

English is the primary UI language. Polish and German string resources are included so the app can move toward EN/PL/DE without changing the project structure later.

## Safety Boundary

The app shell uses mock data. It does not unlock heater output, does not prove BLE behavior and does not replace the firmware-side safety gates.
