# Physical Controls and Indicator LEDs

SnapHeater U1 v1.6 adds a **Physical Control Layer** for Panda Breath-style buttons and LED/backlight indicators.

The goal is not to copy the original user interface. The goal is to make the physical controls match the SnapHeater U1 firmware:

- local AUTO / Symbiont-aware mode request,
- local ON / manual chamber hold request,
- local OFF / safe-stop,
- long-press emergency safe-off,
- acknowledgement of warnings/completion events,
- indicator LEDs for active mode, warnings, faults and connectivity.

## GPIO strategy

The accepted Panda Breath map includes LED/backlight GPIOs, but button actions
remain disabled by default until the K1/K2/K3 semantics are mapped safely to
SnapHeater actions.

Current defaults:

```txt
CONFIG_SHU1_BUTTON_AUTO_GPIO=-1
CONFIG_SHU1_BUTTON_ON_GPIO=-1
CONFIG_SHU1_BUTTON_OFF_GPIO=-1
CONFIG_SHU1_BUTTON_GENERIC_GPIO=-1

CONFIG_SHU1_LED_AUTO_GPIO=6
CONFIG_SHU1_LED_ON_GPIO=5
CONFIG_SHU1_LED_OFF_GPIO=4
CONFIG_SHU1_LED_ERROR_GPIO=-1
CONFIG_SHU1_LED_WIFI_GPIO=-1
CONFIG_SHU1_LED_BLE_GPIO=-1
```

GPIO7 is reserved for zero-cross detection and GPIO0/GPIO1 are sensor inputs.
Do not reuse those shared nets as generic buttons.

## Button behavior

| Button | Short press | Long press |
|---|---|---|
| AUTO | Request Auto mode | Toggle U1 Symbiont Mode |
| ON | Request Manual / Power-On chamber hold | Start Preheat/Hold using current target |
| OFF | Stop user heater cycles | Emergency safe-off, heater/fan forced off |
| Generic | Acknowledge notifications | Emergency safe-off |

Physical ON/AUTO does **not bypass** the Output Safety Latch. If the safety latch is not ready, the firmware records a warning and refuses to start heating intent.

## LED/backlight behavior

| LED | Meaning |
|---|---|
| AUTO LED | Auto mode active; slow blink when Auto is selected but not active |
| ON LED | Manual/preheat/drying/health mode active; slow blink during fan/cooldown |
| OFF LED | System is safely idle; slow blink during post-run fan/cooldown |
| ERROR LED | Fast blink for critical/fault, slow blink for pending warnings |
| Wi-Fi LED | Moonraker connected = solid; waiting/reconnect = blink |
| BLE LED | BLE enabled = solid placeholder |

## Safety notes

- OFF long press forces outputs off directly through `shu1_heater_force_off()`.
- ON/AUTO only change requested mode; the normal safety logic still decides whether heating is allowed.
- Button GPIOs remain disabled until their behavior is implemented deliberately.
- The physical panel is optional; Android/BLE and REST remain the primary configuration interfaces.
