# Third-Party Notices

SnapHeater U1 is a from-scratch firmware framework intended for ESP32-C3 based chamber-heater hardware and Snapmaker U1 integration through Moonraker.

## External ecosystems referenced

The project is designed to interoperate with:

- ESP-IDF / ESP32-C3 development tools,
- Moonraker / Klipper style APIs,
- Snapmaker U1 exposed printer status objects,
- Android BLE clients.

Those projects and ecosystems are not bundled as proprietary source code in this repository and remain under their respective licenses.

## Clean-room implementation note

The project source is intended to define an independent firmware framework and feature architecture. It does not include proprietary firmware source code, proprietary device assets, private keys, certificates, NVS data, or vendor-specific confidential files.

## User responsibility

Before enabling physical heater output, users must validate the target hardware, GPIO mapping, sensor readings, output polarity, fuse/protection behavior and thermal limits.
