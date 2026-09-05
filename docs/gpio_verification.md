# Panda Breath GPIO map

SnapHeater U1 accepts the following Panda Breath GPIO map for public development
builds. Normal heater output remains locked by default and must not be enabled
without safe bench testing.

## Accepted map

Confidence follows DragonBreath's own documentation: GPIO3 fan and GPIO7
zero-cross are confirmed; GPIO18 heater and GPIO0/GPIO1 NTC routing are inferred
and require continuity confirmation on the target PCB before flashing.

```txt
GPIO18 = PTC relay / heater output
GPIO3  = fan TRIAC gate
GPIO7  = zero-cross detector
GPIO0  = chamber/warehouse NTC ADC
GPIO1  = PTC element NTC ADC
GPIO19 = 33k/82k NTC reference-resistor strap
GPIO9  = Power button (strap, active low)
GPIO8  = Auto button (strap, active low)
GPIO10 = On button (active low)
GPIO2  = Dry button (strap, active low)
GPIO6  = Auto LED
GPIO5  = On LED
GPIO4  = Dry LED
GPIO21 = UART0 TX through the CH340K USB bridge
GPIO20 = UART0 RX through the CH340K USB bridge
```

Panel support is intentionally disabled in the public defaults. GPIO7 is
reserved for zero-cross detection, and GPIO0/GPIO1 are sensor inputs. Power LED
support is separately disabled because GPIO21 is also UART0 TX. Buttons on
strapping GPIO9/GPIO8/GPIO2 are ignored when held at boot until released.

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

Heater probe command (expected to be rejected):

```bash
curl -X POST http://snapheater.local/api/probe \
  -H 'Content-Type: application/json' \
  -d '{"output":"heater","duration_ms":200}'
```

First heating must use the normal PID path and every runtime safety governor,
never a direct GPIO pulse.
