# SnapHeater U1 Material Profiles

Firmware v0.5.0 adds preset material profiles. They are intentionally conservative and capped by `CONFIG_SHU1_MAX_TARGET_TEMP_C`.

| ID | Name | Chamber target | Preheat hold | Drying preset |
|---:|---|---:|---:|---|
| 0 | custom | config default | 15 min | custom |
| 1 | PLA | 45°C | 15 min | 55°C / 12 h |
| 2 | PETG | 55°C | 20 min | 60°C / 12 h |
| 3 | ABS | 60°C | 30 min | 60°C / 12 h |
| 4 | ASA | 60°C | 30 min | 60°C / 12 h |
| 5 | TPU | 40°C | 15 min | 45°C / 8 h |
| 6 | NYLON | 60°C | 45 min | 60°C / 12 h |
| 7 | PC | 60°C | 45 min | 60°C / 12 h |

## REST examples

Apply PETG profile:

```json
{"profile":"PETG"}
```

Start PETG preheat/hold using profile defaults:

```json
{"profile":"PETG","preheat_running":true}
```

Use numeric profile ID:

```json
{"material_profile":4}
```

## BLE examples

Same JSON payloads can be written to the BLE Control characteristic after unlock.

```json
{"unlock":"123456"}
```

```json
{"profile":"ASA","preheat_running":true}
```
