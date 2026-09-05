# Physical controls and indicator LEDs

This document describes the original Panda Breath panel as mapped by
DragonBreath. It does not apply to AirGuard 300.

## Exact map used by SnapHeater U1

```txt
Power button = GPIO9   (active low, internal pull-up, strapping pin)
Auto button  = GPIO8   (active low, internal pull-up, strapping pin)
On button    = GPIO10  (active low, internal pull-up)
Dry button   = GPIO2   (active low, internal pull-up, strapping pin)

Auto LED     = GPIO6   (active high)
On LED       = GPIO5   (active high)
Dry LED      = GPIO4   (active high)
Power LED    = GPIO21  (active high, shared with UART0 TX)
```

GPIO7 remains exclusively the zero-cross input. GPIO0 and GPIO1 remain NTC ADC
inputs. They are not panel-button nets.

Physical controls are optional and build-disabled in `sdkconfig.defaults`.
`sdkconfig.panda-safe.defaults` enables the three unshared LEDs and four buttons
for a supervised Panda Breath build. Power LED support remains disabled because
GPIO21 is also UART0 TX through the CH340K path.

## Boot-strapping protection

GPIO9, GPIO8 and GPIO2 are ESP32-C3 strapping pins. Firmware samples them as
inputs but ignores any button found held at boot until that button has first
been released. This reproduces DragonBreath's protection against a boot-held
button being interpreted as a command.

## SnapHeater actions

| Button | Short press | Long press |
|---|---|---|
| Power | Toggle current work request | Clear a persisted heater fault only after safe-stop validation |
| Auto | Select/toggle Auto mode | Emergency safe-off |
| On | Select/toggle Manual mode | Emergency safe-off |
| Dry | Select/toggle Drying mode | Emergency safe-off |

Every start request still passes through the output latch, sensor validation,
fan confirmation, hard temperature cutoffs and the persistent fault latch.

## LED behavior

| LED | Meaning |
|---|---|
| Auto | Auto selected/active; blink while selected but inactive |
| On | Manual or another active heat workflow; blink during cooldown |
| Dry | Drying selected/active |
| Power | Optional general power/status indication; disabled by default due to UART0 TX sharing |

No panel action directly drives the heater or fan GPIO.
