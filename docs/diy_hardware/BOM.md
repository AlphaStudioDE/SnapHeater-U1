# DIY Reference BOM

This bill of materials describes component roles and selection criteria. Exact parts depend on the target voltage, heater power, enclosure and local availability.

## Core Electronics

| Item | Quantity | Selection notes |
|---|---:|---|
| ESP32-C3 development board or module | 1 | Must expose enough GPIOs and ADC-capable pins |
| 5 V or 3.3 V regulator/buck converter | 1 | Sized for ESP32-C3, LEDs and control electronics |
| Logic-level MOSFET heater driver | 1 | Rated above heater voltage and current, with thermal margin |
| Logic-level MOSFET fan driver | 1 | Rated above fan voltage and current |
| Gate resistors | 2 | Typical driver input damping; value depends on circuit |
| Gate pulldown resistors | 2 | Keeps outputs off during boot/reset |
| Flyback diode or TVS for fan path | 1 | Use suitable protection for inductive loads |
| Fuse holder and fuse for heater supply | 1 | Sized for the heater branch |
| Main power switch or cutoff | 1 | Must physically remove heater power |
| Terminal blocks or rated connectors | as needed | Current and temperature rated |

## Thermal Components

| Item | Quantity | Selection notes |
|---|---:|---|
| Low-voltage PTC heater module | 1 | Use DC module with known voltage/current rating |
| Fan or blower | 1 | Rated for supply voltage and chamber temperature |
| Chamber temperature sensor | 1 | NTC thermistor recommended for current firmware |
| PTC/heater-local temperature sensor | 1 | Mounted near heater body or exhaust path |
| Independent thermal cutoff/fuse | 1 | Hardware cutoff independent of firmware |

## Sensor Interface

| Item | Quantity | Selection notes |
|---|---:|---|
| NTC divider resistor for chamber sensor | 1 | Match firmware assumptions or update config/calibration |
| NTC divider resistor for PTC sensor | 1 | Match firmware assumptions or update config/calibration |
| Optional RC filter components | 2 sets | Useful for noisy ADC wiring |

Default firmware assumptions:

```txt
CONFIG_SHU1_NTC_BETA=3950
CONFIG_SHU1_NTC_R0_OHM=100000
CONFIG_SHU1_NTC_SERIES_OHM=100000
```

If your thermistors or divider resistors differ, update firmware configuration and validate readings before any output test.

## Physical Controls And Indicators

| Item | Quantity | Selection notes |
|---|---:|---|
| OFF / safe-stop button | 1 | Strongly recommended |
| AUTO button | optional | For printer-aware mode |
| ON / manual button | optional | For local hold/preheat shortcut |
| ACK button | optional | For acknowledging warnings |
| Status LEDs | optional | Use current-limiting resistors or LED driver |

Unknown or unused GPIOs should stay `-1` in firmware configuration.

## Power Sizing Notes

Use conservative margins.

```text
heater_current_A = heater_power_W / supply_voltage_V
mosfet_current_rating > heater_current_A with thermal margin
fuse_rating > normal_current and < unsafe_wire_or_connector_current
wire_rating > fuse_rating
connector_rating > fuse_rating
```

Avoid running high heater current through breadboards, jumper wires or small prototype contacts.

## What Not To Use

- ESP32 GPIO directly connected to heater or fan load.
- Solderless breadboard for heater current.
- Unknown heater modules without voltage/current rating.
- Connectors or wires that get warm during normal use.
- Mains AC heater path unless designed and validated by a qualified person.
