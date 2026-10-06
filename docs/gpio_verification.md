# Panda Breath GPIO map

SnapHeater U1 accepts the following Panda Breath GPIO map for public development
builds. Tester defaults include heater support but do not start heating on boot.
This map is not a hardware qualification; see [safety status](SAFETY_STATUS.md).

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

Panel support is enabled in tester defaults. GPIO7 is
reserved for zero-cross detection, and GPIO0/GPIO1 are sensor inputs. Power LED
support is separately disabled because GPIO21 is also UART0 TX. Buttons on
strapping GPIO9/GPIO8/GPIO2 are ignored when held at boot until released.

## Safe bring-up order

1. Hardware inspection must be performed by someone qualified for mains equipment,
   with power disconnected and stored-energy hazards addressed.
2. Photograph both sides of PCB.
3. Identify heater connector, fan connector and NTC connectors.
4. Confirm ADC channels move when warming the chamber and PTC sensors.
5. Confirm fan behavior before any heater-related test.
6. Qualify outputs using an appropriate protected test setup. The firmware has no
   direct-GPIO probe bypass; do not create one to conduct this check.
7. Actual heating must use the normal guarded control path under supervision.

## Diagnostic probe build

The former probe API is disabled for both outputs. There is no supported probe
build. Current heating uses the normal hysteresis regulator and every runtime
safety governor, never a direct GPIO pulse. Panda fan control is held-gate ON/OFF,
not PWM or phase-angle regulation.
