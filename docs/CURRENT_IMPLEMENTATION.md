# Current implementation — 2026-09-06

This source snapshot targets original Panda Breath hardware. Use matching
Android and firmware versions. Credit to plastikman / DragonBreath remains in
AUTHORS.md and THIRD_PARTY_NOTICES.md; no upstream approval is implied.

## Implemented and offline checked

- Localized Android controls, per-mode icons, saved heaters and editable names.
- Separate visual preview without physical transport/control.
- BLE Wi-Fi provisioning with scan, connection deadline and explicit errors.
- Phone-assisted printer discovery; Panda executes its own Moonraker client.
- Printer-dependent AUTO and AUTO + Tempering; standalone manual tempering.
- Shared BLE/REST job commands; preferences separate from activation.
- Preheat/hold, exact-minute drying, scheduler delay/target/hold and cancellation.
- REST token provisioning over BLE, encrypted local credential storage and
  read-only access verification.
- Accepted jobs continue after phone disconnection; OFF and fault interlocks
  remain authoritative. No automatic heating restart after a reboot.
- Virtual-door detection is advisory in all modes. The latest pending event
  survives phone disconnection in RAM, not Panda reboot.
- Firmware inactive-slot OTA exists with validation and maintenance exclusion.

Verification: 58 Python tests, Kotlin command generation, both production parser
sections, the real-mutex safety host test, Android APK build and ESP-IDF tester
build passed. These are not real radio, waveform or hardware qualification.

## Partial or deferred

- Anti-Warp and Large Print Protection affect advisory risk calculations rather
  than implementing a separate heating strategy.
- Smart Resume records a recovery window without a complete control strategy.
- Safe Overnight has no distinct stricter timeout policy; its 55 C ceiling
  duplicates the current general target ceiling.
- Symbiont ventilation writes are not implemented.
- Recipes and remaining Showcase metadata are not complete features.
- Android history/report viewing/export and OTA updater remain incomplete.
- Filter-life and heater-wear runtime history do not persist across reboot.
- Not all firmware events have Android notifications; no guaranteed background
  receiver. Notifications require telemetry reception.
- Panda does not rediscover a printer after IP changes; changed Moonraker
  configuration requires restart. Moonraker API-key setup is absent.
- Plausible frozen NTC readings near target remain an unresolved diagnostic gap.
- Installed bootloader compatibility, interrupted OTA/stock recovery, actual
  TRIAC/SSR waveforms, reset/watchdog and loss-of-cooling behavior require
  device-specific qualification.

Older feature matrices and UI maps are historical plans, not evidence that all
listed functions are complete. No private working archives, credentials, local
build products or new firmware/APK binaries are included in this update.
