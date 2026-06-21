# Original Panda Breath flash layout

This project targets Panda Breath-style ESP32-C3 heater hardware. A full 4 MB flash dump from a Panda Breath unit was analyzed to identify the original flash layout.

## Why this matters

The original hardware uses a large dual-OTA layout. Matching that layout gives SnapHeater U1 more app space and keeps the project closer to the hardware's expected memory organization.

## Parsed original partition table

| Name | Type | SubType | Offset | Size | Purpose |
|---|---|---|---:|---:|---|
| `nvs` | data | nvs | `0x9000` | `0x5000` | Settings, calibration, device config |
| `otadata` | data | ota | `0xe000` | `0x2000` | OTA slot selection |
| `app0` | app | ota_0 | `0x10000` | `0x1e0000` | OTA application slot 0 |
| `app1` | app | ota_1 | `0x1f0000` | `0x1e0000` | OTA application slot 1 |
| `spiffs` | data | spiffs | `0x3d0000` | `0x2f000` | Reserved for future local assets/config exports |
| `coredump` | data | coredump | `0x3ff000` | `0x1000` | Crash diagnostics |

## SnapHeater U1 partition table

The project now uses the matching layout in `partitions.csv`:

```csv
# SnapHeater U1 - Panda Breath original 4 MB flash layout
# Derived from full flash dump partition table.
# Name,    Type, SubType, Offset,   Size,     Flags
nvs,       data, nvs,     0x9000,   0x5000,
otadata,   data, ota,     0xe000,   0x2000,
app0,      app,  ota_0,   0x10000,  0x1e0000,
app1,      app,  ota_1,   0x1f0000, 0x1e0000,
spiffs,    data, spiffs,  0x3d0000, 0x2f000,
coredump,  data, coredump,0x3ff000, 0x1000,
```

## Notes

- The original dump contained two OTA app slots and an OTA data partition.
- The SPIFFS and coredump partitions were empty in the analyzed dump, but they are useful for future SnapHeater features.
- Raw NVS data may contain private device or Wi-Fi configuration and should not be published.
- Before flashing real hardware, keep a private full flash backup so the original device state can be restored if needed.
