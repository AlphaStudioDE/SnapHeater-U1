# Snapmaker U1 Moonraker integration (v0.6.0)

This module was updated using the user's older coroNET `sketch_v20.ino` Moonraker implementation as a field-tested reference for Snapmaker U1 behavior. The implementation remains from-scratch/reimplemented in ESP-IDF C for SnapHeater U1.

## Connection

Default Moonraker endpoint:

```text
ws://<snapmaker-u1-ip>:7125/websocket
http://<snapmaker-u1-ip>:7125/printer/objects/list
```

The firmware sends `server.connection.identify` after WebSocket connect:

```json
{
  "jsonrpc":"2.0",
  "method":"server.connection.identify",
  "params":{
    "client_name":"SnapHeater U1",
    "version":"0.6.0-dev",
    "type":"display"
  },
  "id":1
}
```

It then calls `server.info` and waits for `klippy_state == "ready"` before autodetection.

## Snapmaker U1 object set

The field-tested coroNET object set uses:

```text
print_stats.state
print_stats.filename
print_stats.print_duration
print_stats.total_duration
display_status.progress
toolhead.extruder
print_task_config.filament_color_rgba
print_task_config.filament_type
extruder.temperature / target
extruder1.temperature / target
extruder2.temperature / target
extruder3.temperature / target
heater_bed.temperature / target
```

Unlike the first SnapHeater skeleton, v0.6.0 prefers `display_status.progress` and only uses `virtual_sdcard.progress` as fallback.

## U1 chamber/cavity detection

`/printer/objects/list` is used to detect a chamber temperature object. Preferred names:

```text
temperature_sensor cavity
temperature_sensor chamber
temperature_sensor enclosure
temperature_sensor chamber_temp
temperature_sensor enclosure_temp
```

Fallback:

```text
temperature_sensor cavity
```

The reported U1 chamber temperature is smoothed with EMA alpha 0.25. This value is informational for now; the heater safety loop still relies on SnapHeater's local chamber and PTC sensors.

## Cavity fan detection

The module also tries to detect:

```text
fan_generic cavity_fan
temperature_fan cavity_fan
objects containing cavity_fan
```

This is optional and used only for status/diagnostics unless later mapped into a feature.

## Reconnect and stale detection

From coroNET field behavior:

```text
WS reconnect: 5000 ms
WS stale: 15000 ms
HTTP object-list timeout: 200 ms
Autodetect retry: 8000 ms
Printer stale/failsafe window: 22000 ms
```

If the WebSocket stays technically connected but no useful messages arrive for the stale window, SnapHeater stops the client and forces reconnect. This avoids silent frozen printer states.

## Auto mode safety policy

Auto mode may heat only when printer data is fresh and the normalized print state is:

```text
printing
paused
```

The bed target or bed temperature must also meet the user-configured trigger:

```text
filtertemp: fan/filter threshold
hotbedtemp: heater threshold
```

If Moonraker is offline or stale, Auto mode reports `printer_not_ready` and does not request heat.

## BLE/REST status fields added

The printer status now exposes:

```text
moonraker_connected
klippy_ready
subscribed
autodetect_done
print_state
normalized_state
filename
progress
print_duration_sec
total_duration_sec
bed_temp
bed_target
active_tool
active_tool_object
active_tool_temp
active_material
active_color_rgba
u1_chamber_online
u1_chamber_object
u1_chamber_temp
cavity_fan_online
cavity_fan_object
cavity_fan_speed
last_ws_message_ms
```

## Important limitation

This firmware has not yet been built or tested on real hardware in this step. Treat it as a stronger project skeleton for later build/test.
