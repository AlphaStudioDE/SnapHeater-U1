# Changelog v1.8.0-dev

## Added

- Added `docs/original_flash_layout.md`.
- Updated `partitions.csv` to match the original Panda Breath 4 MB flash layout:
  - NVS at `0x9000`
  - OTA data at `0xe000`
  - `app0` at `0x10000`, size `0x1e0000`
  - `app1` at `0x1f0000`, size `0x1e0000`
  - SPIFFS at `0x3d0000`
  - coredump at `0x3ff000`

## Why

The full flash dump confirms that Panda Breath uses a large dual-OTA layout. SnapHeater U1 now follows this layout to provide more room for the firmware skeleton and future features.

## Safety note

This change does not enable heater output. Physical heater control remains locked by default until GPIO mapping and sensor calibration are verified on real hardware.
