# Persistent Settings and BLE/Wi-Fi Provisioning

Firmware v0.5.0 adds an NVS storage layer.

## Persisted user settings

The following user-facing settings are saved to NVS after REST/BLE changes:

- material profile
- chamber target temperature
- filter fan threshold
- heater permission threshold
- PTC cutoff
- drying mode
- custom drying temperature and timer
- preheat target and hold time
- maximum session time watchdog

Runtime states are deliberately **not** restored after reboot:

- `work_on`
- `drying_running`
- `preheat_running`
- heater output state

This prevents the heater from starting automatically after a reset or power loss.

## BLE provisioning fields

The BLE Control characteristic and `/api/settings` can store device network settings:

```json
{
  "wifi_ssid":"MyWiFi",
  "wifi_password":"secret-password",
  "moonraker_host":"192.168.1.100",
  "moonraker_port":7125
}
```

The config is saved to NVS. A reboot is recommended after changing Wi-Fi or Moonraker host settings.

## Factory reset

```json
{"factory_reset":true}
```

This erases the SnapHeater NVS namespace. The device will fall back to compile-time defaults after reboot.
