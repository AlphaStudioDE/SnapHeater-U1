# Safety design notes

SnapHeater U1 treats chamber heating as a hazardous function.

## Firmware safety layers

1. Normal heater output disabled by default at build time.
2. Diagnostic probe API disabled by default at build time.
3. Heater GPIO, fan GPIO and ADC channels are marked as inferred until PCB verification.
4. Sensor fault stops heater request.
5. Chamber overtemperature stops heater request.
6. PTC local overtemperature stops heater request.
7. Fan runs during heating and during fault cooldown.
8. Chamber target is clamped by `CONFIG_SHU1_MAX_TARGET_TEMP_C`, default 60 C.
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

These values are conservative development placeholders and must be tuned on real hardware.

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

1. Flash only with `CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n`.
2. Read `/api/status` and confirm ADC values move with temperature.
3. Open device and verify PCB routing or measure outputs.
4. Optionally compile a separate probe build with `CONFIG_SHU1_ENABLE_GPIO_PROBE=y`.
5. Test fan pulse first.
6. Test heater pulse only with supervision, current limiting and independent thermometer.
7. Enable normal heater output only after confirming GPIO, polarity and sensors.
