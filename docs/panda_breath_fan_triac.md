# Panda Breath TRIAC Fan Control

SnapHeater U1 drives the Panda Breath fan through the accepted hardware map:

```txt
GPIO3 = TRIAC gate
GPIO7 = zero-cross detector
```

The firmware uses GPIO7 interrupts to detect AC half-cycles and a GPTimer to
schedule a short GPIO3 gate pulse after each zero-cross event.

Default tuning:

```txt
CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL=y
CONFIG_SHU1_AC_MAINS_HZ=50
CONFIG_SHU1_ZERO_CROSS_RISING_EDGE=y
CONFIG_SHU1_FAN_TRIAC_RUN_PERCENT=100
CONFIG_SHU1_FAN_TRIAC_MIN_DELAY_US=200
CONFIG_SHU1_FAN_TRIAC_GATE_PULSE_US=100
```

The current safety loop requests fan as ON/OFF. When fan is ON, the TRIAC driver
uses `CONFIG_SHU1_FAN_TRIAC_RUN_PERCENT`. Later UI work can expose variable fan
speed if real hardware testing shows it is useful.

If a DIY build uses a DC fan driver instead of Panda Breath AC/TRIAC hardware,
set:

```txt
CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL=n
```

Then `CONFIG_SHU1_FAN_GPIO` is driven as a plain output.
