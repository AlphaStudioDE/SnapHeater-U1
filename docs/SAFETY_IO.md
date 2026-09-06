# Safety-path I/O isolation — 2026-09-06

This change removes identified blocking application I/O, not all possible
ESP32 scheduling, interrupt, flash-cache or hardware latency. It is not hardware
qualification and does not establish a maximum physical OFF response time.

## Output and diagnostic paths

- Runtime heater set, force-off and cut-power functions do not log. SSR OFF is
  applied before fan/output bookkeeping. Boot-only driver diagnostics remain.
- ESP logging is compiled out of the safety, ADC, physical-control and BLE
  modules, including paths executed while the thermal-policy guard is held.
- The RAM event writer takes its mutex with zero wait and uses bounded string
  copies. Contention drops an advisory event instead of blocking thermal control.
  Fault state and output inhibition do not depend on event delivery.
- Watchdog reset failure cuts SSR and sets the non-clearable runtime inhibit
  instead of taking a log/abort path with an energized output.

## Storage

- Ordinary BLE/REST settings commands enqueue a bounded latest-value snapshot.
  They affect RAM immediately. A low-priority worker saves only during existing
  idle checkpoint admission (valid fresh safety temperatures below 45 C,
  no active/paused/scheduled job, outputs off including fan/purge, no fault latch).
  OTA retains its separate below-30 C admission. A 35 C idle device can save
  settings without allowing an OTA upload at that temperature.
- The worker releases the thermal-policy guard before NVS. It only acknowledges
  its saved revision afterward, never replays an old job into live state.
- A failed settings write remains pending and is retried. REST and BLE status
  expose `settings_pending` and `settings_persist_ok`; these are diagnostic fields,
  not yet a dedicated Android save-progress indicator. RAM settings can be lost
  on power removal before successful persistence, including throughout a job.
- Hazardous trips cut SSR and latch in RAM before queuing fault persistence.
  Persistence and explicit-clear storage run in the worker, outside the policy
  guard. Clear checks current sensor/state validity before and after storage;
  a newer fault generation or runtime inhibit defeats an older clear.
- A failed clear keeps the latch. Neither storage completion nor clear starts
  heating. Boot still requires a new heating command.
- The session journal writes a durable dirty marker BEFORE SSR admission. It is
  cleared only at safe idle after the session. Boot with a dirty marker latches
  heating off until an explicit safe fault clear; an ordinary power cut during
  heating therefore also requires that clear. This is automatic bookkeeping,
  not a separate arming step for normal starts. Failed marker writes inhibit heat.
- Power loss before an asynchronous fault write completes can lose the newest
  persistent fault record. If validity changes during a clear write, RAM remains
  latched and persistence is repaired. The dirty session marker preserves the
  need for a clear across power loss during an unclosed heating session, without
  depending on the exact fault record. It does not guarantee retention of the
  exact reason, nor atomic recording of every new fault arising during idle
  marker cleanup. Physical SSR cut does not wait for a fault write.
- Calibration, REST-token provisioning and factory-reset flash transactions
  release the policy guard under maintenance exclusion. Legacy flat Wi-Fi fields
  are rejected; use the existing dedicated BLE Wi-Fi setup worker.

## Offline verification

- Firmware `build-panda-tester` builds; image size `0x133090`, slot `0x1e0000`.
- 95 Python tests pass, including 62 production control-loop scenarios and new
  no-log, nonwaiting-event and deferred-settings regressions.
- Real Windows mutex/thread tests stall NVS and confirm OFF still completes;
  a newer fault defeats an in-flight clear, failed clear stays latched, and a
  successful clear does not restart heating.
- Compiled safety, ADC, physical-control and BLE objects have no ESP log symbol
  references. Host tests do not emulate ESP-IDF flash/cache suspension.
- No firmware was installed and no device was actuated for these checks.

## Follow-up audit fixes

Android now shows the fault-clear card on the Dashboard when the matched
firmware reports a latch/inhibit. "Check and clear fault" sends a dedicated
`clear_heater_fault` command with the current revision over BLE or REST, once.
It polls status for confirmation instead of interpreting an accepted request as
a cleared fault. Success requires the same device, clear-capable telemetry,
no latch/inhibit and STOP mode. No heating command is sent. Sensor, temperature,
ZC, busy and permanent-inhibit reasons are displayed; unknown/network/storage
failures are reported as unconfirmed, not success. New firmware and Android must
be installed together to expose the new root-level fault status fields.

- Application preinit OFF precedes its first log.
- Authenticated BLE/REST STOP is recognized before other mutation branches,
  including mixed heartbeat/unlock/token/reset and duplicate stop fields.
- Android BLE STOP cancels already admitted active/pending sessions through a
  per-device generation gate. It cannot recall a sent packet or guarantee delivery
  without a working radio link. New commands require new admission.
- Safety temperature is the hotter of calibrated and uncorrected table values;
  negative calibration cannot delay hard cutoff or PTC foldback. Positive
  correction still cuts conservatively. Telemetry retains calibrated values.
- PTC override can only lower the board limit (33 kOhm 99 C, 82 kOhm 102 C).
- Android OTA response reading is bounded while receiving; normal REST redirects
  are disabled too. Project names/hashes are still NOT digital signatures.
- Android tests include cancellation of active read/queued ON by STOP and bounded
  response parsing; 17 unit tests pass. Actual phone radio timing remains untested.
