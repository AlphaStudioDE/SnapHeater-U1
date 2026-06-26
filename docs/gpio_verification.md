# GPIO verification guide

Static analysis gives a strong but not absolute pin map. Confirm before enabling live heating.

## Candidate map to verify

```txt
GPIO18 = PTC relay / heater output candidate
GPIO3  = fan TRIAC gate candidate
GPIO7  = zero-cross detector candidate, shared with K1 button behavior
GPIO0  = chamber/warehouse NTC ADC candidate, also shared with K2 button net
GPIO1  = PTC element NTC ADC candidate
GPIO2  = K3 button candidate
GPIO6  = K1 button/backlight LED candidate
GPIO5  = K2 button/backlight LED candidate
GPIO4  = K3 button/backlight LED candidate
GPIO21 = UART0 TX through the CH340K USB bridge
GPIO20 = UART0 RX through the CH340K USB bridge
```

`GPIO0`, `GPIO1` and `GPIO18` are still treated as unverified until continuity
or live measurement confirms the real board. Do not use this table as permission
to enable heater output.

## Preferred verification order

1. Open device with power disconnected.
2. Photograph both sides of PCB.
3. Identify heater connector, fan connector and NTC connectors.
4. Confirm whether the board uses ESP32-C3-MINI-1 module pad numbering when
   comparing schematic labels to GPIO numbers.
5. Trace PCB from ESP32-C3 module pads to driver, optocoupler, relay and ADC
   nets if possible.
6. Verify that GPIO18 reaches the PTC relay driver path before any heater pulse.
7. Verify that GPIO3 reaches the fan TRIAC gate driver path.
8. Verify that GPIO7 is the zero-cross detector input and understand its shared
   K1 button behavior before implementing K1.
9. Confirm active polarity: HIGH = ON or LOW = ON.
10. Confirm ADC channels move when warming the chamber and PTC sensors.
11. Only then enable live output in SnapHeater U1.

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
