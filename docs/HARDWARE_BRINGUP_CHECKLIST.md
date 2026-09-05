# SnapHeater U1 Hardware Bring-Up Checklist

Use this checklist for SnapHeater U1 on original Panda Breath V1.0/V1.0.1
electronics. It does not cover AirGuard 300.

The goal is to move from an untouched device to a safe SnapHeater U1 test boot without energizing the heater unexpectedly.

For the staged unlock criteria behind GPIO probe and normal heater output, see
[SAFETY_UNLOCK_PROCEDURE.md](SAFETY_UNLOCK_PROCEDURE.md).

## Stop Conditions

Stop immediately if any of these happen:

- The board, wiring, connector, MOSFET path or power path is unclear.
- The original flash backup is missing or unverified.
- The serial log shows repeated resets, brownouts or boot loops.
- ADC readings are missing, stuck, inverted or physically implausible.
- The fan output cannot be verified independently.
- The heater output polarity is unknown.
- The emergency-off path is not understood.
- Any sensor reports open, short or invalid during a heating-related test.

## 1. Before Connecting Power

- Photograph the PCB top and bottom.
- Photograph all connectors before unplugging anything.
- Identify heater, fan, temperature sensor and button/LED connectors.
- Note any printed PCB markings, component labels and connector labels.
- Check for visible damage, loose wires, cracked solder joints or heat marks.
- Keep the device on a non-flammable bench surface.
- Keep external power cutoff available.

Do not connect the heater to unattended power during inspection.

## 2. Backup And Restore Readiness

- Verify a full original flash backup exists.
- Confirm the backup size is exactly 4 MB.
- Store a hash of the backup.
- Keep restore instructions available before flashing anything.

Reference command:

```bash
esptool.py --chip esp32c3 -p <PORT> read_flash 0x0 0x400000 panda_breath_full_backup.bin
```

Reference restore command:

```bash
esptool.py --chip esp32c3 -p <PORT> write_flash 0x0 panda_breath_full_backup.bin
```

Do not publish full flash backups. They can contain private device data.

## 3. First Serial Connection

- Connect USB/UART without changing firmware.
- Identify the serial port.
- Capture the boot log.
- Record detected chip, flash size and boot mode.
- Confirm the device can enter bootloader/download mode.

Useful commands:

```bash
esptool.py --chip esp32c3 -p <PORT> chip_id
esptool.py --chip esp32c3 -p <PORT> flash_id
```

## 4. Build Verification

Before flashing SnapHeater U1, rebuild the project locally:

```bash
idf.py set-target esp32c3
idf.py build
```

Required defaults for a no-output bring-up build:

```txt
CONFIG_SHU1_ENABLE_GPIO_PROBE=n
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
```

Do not build with physical outputs enabled until the non-heating hardware layer
has passed the checks below. Do not
continue if `/api/status` reports `heater_output_on=true`, `fan_output_on=true`,
or an armed latch during an observation-only test.

## 5. First SnapHeater Flash: Observation Only

For the first flash, use either a no-output build or a build where runtime
state starts with outputs off and the latch disarmed.

```bash
idf.py -p <PORT> flash monitor
```

First boot checks:

- Firmware name appears in UART log.
- No reset loop.
- No brownout.
- Heater output reports off.
- Fan output reports off.
- Output Safety Latch reports not armed / not ready.
- GPIO probe reports disabled.
- Wi-Fi initialization does not crash.
- BLE initialization does not crash.
- REST server initialization does not crash.
- Event log records boot/startup events.

Do not command heating during this stage.

## 6. Local REST Smoke Test

From the same LAN:

```bash
curl http://<snapheater-ip>/api/health
curl http://<snapheater-ip>/api/status
curl http://<snapheater-ip>/api/events
```

Expected:

- JSON responses are valid.
- Firmware reports heater and fan outputs off.
- `runtime.zero_cross_*` fields are present.
- Runtime status is stable.
- Temperature fields are present.
- No fault storm or reboot loop.

## 6A. Zero-Cross Observation

With the board fully assembled, covers in a safe state, and mains connected
only when it is physically safe to do so, observe zero-cross without requesting
fan or heater output:

```bash
curl http://<snapheater-ip>/api/status
```

Expected on 50 Hz mains:

- `runtime.zero_cross_signal_present` is `true`.
- `runtime.zero_cross_edges_per_sec` is near `100` if both half-cycles are reported.
- `runtime.zero_cross_last_period_us` is near `10000`.
- `runtime.heater_output_on` remains `false`.
- `runtime.fan_output_on` remains `false`.

Expected when mains is absent or the detector is not receiving AC:

- `runtime.zero_cross_signal_present` is `false`.
- `runtime.zero_cross_edges_per_sec` is `0` or remains stale.

Do not request the held-gate fan until zero-cross is stable.

## 7. BLE Smoke Test

Use a BLE scanner or test client:

