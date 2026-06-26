# SnapHeater U1 Hardware Bring-Up Checklist

Use this checklist when the Panda Breath-style ESP32-C3 heater hardware is physically available.

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

Required defaults:

```txt
CONFIG_SHU1_ENABLE_HEATER_OUTPUT=n
CONFIG_SHU1_ENABLE_GPIO_PROBE=n
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
```

Do not continue if heater output or GPIO probe is enabled by accident.

## 5. First SnapHeater Flash: Heater Locked

Only flash a build where normal heater output is disabled.

```bash
idf.py -p <PORT> flash monitor
```

First boot checks:

- Firmware name appears in UART log.
- No reset loop.
- No brownout.
- Heater output reports disabled or locked.
- GPIO probe reports disabled.
- Wi-Fi initialization does not crash.
- BLE initialization does not crash.
- REST server initialization does not crash.
- Event log records boot/startup events.

Do not connect or trust heater output yet.

## 6. Local REST Smoke Test

From the same LAN:

```bash
curl http://<snapheater-ip>/api/health
curl http://<snapheater-ip>/api/status
curl http://<snapheater-ip>/api/events
```

Expected:

- JSON responses are valid.
- Firmware reports heater output locked.
- Runtime status is stable.
- Temperature fields are present.
- No fault storm or reboot loop.

## 7. BLE Smoke Test

Use a BLE scanner or test client:

- Confirm advertising name is `SnapHeater U1`.
- Read status characteristic.
- Read diagnostics characteristic.
- Confirm control writes are rejected until unlock if PIN protection is enabled.
- Confirm accepted settings do not energize heater output.

## 8. ADC And Sensor Validation

With heater output still disabled:

- Treat GPIO0 as the chamber/warehouse NTC ADC candidate.
- Treat GPIO1 as the PTC element NTC ADC candidate.
- Read chamber sensor at room temperature.
- Read PTC sensor at room temperature.
- Warm each sensor gently by hand or controlled warm air.
- Confirm temperature direction is correct.
- Confirm values are plausible.
- Confirm open/short detection if safe and understood.

Do not continue to output testing if ADC conversion is wrong.

## 9. Buttons And LEDs

- Treat GPIO7 as a zero-cross detector candidate first, not as a simple button.
- Do not enable K1 button behavior until GPIO7 pulse-width handling is understood.
- Treat GPIO0 as shared with the K2 button net and avoid configuring it as a
  button while it is needed for chamber ADC validation.
- Treat GPIO2 as the K3 button candidate.
- Treat GPIO6, GPIO5 and GPIO4 as K1/K2/K3 LED candidates.
- Test each known physical button.
- Confirm short press and long press behavior.
- Confirm OFF or emergency safe-off input is detected.
- Leave unknown button GPIOs at `-1`.
- Leave unknown LED GPIOs at `-1`.

Physical controls must not bypass the safety latch.

## 10. Fan Verification Before Heater

Only after board inspection:

- Enable GPIO probe only for a supervised fan test.
- Keep normal heater output disabled.
- Pulse fan first.
- Treat GPIO3 as the fan TRIAC gate candidate.
- Confirm GPIO7 zero-cross behavior before relying on phase-angle fan control.
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

Do not pulse the heater until all of these are true:

- Heater connector and MOSFET path are identified.
- GPIO18 is confirmed as the PTC relay driver candidate on the actual PCB.
- Heater GPIO is confirmed.
- Active polarity is confirmed.
- Fan behavior is confirmed.
- Chamber sensor is valid.
- PTC sensor is valid.
- Current path and power cutoff are understood.
- Output Safety Latch status is understood.
- Unlock Level L2 in [SAFETY_UNLOCK_PROCEDURE.md](SAFETY_UNLOCK_PROCEDURE.md) is satisfied.

Suggested first heater pulse, only under supervision:

```txt
heater pulse: 100-300 ms
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

Only after this should normal heater output be considered:

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
