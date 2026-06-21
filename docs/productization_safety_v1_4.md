# SnapHeater U1 v1.4 Productization & Safety Pack

This release adds the final productization layer before first hardware build tests.

## Implemented items

1. **First Setup / Commissioning Wizard** — BLE/Android guided setup states.
2. **Temperature History Buffer** — compact RAM ring buffer for chamber/PTC/target/heater/fan samples.
3. **Incident Report / Fault Snapshot** — fault snapshot for Android export/debugging.
4. **Output Safety Latch** — live heater output requires verified sensors, fan, heater and explicit runtime arming.
5. **Notification Levels** — Info, Warning, Critical and Action Required.
6. **Local-only Mode** — default privacy stance: BLE + LAN; no cloud required.
7. **Language/Event Codes** — firmware exposes event/notification codes; Android translates EN/DE/PL.
8. **OTA/Rollback placeholders** — fields exist for future safe OTA and rollback flow.
9. **Contest/Showcase Mode** — separate from demo mode; intended for Snapmaker Innovation Fund videos and app demos.
10. **U1 Symbiont Mode** — biological/cooperative naming for safe U1 cooperation through Moonraker.

## U1 Symbiont Mode

By default SnapHeater U1 observes Moonraker in read-only mode. When the user explicitly enables U1 Symbiont Mode, the firmware may later cooperate with Snapmaker U1 only in climate-related areas such as ventilation/top-cover/fan behavior, and only through a whitelist. Printer-critical operations such as pause, cancel, motion, extrusion, nozzle temperature and bed temperature remain blocked.

## API examples

```json
{"symbiont_mode_enabled":true,"symbiont_ventilation_allowed":true,"symbiont_safe_control_enabled":true,"symbiont_policy":1}
```

```json
{"heater_output_verified":true,"fan_output_verified":true,"sensors_verified":true,"moonraker_verified":true,"arm_output_safety_latch":true}
```

```json
{"first_setup_wizard_enabled":true,"temp_history_enabled":true,"incident_report_enabled":true,"local_only_mode":true}
```
