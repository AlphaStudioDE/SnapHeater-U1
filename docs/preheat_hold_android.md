# SnapHeater U1 Preheat / Hold mode

`Preheat / Hold` is a SnapHeater-only mode for preparing the Snapmaker U1 chamber before a print.

Behavior:

1. Android app sends `preheat_running=true`, target temperature, and hold time.
2. Firmware switches to `work_mode=4` (`SHU1_MODE_PREHEAT`).
3. Heater/fan safety logic heats the chamber to `preheat_target`.
4. The hold countdown starts only after the chamber temperature reaches the target band.
5. Firmware maintains the chamber temperature for `preheat_hold_min` minutes.
6. When the hold time expires, heater is turned off, fan post-run/cooldown remains active, and `preheat_complete_pending=true` is exposed in BLE/REST status.
7. Android app should show a local phone notification when BLE status contains `pd=true` or diagnostics contains `preheat_complete_pending=true`.
8. App should acknowledge the event with `ack_preheat_complete=true` after showing the notification.

## BLE command examples

Unlock BLE control first if PIN lock is enabled:

```json
{"unlock":"123456"}
```

Start preheating to 60 C and hold it for 30 minutes:

```json
{"preheat_running":true,"preheat_target":60,"preheat_hold_min":30}
```

Start preheating to 45 C and hold it for 15 minutes:

```json
{"preheat_running":true,"preheat_target":45,"preheat_hold_min":15}
```

Stop preheating:

```json
{"preheat_running":false}
```

Acknowledge phone notification/event:

```json
{"ack_preheat_complete":true}
```

## Compact BLE status fields

Status characteristic notification is compact. Relevant preheat fields:

| Field | Meaning |
|---|---|
| `m` | Work mode; `4` means Preheat/Hold |
| `set` | Effective target temperature |
| `tc` | Chamber temperature |
| `tp` | PTC temperature |
| `ph` | Preheat phase: `0` idle, `1` heating, `2` holding, `3` complete |
| `pr` | Preheat remaining seconds once hold phase has started |
| `pd` | Preheat done pending; app should show local notification |

Example while heating:

```json
{"v":"0.4.0-dev","on":true,"m":4,"set":60,"tc":41.5,"tp":87.0,"h":true,"f":true,"err":"ok","dry":false,"rem":0,"ph":1,"pr":0,"pd":false}
```

Example while holding:

```json
{"v":"0.4.0-dev","on":true,"m":4,"set":60,"tc":60.1,"tp":96.0,"h":false,"f":true,"err":"ok","dry":false,"rem":0,"ph":2,"pr":1720,"pd":false}
```

Example after completion:

```json
{"v":"0.4.0-dev","on":false,"m":4,"set":60,"tc":59.5,"tp":72.0,"h":false,"f":true,"err":"preheat_complete","dry":false,"rem":0,"ph":3,"pr":0,"pd":true}
```

## Android notification note

BLE itself does not display a system notification on Android. The firmware only sends BLE notifications. The Android app must remain connected, usually through a foreground service, and create a local Android notification when it receives `pd=true`.

If the phone is disconnected when preheat finishes, the firmware keeps `preheat_complete_pending=true` until the app reconnects and acknowledges it.
