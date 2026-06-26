# Panda Breath GPIO map

SnapHeater U1 accepts the following Panda Breath GPIO map for public development
builds. Normal heater output remains locked by default and must not be enabled
without safe bench testing.

## Accepted map

```txt
GPIO18 = PTC relay / heater output
GPIO3  = fan TRIAC gate
GPIO7  = zero-cross detector, shared with K1 behavior
GPIO0  = chamber/warehouse NTC ADC, also shared with K2 button net
GPIO1  = PTC element NTC ADC
GPIO2  = K3 button net
GPIO6  = K1 button/backlight LED
GPIO5  = K2 button/backlight LED
GPIO4  = K3 button/backlight LED
GPIO21 = UART0 TX through the CH340K USB bridge
GPIO20 = UART0 RX through the CH340K USB bridge
```

Button semantics are intentionally disabled by default. GPIO7 is reserved for
zero-cross detection, and GPIO0/GPIO1 are sensor inputs. Do not turn those shared
nets into generic buttons without a dedicated firmware change.

## Safe bring-up order

1. Open device with power disconnected.
2. Photograph both sides of PCB.
3. Identify heater connector, fan connector and NTC connectors.
4. Confirm ADC channels move when warming the chamber and PTC sensors.
5. Confirm fan behavior before any heater-related test.
6. Confirm the heater output only with short, supervised probe pulses.
7. Only then consider enabling normal heater output in SnapHeater U1.

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
