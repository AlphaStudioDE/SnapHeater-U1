# DIY Hardware Validation Checklist

Use this checklist before enabling any heater-related output on DIY hardware.

## Before Firmware Flash

- [ ] Wiring matches the documented schematic or personal wiring notes.
- [ ] Heater current path does not use breadboard or jumper wires.
- [ ] Heater branch has a fuse.
- [ ] Main power cutoff is reachable.
- [ ] ESP32-C3 is powered through a suitable regulator.
- [ ] Heater and fan drivers default to OFF during reset.
- [ ] Gate pulldowns are installed or driver inputs are otherwise held safe.
- [ ] Chamber NTC is connected to the intended ADC input.
- [ ] PTC NTC is connected to the intended ADC input.
- [ ] OFF/safe-stop input is wired or deliberately marked not installed.

## Firmware Configuration

- [ ] GPIOs match the DIY wiring.
- [ ] Unknown buttons and LEDs are set to `-1`.
- [ ] Heater output is disabled.
- [ ] GPIO Probe is disabled.
- [ ] NTC beta and resistor values match installed sensors.
- [ ] Flash size and partition layout build successfully.

Required first-build safety settings:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n
CONFIG_SHU1_ENABLE_GPIO_PROBE=n
```

## Sensor Validation

- [ ] Chamber sensor reads plausible room temperature.
- [ ] PTC sensor reads plausible room temperature.
- [ ] Chamber sensor changes correctly when warmed.
- [ ] PTC sensor changes correctly when warmed.
- [ ] Open/short behavior is understood.
- [ ] ADC values are not stuck or noisy enough to break safety logic.

## Fan Validation

- [ ] GPIO Probe enabled only for supervised testing.
- [ ] Heater output remains disabled.
- [ ] Fan pulse works.
- [ ] Fan turns off after pulse.
- [ ] Fan active polarity is confirmed.
- [ ] No heater activity occurs during fan pulse.

## Heater Probe Validation

Only continue after fan and sensors pass.

- [ ] Heater path is fused.
- [ ] Heater connector and MOSFET path are identified.
- [ ] Independent temperature observation is available.
- [ ] Emergency power cutoff is ready.
- [ ] First heater pulse is short.
- [ ] Heater turns off after pulse.
- [ ] Heater polarity is confirmed.
- [ ] PTC temperature response is plausible.
- [ ] No abnormal current, odor, smoke, reset or brownout occurs.

## Normal Heater Unlock

Before normal heater automation:

- [ ] Fan test passed.
- [ ] Heater probe passed.
- [ ] Chamber sensor valid.
- [ ] PTC sensor valid.
- [ ] Heater polarity confirmed.
- [ ] Fan polarity confirmed.
- [ ] OFF/safe-stop behavior confirmed.
- [ ] Output Safety Latch validation complete.
- [ ] Conservative target and cutoff values configured.

Then, and only then, consider:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=y
CONFIG_SHU1_ENABLE_GPIO_PROBE=n
```

## Documentation To Keep

- [ ] Wiring photo.
- [ ] PCB/perfboard photo.
- [ ] Firmware commit hash.
- [ ] `sdkconfig` or relevant config values.
- [ ] Sensor readings at room temperature.
- [ ] Fan pulse result.
- [ ] Heater pulse result.
- [ ] First low-temperature heating result.
