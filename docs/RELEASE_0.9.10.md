# SnapHeater U1 0.9.10 — control corrections and clearer sensor warnings

**Experimental prerelease for supervised testing. Not stable, certified or
hardware-qualified.** No energized Panda validation of this release is claimed.
Heater/fan support is enabled but heating requires deliberate activation and
local safety checks. Continuously supervise operation, report abnormalities and
read the [risk notice](HARDWARE_LIABILITY_DISCLAIMER.md).

## What changed from 0.9.9

- Local 1 C chamber hysteresis replaces the older PID/10-second SSR window,
  following DragonBreath's current default policy. The 55 C target ceiling,
  thermal cutoffs, board-specific PTC foldback and held-gate fan/ZC path remain.
- Airflow warm-up observation survives normal SSR OFF/foldback intervals. It
  remains a heuristic warning, not measured airflow or a separate fan-failure cutoff.
- Frozen raw readings are diagnosed independently per sensor, including during
  continuous heating. Activity in another channel cannot erase the warning.
  The fixed five-minute grace remains; hard temperature/sensor/ZC faults do not wait.
- Android names the chamber sensor, heater-element sensor or both in warnings,
  notifications and history. Older firmware with no identity field is explicitly
  unspecified. New read-only telemetry is sent over both BLE and REST.
- Updated safety/probe documentation and attribution to plastikman/DragonBreath.

## Download the matching files

- **SnapHeater-U1-0.9.10-ota.bin** — application-only firmware, not a full flash image.
- **SnapHeater-U1-Android-0.9.10-debug.apk** — Android 8+ testing app, version code 6;
  no disconnected-device preview. Uses the existing testing signing identity.
- **SnapHeater-U1-0.9.10-tester.zip** — both files, installation/recovery instructions,
  risk notices, licenses, manifest and checksums.
- **SHA256SUMS.txt** and **manifest.json** — integrity and source/build provenance.

[Installation, OTA and return-to-stock instructions](INSTALL_0.9.10.md).
Install the APK in place; do not uninstall and lose history/settings. Then select
the downloaded BIN in Settings → Firmware update (OTA), connected to the correct
Panda over LAN. Stop all work and let cooling finish; both sensors must be below
30 C. Never bypass admission checks. The application does not auto-download OTA.
**Never write this BIN at 0x0 or use generic full-flash commands.**

This is a new release; previous 0.9.9 assets are left unchanged. The firmware and
APK now both report 0.9.10, removing ambiguity with earlier local test builds.

## Verification and remaining limits

Software verification: 114 host tests including 67 control-loop scenarios, 28
Android unit tests, real-mutex OFF/maintenance/SSR-before-NVS test, and ESP32-C3 /
Android builds. These checks are not electrical or thermal qualification.
Real fan airflow, TRIAC/SSR/reset/watchdog behavior and interrupted OTA/stock
bootloader recovery remain device-specific validation requirements. A failed-short
power component cannot be switched off by software. Plausible noisy or incorrect
sensor readings can evade diagnostics. Notifications are best effort.

App-only OTA retains the installed bootloader; application rollback build flags
do not prove that stock bootloader recovery works. No private dumps, NVS,
credentials, bootloader replacements or partition-table replacements are shipped.
No cryptographic firmware signatures are enforced. SHA-256 verifies integrity,
not publisher identity. Provided without warranty; liability is disclaimed to the
extent permitted by applicable law.

Thanks to [plastikman / DragonBreath](https://github.com/plastikman/DragonBreath)
for the hardware discoveries that made this project revival possible. SnapHeater
retains its own Android and Snapmaker U1 integration; no manufacturer or upstream
endorsement is implied. See [control changes](CONTROL_SAFETY_UPDATE.md) and
[safety status](SAFETY_STATUS.md).
