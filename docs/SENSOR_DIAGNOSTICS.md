# Passive sensor diagnostics

This is an additional SnapHeater diagnostic, not a claim that DragonBreath
validated these detection thresholds. It does not change Panda pin assignments,
TRIAC gate timing, NTC conversion/filtering, SSR PID/window timing or PTC foldback.
It never requests a heating pulse to test a sensor.

## Acquisition freshness

The ADC producer stamps each two-channel acquisition with its start/end time and
a nonzero sequence number. The consumer requires a new sequence, start no earlier
than this acquisition attempt, ordered timestamps and at most 1500 ms from start
to checking the result. ADC errors still invalidate the affected channel; the
other channel is always attempted so its hard overtemperature cannot be hidden.
Successful function execution alone no longer refreshes sensor trust.

An invalid/replayed/late result removes heater permission. A blocked task cannot
execute this check until it runs again; the watchdog and physical protections
remain separate necessities. These stamps prove execution of acquisition code,
not that a malfunctioning ADC returned the true physical temperature.

## Suspicious raw invariance

Only an active, unpaused heating job collects evidence. All of these are required:

- Both valid raw ADC codes remain bit-identical for at least 60 seconds.
- The previous applied logical SSR output includes at least 30 seconds ON and
  30 seconds OFF during that unchanged interval.
- At least six logical SSR transitions have been observed.
- Samples remain valid and adjacent monitoring intervals are at most 1500 ms.

These conditions start a warning, not an immediate frozen-reading cutoff.
Panda starts a fixed 300-second countdown when it generates `sensor_freeze_warning`.
If neither raw code changes before that deadline, the control task trips the
existing persistent sensor fault. No phone response, receipt, disconnection or
pause extends the deadline. OFF cancels the pending warning by stopping the job.
Freshness failures, invalid sensors, overtemperature and ZC faults continue to
stop heating immediately, including during this grace period.

Any change in either raw code ends the pending warning and starts a new evidence
window. Before a warning starts, invalid samples, observation gaps, OFF and user
pause reset unconfirmed evidence. Display rounding,
calibration and the five-sample average are not inputs to this raw-code comparison.
Steady displayed temperature with changing raw codes does not trigger it.

These are conservative engineering thresholds, **not hardware-qualified thermal
limits or a guaranteed time-to-detect**. Normal quantized readings can theoretically
meet the condition: a trip means suspicion requiring investigation, not a proven
broken thermistor. One frozen sensor while the other changes, a stuck value with
noise, an erroneous but changing reading, or no qualifying SSR cycling can evade
this diagnostic. Existing no-rise protection still covers its own warm-up cases.
Do not interpret a passing diagnostic as proof that the device is safe.

## Fault handling and recovery

Freshness failure emits `sensor_sample_stale`; expired dual raw invariance emits
`sensor_raw_frozen`. Both use the existing `sensor_fault` safety category.
The control task cuts SSR before diagnostic logging or fault persistence. During
an active/armed session, the existing persistent latch stops the job and requests
fault cooling; ZC/physical fan limitations still apply. All command transports
see the same fault. Notification delivery depends on phone connectivity/settings.

Confirmed raw-invariance suspicion is not erased by OFF/pause. During the same
boot, both raw channels must show activity before fault clear is permitted. This
is still not proof of sensor accuracy. A rejected clear request is consumed, not
queued until conditions improve: a new explicit clear is required. Clearing never
restores the stopped job. After restart the generic persistent fault remains,
but detailed raw evidence is RAM-only; restart is not a sensor validation test.

## Offline verification

`tests/test_sensor_watch.py` compiles the production monitor and acquisition
function with fake ADC/time I/O. It checks freshness, sequence wrap, channel read
failures, normal stable temperatures with raw activity, idle/pause, one-channel
activity, incomplete SSR evidence, warning and grace boundaries and recovery.
`tests/control_loop_sim.c` executes the production safety loop and logical output
drivers: dual freeze near target trips, SSR is already OFF at NVS writes, fault
cooling stays requested, early clear is rejected, and recovery plus clear does
not restart heating. It also injects replayed, old, future and slow acquisitions.

These are software simulations, not tests on energized Panda hardware. Real ADC
noise/thermal response, false-positive behavior, watchdog/reset waveforms and
electrical conduction still require device-specific validation.

## Phone warning and local error history

REST runtime and compact BLE status expose `sensor_freeze_warning_ms` and
`sensor_freeze_remaining_s`. These are Panda-owned state, never control fields.
An active warning opens a dialog with Stop and Continue. Stop uses the existing
control path; failure is not presented as a confirmed stop. Continue records a
local acknowledgement only and does not send any heater/start/lease command.
The status dialog also works when OS notifications are disabled. On disconnection
the UI does not claim that its last remaining-time value is current.

OS event notifications open the app (they do not directly command an actuator).
The app selects the saved notified Panda and checks its full device identity
before showing connected controls. The two choices are in the app dialog.
Background receipt still requires the opt-in monitor and OS permissions; delivery
is not guaranteed. The Panda deadline never waits for notification delivery.

Below the temperature chart, the phone stores errors, warnings, warning-end/fault-
clear events and user actions in `event_history.db`. Storage is independent of
notification permission, deduplicated by full device ID + boot + sequence, and
excluded from automatic cloud/device-transfer backups. A storage failure does not
prevent attempting an OS notification. Older rows can be loaded in batches.

Event pages include `now_ms` and per-event uptime `ms`. A fixed per-boot phone
anchor reconstructs approximate calendar times, shown with `≈`; legacy pages
without these timestamps use receipt time. Phone actions use local phone time.
The Panda queue is still 32 RAM events: disconnection, overwrite or restart can
lose unreceived events. This is not a durable or complete incident black box.
