# Safety and Security Policy

SnapHeater U1 controls heater-related logic and must be treated as safety-relevant firmware.

## Default safety position

- Physical heater output is disabled by default.
- Heater control requires compile-time enablement and runtime safety validation.
- Output Safety Latch must confirm board setup before live heater control.
- Unknown GPIOs must remain disabled until verified on real hardware.

## Moonraker cooperation model

SnapHeater U1 uses **U1 Symbiont Mode** for safe cooperation with Snapmaker U1.

Printer-critical actions remain blocked by design, including print cancel, print pause, motion, extrusion, nozzle temperature and bed temperature changes.

Optional chamber-related cooperation may be enabled only for safe, whitelisted climate/ventilation behavior when the user explicitly allows it.

## Reporting issues

When reporting a safety issue, include:

- firmware version,
- hardware revision if known,
- board pin mapping used,
- heater/fan output configuration,
- sensor readings,
- event log or incident report,
- steps to reproduce.
