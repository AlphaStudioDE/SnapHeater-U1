# Android / firmware control contract

Use matching Android and firmware sources from this tree. Offline verification
is not physical qualification.

- REST and BLE use the same Kotlin job payload builder. Explicit work_on and
  work_mode replace an old job; OFF/emergency/disarm handling still runs first.
- Preheat sends preheat_running, preheat_target and preheat_hold_min.
- Drying selects custom mode, sends isrunning, custom_temp and exact
  drying_duration_min. Existing global session limits remain authoritative.
- Standalone Tempering sends tempering_start_now with an explicit activation.
  It uses the manual controller without a printer, ramps from the selected
  target down to 35 C over the chosen duration, then stops. It never ramps
  upward when the starting target is below 35 C. This is not refrigeration.
- AUTO + Tempering still waits for printer completion.
- Advanced settings are configuration-only requests without start flags.
  Saving while OFF cannot request heating; saving during a job does not reset
  job timers. The editor keeps its draft separate from polling.
- Scheduling has separate confirm/cancel actions: delay 1–1440 minutes, target
  30–55 C and hold after reaching target 1–240 minutes. The app requires the
  current job to be stopped first. BLE status includes settings readback.

## LAN credentials

The app no longer uses token "web". While connected by BLE and with all jobs
stopped, advanced settings can provision a unique 16–64-character token using
the existing unlocked rest_token command. Retain a copy for another phone/IP.
Panda stores the token in NVS. Android stores its copy encrypted with an Android
Keystore AES-GCM key, associated with the Panda address.

GET /api/v2/auth validates X-DragonBreath-Auth without changing device state.
LAN connection setup verifies access before reporting success. A bad entered
token does not overwrite a previously saved valid credential. Token setup uses
the idle networking gate, not OTA's cold-temperature gate; it starts no outputs.
This does not add TLS or replace the development BLE PIN/bonding design. Use a
trusted LAN; Android key loss/restoration can require token re-entry.

## Tests

Run Android gradlew testDebugUnitTest before Python unittest discovery. Kotlin
tests execute the real payload builders and export app/build/control-fixtures.
test_app_firmware_contract.py feeds these JSONs through both production parser
sections, checking mode replacement, AUTO, preheat, exact drying time, standalone
ramp completion, OFF-state preferences and schedule/cancel. Existing OFF,
arbitration, maintenance and thermal-fault tests cover the surrounding policy.
No radio timing, installed-bootloader or energized-hardware validation is claimed.
