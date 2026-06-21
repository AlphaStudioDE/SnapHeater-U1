# SnapHeater U1 Safety Unlock Procedure

This document defines what must be checked before enabling features that are locked by default.

Locked features are deliberately staged:

- Diagnostic GPIO Probe
- Fan output probing
- Heater output probing
- Normal heater automation
- Optional Moonraker write cooperation

Do not treat a successful firmware build as permission to unlock physical outputs.

## Unlock Levels

| Level | Feature | Build option | Allowed only after |
|---|---|---|---|
| L0 | Firmware boots, no output probing | `CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n`, `CONFIG_SHU1_ENABLE_GPIO_PROBE=n` | ESP-IDF build and serial boot are verified |
| L1 | Fan-only diagnostic probe | `CONFIG_SHU1_ENABLE_GPIO_PROBE=y`, heater output still disabled | Fan GPIO and power path are understood |
| L2 | Heater diagnostic probe | `CONFIG_SHU1_ENABLE_GPIO_PROBE=y`, heater output still disabled | Fan, sensors, polarity and emergency cutoff are verified |
| L3 | Normal heater automation | `CONFIG_SHU1_ENABLE_HEATER_OUTPUT=y` | Output Safety Latch checklist is complete |
| L4 | Optional safe printer cooperation | future whitelist setting | U1 Moonraker objects and safe commands are verified |

L4 must never include printer-critical actions such as movement, extrusion, bed/nozzle temperature control, pause, cancel, emergency stop or print start.

## Required Evidence Log

Keep a simple test log before unlocking anything beyond L0.

Recommended fields:

```text
date:
operator:
hardware revision / photos:
firmware commit:
ESP-IDF version:
serial port:
original flash backup hash:
heater GPIO:
fan GPIO:
heater active level:
fan active level:
chamber ADC channel:
PTC ADC channel:
emergency cutoff method:
test result:
```

Screenshots or photos are useful for PCB routing, connector labels and measurement setup.

## L0: Locked Firmware Boot

Required configuration:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n
CONFIG_SHU1_ENABLE_GPIO_PROBE=n
```

Checks:

- Firmware builds for `esp32c3`.
- Original flash backup exists and restore command is known.
- Firmware boots without reset loop.
- UART log reports heater output disabled or dry-run.
- UART log reports GPIO probe disabled.
- REST status endpoint responds.
- BLE advertising is visible if BLE is enabled.
- No output pin is expected to energize the heater.

Suggested checks:

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p <PORT> flash monitor
curl http://<snapheater-ip>/api/health
curl http://<snapheater-ip>/api/status
```

Stop if the board resets, overheats, smells unusual, browns out, or reports invalid critical state.

## L1: Fan Probe Unlock

Purpose: verify fan GPIO and active polarity before any heater-related test.

Required configuration:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n
CONFIG_SHU1_ENABLE_GPIO_PROBE=y
```

Prerequisites:

- Fan connector is identified.
- Fan supply voltage is understood.
- Fan driver path is visually inspected or measured.
- A current-limited supply or safe bench setup is used where possible.
- Heater connector is disconnected or otherwise unable to heat if practical.

How to test:

1. Confirm the fan GPIO from firmware configuration.
2. Send the shortest practical fan pulse.
3. Watch the fan, supply current and UART log.
4. Confirm active-high or active-low behavior.
5. Repeat only if the result is clear and safe.

Suggested first pulse:

```txt
fan pulse: 500 ms
```

Pass criteria:

- Fan turns on only during the requested pulse.
- Fan turns off immediately after the pulse.
- No heater activity occurs.
- No brownout or reset occurs.
- The observed polarity matches configuration.

After the test, disable GPIO Probe again unless more supervised probing is needed.

## L2: Heater Probe Unlock

Purpose: verify heater GPIO and active polarity using very short supervised pulses.

Required configuration:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n
CONFIG_SHU1_ENABLE_GPIO_PROBE=y
```

Prerequisites:

