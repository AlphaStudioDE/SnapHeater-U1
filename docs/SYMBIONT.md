# Symbiont printer ventilation — local implementation

Android offers Auto and Symbiont in printer ventilation settings; Apply saves
the selection. Auto ends Panda overrides. Symbiont acts during active unpaused
Panda heating tasks with a positive effective target, including tempering using
the current ramp target. It does not operate an idle printer without a Panda job.
Legacy read-only configurations remain read-only until the new explicit policy
and safe-control flags are saved by the app.

## Curve

Use the fresh unrounded Panda chamber reading minus the effective target:

| Delta | Requested speed while cooling |
| --- | --- |
| >= 7 C | 100% |
| 6 C | 75% |
| 5 C | 50% |
| 3 C | 30% |
| > 2 and < 3 C | 30% |
| <= 2 C | OFF |

Linear interpolation between the points, rounded to integer percent. Enter
cooling only ABOVE +5 C; remain in cooling until <= +2 C. While this cooling
phase is active, the Panda PID is inhibited so it does not oppose the requested
cooling. This curve NEVER controls the Panda fan TRIAC: its held-gate airflow
and thermal purge remain under the existing safety loop.

## Printer protocol

The Moonraker worker allows only the exact `fan_generic cavity_fan` object and
optional `purifier` object found during discovery. The user confirmed the AUX
fan physically with `SET_FAN_SPEED`; the new transport still needs testing.

- AUX: `printer.control.generic_fan`, params `name: cavity_fan`, integer `speed`.
- Top cover: `printer.control.purifier`, params `fan: exhaust`, integer `speed`,
  `skip_delay: 1`. No `delay`, `work`, mode changes or inner-fan writes.

These endpoint contracts were inspected in the local SnapScreen U1 rootfs:
Moonraker `components/klippy_apis.py`, `components/klippy_connection.py`, and
Klipper `extras/fan_generic.py`, `extras/purifier.py`. This is a stock-U1-specific
extension, NOT a universal Klipper interface. Unsupported endpoints produce an
unavailable state rather than fallback G-code or arbitrary object names.

Full queries check webhooks readiness, fan speed, top-cover detection and its
reported critical-temperature/fan alarms. No lowering/writing follows an
invalid observation or reported alarm. Disappearance of a previously detected
top cover suppresses writes until data recovers (a fresh connection starts new
discovery). Only detected top covers receive commands. The internal circulation
fan, heater, movement commands and alarm-clear commands are not addressed.

One request is outstanding at a time, with a unique negative ID. Replies are
ignored after demand/activation changes, timeout or disconnect. Commands are
followed by full readback before another write. The two fan channels alternate
priority when both are repeatedly overridden, avoiding top-cover starvation.
Partial fan notifications trigger a fresh full query; absent notifications are
covered by nominal 500 ms polling. The shared Moonraker task checks at nominal
100 ms cadence, but network operations/autodetection may add latency. A request
times out after 2 seconds; errors back off. No network I/O holds the thermal
policy lock. Unavailability/recovery events enter the existing event history.

## Important limitations

This is reactive correction, NOT exclusive ownership of printer fans. Slicer
and printer automation can change outputs between corrections. Top-cover HOT
mode may repeatedly undo our exhaust command; we do not disable that mode or
its safety logic. A matched readback is one observation, not guaranteed physical
airflow, cooling effectiveness or continued ownership.

Per user decision, ventilation link loss does not introduce a new heater-stop
condition. Panda sensor, overtemperature, ZC, watchdog, timeout and other existing
protection remains authoritative. Existing AUTO printer-freshness requirements
also remain unchanged. Reconnection requires fresh discovery/readback before
resuming corrections. Phone disconnection does not control this mechanism.

Auto, OFF, pause, faults and maintenance stop subsequent overrides; they do not
restore a guessed historic slicer speed. The last speed can remain until the
printer sends another command. An already transmitted packet cannot be recalled
and may finish after a user changes the mode. A lost TCP connection does not
prove the last command failed. No stateful commands are deliberately replayed
after reconnect; current demand is recalculated instead.

The AUX fan primarily cools the print, not an external chamber exhaust. Neither
effective chamber cooling nor stable physical rotation at 30% is qualified by
host simulations. No printer firmware was changed, device flashed or printer
command sent during implementation. Full electrical safety audit/qualification
is a separate task and is not complete merely because these tests pass.
