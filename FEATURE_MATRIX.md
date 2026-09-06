# Feature status — 0.9.9

Implemented means present in the matching firmware/app, not hardware-certified.
All device-facing rows still need supervised real-hardware testing.

| Area | Included in 0.9.9 | Boundary |
|---|---|---|
| Original Panda electronics | SSR, held-gate fan, NTC, ZC, panel | V1.0/V1.0.1 assumptions; no AirGuard/DIY binary |
| Android control | BLE, REST, mode icons, saved heaters | Testing APK; Android 8+; no iOS release |
| Setup | Panda Wi-Fi + printer wizard, optional API key | Phone-assisted discovery; no automatic IP rediscovery |
| Local jobs | Preheat/hold, AUTO, manual hold, drying | Accepted jobs continue without phone |
| Tempering | AUTO finish or standalone ramp | No guarantee of reaching a temperature in a fixed time |
| Pause / Stop | Retain task / cancel task | Fault clear never restarts heating |
| Symbiont | Supported exhaust and optional top-cover supervision | No printer installation; network enforcement is not instantaneous |
| History | Charts, 720-sample Panda ring, catch-up, CSV | 10-second interval; RAM ring is lost on Panda reboot |
| Events | Timestamped phone history, advisory queue | Background delivery not guaranteed |
| Usage | Panda NVS heater/filter counters | Unsaved current work can be lost after power failure |
| OTA | Inactive slot, admission checks, image/identity/SHA-256 | LAN upload, no signatures; installed bootloader needs qualification |
| Safety | Local fault cutoffs, OFF priority, ZC latch, watchdog checks | Does not detect every hardware fault or verify physical airflow |

Removed: Anti-Warp, Large Print Protection, Safe Overnight, recipes/Showcase
metadata and legacy Smart Resume. User pause/resume is a different feature.

See [current implementation](docs/CURRENT_IMPLEMENTATION.md) and
[known safety limits](docs/SAFETY_STATUS.md).
