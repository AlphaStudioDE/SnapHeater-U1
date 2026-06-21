# Back to original firmware

This repository includes a generic 4 MB restoration image:

```text
firmware/original/generic.bin
```

It can be used to write a Panda Breath-style ESP32-C3 device back to the generic original firmware image included with this project.

## Important responsibility note

Flashing firmware always carries risk. You are responsible for selecting the correct serial port, using the correct chip target, keeping heater output safe, and verifying the device after flashing.

Before writing any image, make your own full backup of the device currently in your hands. A backup from your own device is the best restore point because it may contain device-specific data.

## File details

```text
Path: firmware/original/generic.bin
Size: 4,194,304 bytes
SHA-256: 236A4552B3ED014F2026FE40CEAA5252F9AEF92767AD5EF060FBCA4D0950B1F7
Target chip: ESP32-C3
Write offset: 0x0
Flash size: 4 MB
```

## Install esptool

```bash
python -m pip install esptool
```

## Find the serial port

Windows examples:

```text
COM3
COM4
COM5
```

Linux/macOS examples:

```text
/dev/ttyUSB0
/dev/ttyACM0
/dev/cu.usbserial-*
```

Replace `<PORT>` in the commands below with your actual port.

## Make a private backup first

```bash
esptool.py --chip esp32c3 -p <PORT> -b 460800 read_flash 0x0 0x400000 panda_breath_full_backup.bin
```

Keep this backup private. A full flash backup can contain Wi-Fi credentials, device IDs, pairing data or other private data.

## Optional: erase flash

Use this only if you understand the recovery process and have a backup.

```bash
esptool.py --chip esp32c3 -p <PORT> -b 460800 erase_flash
```

## Write the generic restoration image

From the repository root:

```bash
esptool.py --chip esp32c3 -p <PORT> -b 460800 write_flash 0x0 firmware/original/generic.bin
```

If flashing fails at a high baud rate, retry with a slower baud rate:

```bash
esptool.py --chip esp32c3 -p <PORT> -b 115200 write_flash 0x0 firmware/original/generic.bin
```

## Verify after flashing

- Disconnect heater power while validating firmware boot if the hardware allows it.
- Confirm the device boots normally.
- Confirm fan and heater behavior before unattended use.
- Do not assume restore success means the device is safe; wiring and hardware faults can still exist.

## Restore your own backup instead

If you have your own backup, restore it with:

```bash
esptool.py --chip esp32c3 -p <PORT> -b 460800 write_flash 0x0 panda_breath_full_backup.bin
```
