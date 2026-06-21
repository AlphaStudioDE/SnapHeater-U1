# GPIO verification guide

Static analysis gives a strong but not absolute pin map. Confirm before enabling live heating.

## Inferred map

```txt
GPIO18 = heater/PTC output
GPIO3  = fan/filter output
GPIO0  = chamber/warehouse ADC channel
GPIO1  = PTC ADC channel
GPIO7  = physical button
```

## Preferred verification order

1. Open device with power disconnected.
2. Photograph both sides of PCB.
3. Identify heater connector, fan connector and NTC connectors.
4. Trace PCB from ESP32-C3 pins to driver/MOSFET gates if possible.
5. Boot original firmware and measure GPIO18/GPIO3 while switching heater/fan in UI.
6. Confirm active polarity: HIGH = ON or LOW = ON.
7. Confirm ADC channels move when warming chamber/ptc sensor.
8. Only then enable live output in SnapHeater U1.

## Diagnostic probe build

Only after basic PCB inspection, compile with:

```txt
CONFIG_SHU1_ENABLE_GPIO_PROBE=y
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n
```

Fan pulse:

```bash
curl -X POST http://snapheater.local/api/probe \
  -H 'Content-Type: application/json' \
  -d '{"output":"fan","duration_ms":1000}'
```

Heater pulse:

```bash
curl -X POST http://snapheater.local/api/probe \
  -H 'Content-Type: application/json' \
  -d '{"output":"heater","duration_ms":200}'
```

The heater pulse endpoint is intentionally short and compile-time locked. Use current limiting and an independent thermometer.
