# SnapHeater U1 Build and Test Plan

This plan is the recommended order for moving from the current firmware skeleton
to the first real Panda Breath / ESP32-C3 tests.

For a bench checklist to use when hardware arrives, see
[docs/HARDWARE_BRINGUP_CHECKLIST.md](docs/HARDWARE_BRINGUP_CHECKLIST.md).
For the staged criteria to unlock probe and heater output features, see
[docs/SAFETY_UNLOCK_PROCEDURE.md](docs/SAFETY_UNLOCK_PROCEDURE.md).

## Safety principles

1. Do not enable normal heater output during the first build.
2. Use the accepted Panda Breath GPIO map, but keep live heater output locked
   until bench tests pass.
3. Keep `CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n` until fan, heater, sensors and
   output polarity are confirmed.
4. Use GPIO Probe mode only for short, supervised tests.
5. If any sensor value is invalid, heater output must remain OFF.

## Phase 1: Build only

Current public baseline: the firmware skeleton has been verified to build for
ESP32-C3 with ESP-IDF v5.3.5 using the Panda Breath-style 4 MB partition layout.
Hardware bring-up is still pending.

```bash
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
```

If rebuilding from a clean checkout, fix only build-related issues in this
phase. Do not add new features until the project builds.

Recommended first configuration:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n
CONFIG_SHU1_ENABLE_GPIO_PROBE=n
CONFIG_SHU1_ENABLE_BLE=y
CONFIG_SHU1_ENABLE_PHYSICAL_CONTROLS=y
CONFIG_SHU1_HEATER_GPIO=18
CONFIG_SHU1_FAN_GPIO=3
CONFIG_SHU1_ZERO_CROSS_GPIO=7
CONFIG_SHU1_CHAMBER_ADC_CH=0
CONFIG_SHU1_PTC_ADC_CH=1
```

Detailed button actions should remain `-1` until K1/K2/K3 behavior is mapped to
SnapHeater actions. Current accepted hardware pins are:

```txt
GPIO18 = PTC relay / heater output
GPIO3  = fan TRIAC gate
GPIO7  = zero-cross detector, shared with K1 behavior
GPIO0  = chamber/warehouse NTC ADC, also shared with K2 button net
GPIO1  = PTC element NTC ADC
GPIO2  = K3 button net
GPIO6  = K1 LED
GPIO5  = K2 LED
GPIO4  = K3 LED
```

Do not enable shared button behavior until GPIO7 zero-cross handling and
GPIO0/GPIO1 sensor ownership are preserved.

## Phase 2: Flash without heater output

After a successful build and after making or verifying a full original flash
backup:

```bash
idf.py -p <PORT> flash monitor
```

First checks:

- firmware name/version in serial log,
- Wi-Fi init does not crash,
- BLE advertising appears as `SnapHeater U1`,
- REST API starts,
- event log contains boot events,
- no heater output is energized.

## Phase 3: Local API smoke test

From a device on the same LAN:

```bash
curl http://<snapheater-ip>/api/health
curl http://<snapheater-ip>/api/status
curl http://<snapheater-ip>/api/events
```

Expected:

- JSON responses,
- no reboot loops,
- heater status reports locked/disabled,
- ADC fields are present even if not calibrated yet.

## Phase 4: BLE smoke test

Use a BLE scanner or test Android client.

Checks:

- device advertises as `SnapHeater U1`,
- status characteristic can be read,
- diagnostics characteristic can be read,
- control characteristic rejects writes until PIN unlock if enabled,
- after unlock, settings writes update state but do not energize heater.

## Phase 5: Moonraker / Snapmaker U1 test

With Snapmaker U1 online:

- configure Moonraker host/IP,
- confirm WebSocket connection,
- confirm `server.info` / ready state,
- confirm object list autodetection,
- confirm status updates for:
  - `print_stats`,
  - `display_status`,
  - `heater_bed`,
  - `toolhead`,
  - `print_task_config`,
  - cavity/chamber temperature sensor if present.

No Moonraker write actions should be used at this stage.

## Phase 6: Sensor validation

With heater output still disabled:

1. Read chamber ADC/temperature at room temperature.
2. Read PTC ADC/temperature at room temperature.
3. Warm each sensor gently and verify direction of change.
4. Check open-circuit behavior if safely possible.
5. Check short-circuit behavior only if safe and understood.

Do not proceed if ADC-to-temperature conversion is wrong.

## Phase 7: Board inspection / pin verification

Before GPIO Probe:

- photograph PCB top and bottom,
- identify heater connector,
- identify fan connector,
- identify sensor connectors,
- identify MOSFET/driver path,
- confirm GPIO18 PTC relay driver behavior,
- confirm GPIO3 fan TRIAC gate behavior,
- confirm GPIO7 zero-cross input behavior,
- confirm GPIO0/GPIO1 sensor behavior before using shared button nets,
- check output behavior against accepted active-high defaults,
- verify whether the fan can run before/with heater.

## Phase 8: GPIO Probe mode

Only after board inspection:

```txt
CONFIG_SHU1_ENABLE_GPIO_PROBE=y
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n
```

Order:

1. Fan pulse first.
2. Verify GPIO3 behavior.
3. Heater pulse only if output path is understood.
4. Use very short pulses.
5. Keep physical power cutoff available.

Suggested probe durations:

```txt
fan pulse: 1000 ms
heater pulse: 200-500 ms
```

## Phase 9: Output Safety Latch validation

Before real heating:

- fan output verified,
- heater output verified,
- chamber sensor valid,
- PTC sensor valid,
- output polarity verified,
- first setup wizard marked complete,
- incident/fault state clear,
- safety score acceptable.

Only then consider enabling:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=y
```

## Phase 10: First real low-temperature test

Start with conservative values:

```txt
target chamber temp: 30-35 C
PTC cutoff: conservative
manual session max: short
fan post-run: enabled
```

Observe:

- fan starts before or with heater,
- chamber temp rises slowly and plausibly,
- PTC temp remains below cutoff,
- heater stops at target,
- fan post-run works,
- emergency OFF works.

## Phase 11: Feature validation after safe heating

Only after the basic heating loop is proven:

- Preheat/Hold,
- Heat Soak,
- Tempering,
- Drying,
- Smart Pause Hold,
- Virtual Door/Open Lid Detection,
- Heater Health Test,
- Energy Estimate,
- Symbiont Mode read-only behavior.

## Do not test yet

Until later:

- optional Moonraker write/control actions,
- fully automatic ventilation cooperation,
- high-temperature chamber operation,
- unattended overnight mode,
- scheduled preheat without supervision.
