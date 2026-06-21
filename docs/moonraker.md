# Moonraker integration

SnapHeater U1 connects to:

```txt
ws://<snapmaker_u1_or_moonraker_host>:7125/websocket
```

It sends an initial `printer.objects.query` and then a persistent `printer.objects.subscribe`.

Subscribed objects:

```json
{
  "webhooks": ["state"],
  "virtual_sdcard": ["progress"],
  "print_stats": ["state", "filename", "total_duration", "print_duration"],
  "extruder": ["temperature", "target"],
  "heater_bed": ["temperature", "target"]
}
```

## Auto mode policy

Auto mode allows fan/filter when:

- bed target or bed temperature is at or above `filtertemp`, default 30 C.

Auto mode allows heater request only when:

- `work_on` is true,
- printer is printing or paused,
- bed target or current bed temperature is near/above `hotbedtemp`, default 80 C,
- chamber target is not yet reached,
- all sensors are valid,
- PTC/chamber limits are safe.

This is designed to avoid heating the chamber when the printer is idle or not thermally active.

## Future additions

- Moonraker host autodetection / mDNS
- setup portal
- optional chamber temperature object if Snapmaker U1 exposes it
- local API token
- optional Home Assistant discovery
