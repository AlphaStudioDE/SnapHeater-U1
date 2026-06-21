# SnapHeater U1 DIY Reference Hardware

This folder is for builders who want to assemble a SnapHeater-compatible chamber heater controller instead of using Panda Breath-style hardware.

The DIY hardware path is a reference architecture, not a certified product design. It should be treated as a supervised development build.

## Design Goals

- ESP32-C3 controller running SnapHeater U1 firmware.
- Low-voltage DC heater path.
- Separate fan/filter output.
- Two temperature sensors:
  - chamber air sensor,
  - PTC/heater-local sensor.
- Physical OFF or safe-stop input.
- External fuse and power cutoff.
- Firmware defaults that keep heater output locked until validation is complete.

## Recommended Reading Order

1. [BOM.md](BOM.md)
2. [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md)
3. [VALIDATION_CHECKLIST.md](VALIDATION_CHECKLIST.md)
4. [../SAFETY_UNLOCK_PROCEDURE.md](../SAFETY_UNLOCK_PROCEDURE.md)

## Safety Boundary

This reference is intended around low-voltage DC systems. Do not design a mains-powered heater controller from this folder unless you are qualified to design, enclose, fuse and validate mains equipment.

Firmware safety is not a replacement for:

- correct wire gauge,
- fuses,
- thermal cutoff,
- safe connectors,
- adequate enclosure materials,
- independent power disconnect,
- supervised testing.

## Firmware Compatibility

The firmware expects these logical functions:

| Function | Default firmware symbol | Notes |
|---|---|---|
| Heater output | `CONFIG_SHU1_HEATER_GPIO` | Drives a MOSFET/driver, not the heater directly |
| Fan output | `CONFIG_SHU1_FAN_GPIO` | Drives fan/filter path |
| Chamber NTC | `CONFIG_SHU1_CHAMBER_ADC_CH` | ADC input through divider |
| PTC NTC | `CONFIG_SHU1_PTC_ADC_CH` | ADC input through divider |
| Physical controls | `CONFIG_SHU1_BUTTON_*_GPIO` | Optional but OFF/safe-stop is strongly recommended |
| Indicator LEDs | `CONFIG_SHU1_LED_*_GPIO` | Optional |

The default firmware target still uses Panda Breath-inferred GPIOs. DIY builders must set GPIOs to match their own wiring and then follow the unlock procedure.

## Minimum DIY Build

A minimum bench prototype needs:

- ESP32-C3 development board,
- low-voltage DC power supply,
- fused heater supply,
- logic-level MOSFET heater driver,
- logic-level MOSFET fan driver,
- PTC heater module,
- fan,
- chamber NTC,
- PTC/heater NTC,
- physical OFF/safe-stop button,
- wiring, connectors and enclosure suitable for the current and temperature.

Do not run the heater from an ESP32 GPIO. GPIOs only control driver inputs.

## Suggested Repository Flow

- Build firmware with heater output locked.
- Wire and validate sensors first.
- Validate fan output before heater output.
- Probe heater only with short supervised pulses.
- Enable normal heater output only after [../SAFETY_UNLOCK_PROCEDURE.md](../SAFETY_UNLOCK_PROCEDURE.md) level L3 is satisfied.
