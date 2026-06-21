# SnapHeater U1 — System Diagrams

This document provides GitHub-rendered Mermaid diagrams for the SnapHeater U1 architecture, data flow, safety model and user control layers.

---

## 1. Overall architecture

```mermaid
flowchart LR
    Android[Android phone
BLE advanced control] <-->|BLE GATT
status + commands| SH[SnapHeater U1
ESP32-C3 firmware]

    Buttons[Physical buttons
AUTO / ON / OFF / ACK] -->|quick actions| SH
    LEDs[Button backlights
status LEDs] <-->|mode + warning feedback| SH

    Chamber[Chamber NTC sensor] -->|temperature| SH
    PTC[PTC heater sensor] -->|safety temperature| SH
    Heater[PTC heater driver] <-->|safe output| SH
    Fan[Fan / filter driver] <-->|cooling + airflow| SH

    U1[Snapmaker U1
Moonraker / Klipper] <-->|Wi-Fi LAN
read state + optional safe chamber cooperation| SH
```

---

## 2. Data flow between SnapHeater U1 and Snapmaker U1

```mermaid
sequenceDiagram
    participant U as Snapmaker U1 / Moonraker
    participant S as SnapHeater U1
    participant A as Android App
    participant H as Heater/Fan Hardware

    S->>U: Connect to Moonraker WebSocket
    S->>U: Subscribe to printer state, progress, bed, tools, material
    U-->>S: Status updates
    A->>S: BLE command: profile, preheat, tempering, Symbiont Mode
    S->>S: Evaluate safety latch and chamber rules
    S->>H: Control heater/fan outputs when allowed
    S-->>A: BLE status, warnings, events, history
    opt U1 Symbiont Mode enabled
        S->>U: Optional whitelisted chamber/ventilation cooperation command
    end
```

---

## 3. Control layers

```mermaid
flowchart TD
    User[User] --> Phone[Android app]
    User --> Buttons[Physical controls]

    Phone --> Advanced[Advanced functions
profiles, preheat, drying, tempering, recipes, diagnostics]
    Buttons --> Basic[Basic functions
AUTO, ON, OFF, ACK, emergency safe-off]

    Advanced --> Core[SnapHeater U1 chamber logic]
    Basic --> Core
    Core --> Safety[Safety layer
sensors, latch, limits, timeouts]
    Safety --> Outputs[Heater, fan, LEDs, events]
```

---

## 4. Preheat + heat soak workflow

```mermaid
flowchart TD
    Start[User starts preheat] --> Heat[Heat chamber to target]
    Heat --> Reached{Target reached?}
    Reached -- no --> Heat
    Reached -- yes --> Stable{Stability lock satisfied?}
    Stable -- no --> Heat
    Stable -- yes --> Soak[Optional heat soak timer]
    Soak --> Ready[Ready notification to phone]
```

---

## 5. Tempering workflow

```mermaid
flowchart TD
    Finish[Print finished] --> Enabled{Tempering enabled by user?}
    Enabled -- no --> Cooldown[Normal fan post-run / cooldown]
    Enabled -- yes --> Capture[Capture current chamber temperature]
    Capture --> Ramp[Compute gradual target ramp over selected duration]
    Ramp --> Hold[Lower target smoothly]
    Hold --> Done{Time finished?}
    Done -- no --> Hold
    Done -- yes --> Off[Heater off + fan post-run + completion event]
```

---

## 6. Virtual door / top-cover detection

```mermaid
flowchart TD
    Post[Post-print / tempering / keep-warm state] --> Monitor[Monitor chamber temperature trend]
    Monitor --> Drop{Rapid temperature drop?}
    Drop -- no --> Continue[Continue current conditioning]
    Drop -- yes --> Event[Probable chamber opened event]
    Event --> Action[Stop conditioning or keep-warm action]
    Event --> Notify[Notify Android app]
```

---

## 7. U1 Symbiont Mode safety boundary

```mermaid
flowchart TD
    Read[Always allowed:
read U1 status via Moonraker] --> Logic[SnapHeater chamber logic]
    Logic --> Self[Control own heater/fan/LEDs]
    Logic --> Optional{U1 Symbiont Mode enabled?}
    Optional -- no --> Stop[No writes to U1]
    Optional -- yes --> White[Only whitelisted chamber/ventilation cooperation]
    White --> Blocked[Blocked by design:
pause, cancel, motion, extrusion, nozzle temp, bed temp]
```

---

## 8. First setup and safety validation

```mermaid
flowchart TD
    Setup[First Setup Wizard] --> WiFi[Configure Wi-Fi]
    WiFi --> U1[Connect to Snapmaker U1]
    U1 --> Sensors[Validate chamber and PTC sensors]
    Sensors --> Fan[Test fan output]
    Fan --> Heater[Test heater output only in safe staged mode]
    Heater --> Score[Compute setup safety score]
    Score --> Ready{Safety latch ready?}
    Ready -- no --> Locked[Keep heater output locked]
    Ready -- yes --> Armed[Allow staged runtime operation]
```
