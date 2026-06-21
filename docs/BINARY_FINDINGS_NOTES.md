# Actionable findings for SnapHeater U1 from all supplied binaries

## Keep / implement

- Use the original 4 MB Panda Breath partition layout with two 0x1E0000 OTA app slots.
- Keep NVS, otadata, SPIFFS and coredump partitions in the project.
- Use a new SnapHeater NVS namespace; do not depend on original Panda Breath NVS values.
- Keep dual sensor architecture: chamber/warehouse temperature and PTC temperature.
- Keep sensor-fault states: short/open for chamber and PTC paths.
- Keep heater-abnormal detection concept: heater output requested but expected temperature rise is too weak.
- Keep local web/API concepts but define new SnapHeater-owned API/REST/BLE contracts.
- Keep Moonraker/WebSocket integration as the Snapmaker U1 synchronization path.
- Keep physical button support and LED/backlight placeholders, but verify GPIO on hardware before enabling.
- Do not publish full flash dumps, raw NVS, device identifiers, Wi-Fi data or original web UI assets.

## Still requires hardware

- heater GPIO verification
- fan GPIO verification
- LED/backlight GPIO discovery
- button GPIO verification
- active HIGH/LOW confirmation
- NTC calibration
- UART boot logs
- real U1 Moonraker behavior
- Virtual Door temperature-drop calibration