- L1 fan probe passed.
- Fan can run reliably.
- Heater connector and driver path are identified.
- Heater GPIO is identified.
- Heater active level is predicted and ready to verify.
- Chamber sensor gives plausible room-temperature readings.
- PTC sensor gives plausible room-temperature readings.
- Independent thermometer or thermal camera is available if possible.
- Emergency power cutoff is within reach.

How to test:

1. Start with fan verified and ready.
2. Confirm both sensor readings are plausible.
3. Send a very short heater pulse.
4. Watch current, PTC temperature, chamber temperature and UART log.
5. Stop after one pulse if behavior is unclear.

Suggested first pulse:

```txt
heater pulse: 100-300 ms
```

Pass criteria:

- Heater output responds only during the pulse.
- Polarity is confirmed.
- PTC temperature movement is plausible.
- Chamber temperature does not jump unrealistically.
- Fan behavior is safe.
- No reset, brownout, smoke, odor or unexpected heating occurs.

Stop conditions:

- Any sensor becomes invalid.
- PTC temperature rises too fast.
- Fan does not run when expected.
- Current draw is abnormal.
- Output remains on after the pulse.
- GPIO polarity is opposite of configuration.

Do not leave GPIO Probe enabled in builds used for normal operation.

## L3: Normal Heater Output Unlock

Purpose: allow normal firmware automation to energize the heater.

Required configuration:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=y
CONFIG_SHU1_ENABLE_GPIO_PROBE=n
```

Prerequisites:

- L1 fan probe passed.
- L2 heater probe passed.
- Heater GPIO is confirmed.
- Fan GPIO is confirmed.
- Heater active level is confirmed.
- Fan active level is confirmed.
- Chamber ADC channel is confirmed.
- PTC ADC channel is confirmed.
- ADC conversion direction is correct.
- Sensor open/short behavior is understood.
- Output Safety Latch validation passes.
- OFF or emergency safe-off behavior is verified.
- Conservative PTC cutoff is configured.
- Conservative chamber target is configured.

First normal heating test:

```txt
target chamber temperature: 30-35 C
session: supervised and short
fan post-run: enabled
PTC cutoff: conservative
```

Pass criteria:

- Fan starts before or with heater.
- Heater turns off at target or on fault.
- PTC remains below cutoff.
- Chamber temperature rises plausibly.
- OFF command stops heating.
- Faults stop heater and keep fan behavior safe.
- Fan post-run works.

If any pass criterion fails, return to L0 or L1 and correct configuration before trying again.

## L4: Optional Moonraker Write Cooperation

Default policy: read-only.

Do not enable any write cooperation unless all of these are true:

- The U1 Moonraker object is confirmed on a real printer.
- The command is chamber-related and non-critical.
- The command is explicitly whitelisted.
- The user explicitly enables the feature.
- Failure mode is safe.

Never whitelist:

- pause
- cancel
- emergency stop
- movement
- extrusion
- start print
- nozzle temperature changes
- bed temperature changes

SnapHeater U1 should cooperate with chamber climate, not take over the printer.

## Configuration Checklist

Before committing or sharing a test build, record these values:

```txt
CONFIG_SHU1_HEATER_GPIO=
CONFIG_SHU1_FAN_GPIO=
CONFIG_SHU1_CHAMBER_ADC_CH=
CONFIG_SHU1_PTC_ADC_CH=
CONFIG_SHU1_HEATER_ACTIVE_HIGH=
CONFIG_SHU1_FAN_ACTIVE_HIGH=
CONFIG_SHU1_ENABLE_GPIO_PROBE=
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=
CONFIG_SHU1_MAX_HEATER_PROBE_MS=
CONFIG_SHU1_PTC_CUTOFF_C=
CONFIG_SHU1_OVERTEMP_C=
```

## Release Rule

A public release intended for normal users must keep heater output locked by default unless the hardware target, pinout, polarity, sensors and safety behavior have been validated and documented.

Development builds may expose unlock options only when the documentation clearly marks them as supervised hardware validation features.