- Confirm advertising name is `SnapHeater U1`.
- Read status characteristic.
- Read diagnostics characteristic.
- Confirm control writes are rejected until unlock if PIN protection is enabled.
- Confirm accepted settings do not energize heater output.

## 8. ADC And Sensor Validation

With heater output still disabled:

- GPIO0 is the chamber/warehouse NTC ADC input.
- GPIO1 is the PTC element NTC ADC input.
- Read chamber sensor at room temperature.
- Read PTC sensor at room temperature.
- Warm each sensor gently by hand or controlled warm air.
- Confirm temperature direction is correct.
- Confirm values are plausible.
- Confirm open/short detection if safe and understood.

Do not continue to output testing if ADC conversion is wrong.

## 9. Buttons And LEDs

- Treat GPIO7 as a zero-cross detector first, not as a simple button.
- Do not reconfigure GPIO7 away from its zero-cross interrupt role.
- Keep GPIO0/GPIO1 exclusively as chamber/PTC NTC ADC inputs.
- Power/Auto/On/Dry buttons are GPIO9/GPIO8/GPIO10/GPIO2, active low.
- GPIO9/GPIO8/GPIO2 are strapping pins: a held-at-boot button must remain
  ignored until its first release.
- Auto/On/Dry LEDs are GPIO6/GPIO5/GPIO4, active high.
- Leave the GPIO21 Power LED disabled while UART0 TX is in use.
- Test each known physical button.
- Confirm short press and long press behavior.
- Confirm every long press produces emergency safe-off.

Physical controls must not bypass the safety latch.

## 10. Fan Verification Before Heater

Only after board inspection:

- Enable guarded diagnostics only for a supervised fan test.
- Keep normal heater output disabled.
- Test the fan first.
- GPIO3 is the fan TRIAC gate.
- Confirm GPIO7 zero-cross behavior through `/api/status` before requesting fan ON.
- Confirm GPIO3 stays HIGH while ON and goes LOW immediately on OFF; do not use PWM or gate pulses.
- Confirm fan GPIO and active polarity.
- Confirm fan can run without heater.
- Follow Unlock Level L1 in [SAFETY_UNLOCK_PROCEDURE.md](SAFETY_UNLOCK_PROCEDURE.md).

Suggested configuration:

```txt
CONFIG_SHU1_ENABLE_GPIO_PROBE=y
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n
```

Suggested first fan pulse:

```txt
fan pulse: 500-1000 ms
```

Disable GPIO probe again after the test.

## 11. Heater Probe Readiness

Do not run the first low-target PID heating test until all of these are true:

- Heater connector and MOSFET path are identified.
- GPIO18 is the PTC relay driver.
- Heater GPIO is confirmed.
- Active polarity is confirmed.
- Fan behavior is confirmed.
- Chamber sensor is valid.
- PTC sensor is valid.
- Current path and power cutoff are understood.
- Output Safety Latch status is understood.
- Output Safety Latch is explicitly armed and reports ready.
- Normal heater output is build-enabled for this deliberate test; the probe API cannot drive it.
- Unlock Level L2 in [SAFETY_UNLOCK_PROCEDURE.md](SAFETY_UNLOCK_PROCEDURE.md) is satisfied.

Suggested first controlled test, only under supervision:

```txt
normal PID target: 30-35 C, with independent cutoff and thermometer
```

Stop if PTC temperature jumps unexpectedly or fan behavior is wrong.

## 12. Output Safety Latch Validation

Before any normal heating:

- Fan verified.
- Heater verified.
- Sensor readings valid.
- Output polarity verified.
- Setup validation complete.
- Fault state clear.
- Manual safe-off confirmed.
- Safety score acceptable.
- Unlock Level L3 in [SAFETY_UNLOCK_PROCEDURE.md](SAFETY_UNLOCK_PROCEDURE.md) is satisfied.

Only after this should normal heater output be considered for deliberate DIY or research hardware:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=y
```

## 13. First Low-Temperature Heating Test

Start conservatively:

```txt
target chamber temperature: 30-35 C
short supervised session
fan post-run enabled
PTC cutoff conservative
```

Observe:

- Fan starts before or with heater.
- Chamber temperature rises slowly and plausibly.
- PTC temperature remains below cutoff.
- Heater stops at target.
- Fan post-run works.
- OFF or emergency safe-off works.

## 14. After Successful Basic Heating

Only then validate advanced features:

- Preheat / Hold
- Heat Soak
- Chamber Stability Lock
- Tempering
- Drying
- Smart Pause Hold
- Virtual Door / Open Lid Detection
- Heater Health Test
- Energy Estimate
- U1 Symbiont Mode read-only behavior

## Never Test Early

- Unattended heating
- Overnight mode
- Scheduled preheat without supervision
- High-temperature chamber operation
- Moonraker write/control actions
- Heater output with invalid sensors
- Heater output without verified fan behavior
