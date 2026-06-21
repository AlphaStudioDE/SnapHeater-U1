# Material/Profile Mismatch Warning

SnapHeater U1 can compare the user-selected chamber profile with the material reported by Snapmaker U1/Moonraker for the active tool.

Example scenario:

- Android/SnapHeater profile: `ASA`
- Snapmaker U1 active material: `PLA`

The firmware **does not stop heating and does not cancel the print**. It only raises a warning so the Android app can notify the user.

## Why notify-only?

A mismatch may be intentional, caused by incorrect slicer metadata, or caused by a temporary tool/material report during a tool change. Automatically disabling heating could damage a print more than the mismatch itself.

## Default behavior

- `material_mismatch_warning_enabled = true`
- `auto_material_profile_enabled = false`
- mismatch creates `material_mismatch_pending = true`
- heater logic continues normally

## BLE/REST commands

Enable/disable warning:

```json
{"material_mismatch_warning_enabled":true}
```

Acknowledge Android notification:

```json
{"ack_material_mismatch":true}
```

Clear stored mismatch state:

```json
{"clear_material_mismatch":true}
```

Optional auto-apply mode, disabled by default:

```json
{"auto_material_profile_enabled":true}
```

If auto-apply is enabled, SnapHeater may apply the detected U1 material profile in AUTO mode instead of only warning. For safety and predictability, the default project behavior is warning-only.

## Android compact BLE status fields

```json
{
  "mm": true,
  "mmu": 4,
  "mmp": 1,
  "mmmsg": "Profile mismatch: SnapHeater=ASA, U1 active material=PLA. Heating continues."
}
```

Where:

- `mm` = mismatch pending
- `mmu` = user-selected SnapHeater profile id
- `mmp` = printer/U1 detected profile id
- `mmmsg` = human-readable warning
