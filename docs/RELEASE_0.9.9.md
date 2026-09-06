# SnapHeater U1 0.9.9 — Your chamber heater. Connected to your print.

**Experimental testing prerelease — not stable, certified or hardware-qualified.**
Heater/fan support is enabled; heating still requires normal deliberate activation
and passing local checks. Supervise operation, test at your own risk and report
abnormalities. Provided without warranty; liability is disclaimed to the extent
permitted by applicable law. Read the [risk notice](HARDWARE_LIABILITY_DISCLAIMER.md).

## One workflow, from chamber preparation to print completion

Original Panda Breath hardware gains a native Android companion, direct
Snapmaker U1/Moonraker integration, task-oriented heating modes and local execution
that continues after the phone disconnects. Preheat, hold, dry filament, pause or
stop, and optionally follow a print with a timed tempering ramp.

This release brings together the Wi-Fi/printer wizard, multiple saved heaters,
temperature charts, event history, BLE/REST history catch-up, CSV/report export,
Panda usage counters and optional Symbiont chamber/top-cover ventilation supervision.
No printer-side installation is required by this integration.

## Safety and correctness work included

- OFF wins over stale replies and old print-completion/Keep Warm state.
- Local sensor/overheat/ZC cutoffs, watchdog admission and interrupted-job handling.
- SSR cutoff precedes persistence; the safety path avoids blocking log output.
- Bounded REST reception including headers; no unbounded leftover-body drain.
- Whole BLE frame validation, bounded JSON nesting and shared command arbitration.
- OTA exclusion during work/cooling; inactive-slot writes, image checks and
  SHA-256 verification before acceptance, with persistent pending-slot markers.

No cryptographic firmware signatures are enforced. Notifications are best effort.
The fan command/ZC signal is not measured airflow. Reaching 55 °C depends on the
installation; it is a target ceiling, not a guaranteed result.

## Downloads and installation

Use **SnapHeater-U1-0.9.9-ota.bin** for the application updater, not a full-flash
write. Install **SnapHeater-U1-Android-0.9.9-debug.apk** for the matching Android
testing companion. The ZIP combines both with instructions and checksums.
[Follow the installation guide](INSTALL_0.9.9.md).

Public numbering restarts at **0.9.9** for the first packaged prerelease;
historical 1.x source labels do not denote a later stable release.
Android version code is **4**, version name **0.9.9**. The corrected public APK
removes the disconnected-device preview option; preview is a local-only developer
build. APK/ZIP/checksums were refreshed; firmware 0.9.9 is unchanged.

Offline verification: 107 Python tests (including 62 control-loop scenarios),
25 Android unit tests, the real-mutex OFF/maintenance safety test and ESP32-C3
build. These are software checks, not electrical or end-to-end device qualification.

## A community-powered revival

SnapHeater's Panda recovery was made possible by
[plastikman and DragonBreath](https://github.com/plastikman/DragonBreath), whose
published hardware and thermal discoveries provided the essential foundation.
SnapHeater retains its own Android/U1 identity; no upstream or manufacturer
endorsement is implied. Thank you to everyone helping turn findings into
reproducible, supervised test results.

Read the [project story](PROJECT_STORY.md), [known limits](SAFETY_STATUS.md) and
[contribution guide](../CONTRIBUTING.md). Real-device gallery screenshots will
be added when supplied; no mockup is presented as a successful hardware test.
