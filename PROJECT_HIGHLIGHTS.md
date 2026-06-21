# SnapHeater U1 — Project Highlights

## Project origin and authorship

SnapHeater U1 is the original upstream firmware framework created by **Damian Borkowski** (`@damianborkowski88`) for Snapmaker U1-focused chamber-heater development. The project is licensed under MIT and includes dedicated attribution/origin files so forks and redistributions can preserve the original project identity.


This file summarizes what makes **SnapHeater U1** different as a custom chamber-heater firmware concept for Snapmaker U1.

The goal is not only to switch a heater on and off. The goal is to provide a smart chamber-climate companion that understands printer state, material context, safety limits and user intent.

---

## 1. Snapmaker U1 integration

### Moonraker printer awareness
SnapHeater U1 is designed to connect to Snapmaker U1 through Moonraker and read printer state, progress, bed temperature, active tool and material data.

### U1 Symbiont Mode
A safe cooperation mode between SnapHeater U1 and Snapmaker U1. It allows chamber-related coordination while keeping printer-critical operations blocked.

### Material/Profile mismatch warning
Warns the user when the SnapHeater profile does not match the material reported by the printer. Heating continues; the user is informed instead of being forced into an automatic stop.

### Start Print Warning
Warns when a print starts before the chamber has reached the selected readiness conditions.

---

## 2. Chamber preparation

### Preheat / Hold
Preheats the chamber to a selected target and maintains it for a user-defined time.

### Heat Soak Timer
After the target temperature is reached, the chamber can be held for an additional soak period before being marked ready.

### Chamber Stability Lock
The chamber is considered ready only when the temperature stays inside a defined stability band for a selected time.

### Warm-Up Prediction
Estimates how long it may take to reach the target temperature based on observed heating behavior.

### Preheat Scheduler
Allows scheduled chamber preparation, such as warming the chamber before the user starts a print.

---

## 3. Material-aware behavior

### Material Chamber Guide
Provides profile-based chamber recommendations and behavior for materials such as PLA, PETG, ABS, ASA, TPU, Nylon and PC.

### PLA Protection Mode
Warns when the selected chamber target may be too aggressive for PLA.

### Anti-Warp Mode
Adjusts chamber behavior for materials and print jobs where warping is likely.

### Large Print Protection
Uses more conservative chamber behavior for long or large prints.

### Print Risk Score
Generates a simple risk indication based on material, chamber target and print context.

---

## 4. Print and post-print behavior

### Smart Pause Hold
Keeps chamber conditions stable during pauses instead of abruptly cooling the print.

### Smart Resume After Pause
Supports chamber recovery behavior after a paused print resumes.

### Tempering
Gradually lowers chamber target temperature over a user-selected time after print completion.

### Print Finish Conditioning
Provides controlled post-print behavior instead of abrupt heater shutdown.

### Post-Print Pickup Mode
Supports keep-warm and pickup-aware behavior after the print finishes.

### Keep Warm after Print
Maintains a reduced chamber temperature for delayed part pickup.

---

## 5. Software-only chamber intelligence

### Virtual Door / Open Lid Detection
Detects probable chamber or top-cover opening by analyzing sudden chamber temperature drops. This can work without an additional door sensor.

### Chamber Stabilization Score
Scores how stable the chamber temperature was during a session.

### Airflow / Blocked Filter Detection
Uses the relationship between PTC temperature and chamber temperature rise to warn about possible weak airflow, blocked filter or fan problems.

---

## 6. Diagnostics and maintenance

### Heater Health Test
Runs a short diagnostic cycle to estimate whether heating behavior appears normal.

### Heater Wear / Aging Trend
Stores and compares heating performance over time.

### Filter Life Counter
Tracks fan/filter usage and can support maintenance reminders.

### Energy Estimate
Estimates heater energy usage based on heater on-time and configured power.

### Incident Report / Fault Snapshot
Captures important runtime data when a fault occurs, making troubleshooting easier.

---

## 7. Safety and setup

### Output Safety Latch
Prevents physical heater operation until required safety conditions and setup steps are satisfied.

### First Setup Wizard
Guides the user through Wi-Fi, printer connection, sensor checks, output verification and safety readiness.

### Safety Score / Setup Validation
Summarizes system readiness and highlights missing validation steps.

### Safe Overnight Mode
A conservative mode for long or unattended print scenarios.

### GPIO Probe / staged bring-up
Provides a safer path for confirming fan, heater, buttons and LED/backlight pins before full operation.

---

## 8. User interface layers

### Physical buttons
Basic operation is available locally through AUTO, ON, OFF and ACK-style controls.

### Status LEDs / backlights
The firmware has a logical status-light layer prepared for button backlights and indicator LEDs.

### BLE advanced control
Advanced configuration is designed for a future Android app using BLE GATT commands and notifications.

### Notification levels
Events can be categorized as info, warning, critical or action-required.

### Language/event codes
Event codes are designed so a mobile app can provide localized messages.

---

## 9. Convenience and community features

### Local Recipes
Allows reusable chamber-operation presets.

### Export / Import Settings
Supports structured settings backup and sharing.

### Local-only Mode
Designed for local operation without cloud dependency.

### Temperature History Buffer
Stores recent thermal data for charts, diagnostics and stability evaluation.

### Demo / Showcase Mode
Simulates key workflows for testing, presentation and contest videos.

---

## Summary

SnapHeater U1 aims to turn a simple chamber heater into a smart, printer-aware chamber climate system for Snapmaker U1:

- printer-aware,
- material-aware,
- safety-focused,
- mobile-friendly,
- physically controllable,
- maintenance-aware,
- presentation-ready.