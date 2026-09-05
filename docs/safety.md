# Safety design notes

SnapHeater U1 treats chamber heating as a hazardous function.

> Current target: SnapHeater firmware on original Panda Breath V1.0/V1.0.1
> electronics, using the DragonBreath-derived hardware layer. This is not the
> AirGuard 300 target.

## Firmware safety layers

1. Normal heater and fan control are build-disabled by default; verified pins remain mapped.
2. Diagnostic probe commands are rejected; no secondary GPIO writer is admitted.
3. Runtime safety checks and the Output Safety Latch guard physical heating.
4. Sensor fault stops heater request.
5. Chamber overtemperature stops heater request.
6. PTC local overtemperature has a 105 C hard cutoff plus 33k/82k board foldback.
7. Fan ON is applied at a validated zero-cross, OFF is immediate, and the heater
   cannot energize until the fan is confirmed running.
8. Runtime policy caps the chamber target at 55 C, even if the configuration ceiling is higher.
9. Drying mode has a timer.
10. Heater abnormal/no-rise detector turns heater off if no temperature rise is observed.

## Heater abnormal detection

Older v1.0.1 firmware strings included useful debug evidence:

```txt
PTC heating start detect
ptc heating detect finished
warehouse heating detect finished
PTC heating abnormal: temp rise too low
PTC heating normal
Sensor abnormal, reset PTC heating detect
```

SnapHeater U1 implements an independent detector:

- when heater is requested, store PTC and chamber temperatures;
- after a stabilization delay and detection window, check if PTC or chamber temperature rose;
- if both rises are too small, set `no_temperature_rise` and turn heater off.

Default policy:

```txt
rise delay: 15 s
rise window: 60 s
minimum PTC rise: 5 C
minimum chamber rise: 1 C
```

These no-rise values remain SnapHeater policy; DragonBreath does not provide an
equivalent validated no-rise detector. They are intentionally additional to,
not replacements for, the DragonBreath-derived 85 C chamber cutoff, 105 C PTC
cutoff and board-specific PTC foldback thresholds.

## Hardware safety still required

Firmware must not be the only safety layer. Use:

- thermal fuse / independent cutoff,
- correctly rated wire gauge,
- fuse on heater supply,
- adequate connector current rating,
- enclosure material rated for expected temperatures,
- creepage/clearance safety if mains power exists,
- independent emergency disconnection during first tests.

## First power-up recommendation

1. Back up and verify the original 4 MB flash before installing SnapHeater.
2. Read `/api/status` and confirm ADC values move with temperature.
3. Confirm the exact PCB revision and DragonBreath GPIO map before output testing.
4. Do not use the legacy probe API; diagnostic pulses are rejected.
5. Test the held-gate fan path first; never use PWM or phase-angle pulses.
6. Test the heater only through the guarded normal output path, under supervision,
   with current limiting and an independent thermometer.
7. Enable normal heater output only for deliberate research after confirming fan behavior, polarity and sensors.

For a stricter staged procedure, see [SAFETY_UNLOCK_PROCEDURE.md](SAFETY_UNLOCK_PROCEDURE.md).
