# Task-based daily UI

## Disconnected visual preview

The connection screen offers an explicitly labeled app preview. This has its own
sample state and returns before the normal app creates repositories or polling
effects. It has no BLE/REST imports or command callbacks into the connected app.
Task activation changes only sample state. Leaving discards the samples; they
are never copied into device settings. Preview cannot be entered during an active
connection attempt or scan. It does not restore demo behavior in firmware.

## Connected interface

Confirmed OFF is labeled Heating off, with no active target on the dashboard.
If the fan is still requested, the subtitle indicates cooling. Pending OFF is
displayed separately until a response; a failed command does not pretend the
heater stopped. Saved target settings are retained. REST/BLE work-on state takes
precedence over the retained mode number when interpreting a stopped session.

Every action button has an icon and a text label. Task cards and their activation
buttons share a vector: thermometer-up, thermometer, filament spool with heat,
3D printer, or thermometer-down. See [the vector gallery](mode-icons.svg).
Decorative icons have no separate spoken label; adjacent button text supplies it.
Icon-only device switching retains its localized accessibility description.

The primary navigation contains Status, Modes and Settings. Safety checks,
diagnostics and advanced controls are reachable from Settings, not separate
daily tabs. A heating-OFF action remains available in the connected app header.

Status emphasizes chamber temperature, target and reported mode. It does not
present the heuristic safety score as proof of safety. Lost communication hides
the current temperature rather than presenting an old reading as live.

Modes first presents task cards. Selecting a card does not send a command or
change the live device snapshot. The task editor keeps local saved-state drafts
while telemetry updates. It shows temperature and only the relevant duration.
Temperature selection is limited to 30–55 C.

The task-specific activation action sends settings through the existing
repository contract. Pending, rejected and confirmed commands remain distinct;
OFF is still available when positive heating commands are blocked.

Normal activation now needs no separate arm action: firmware derives session
intent from the admitted work request and applies live runtime interlocks.
The app does not require an already-armed session to submit that request.
This does not enable build-disabled outputs or auto-certify hardware. Persistent
faults still block new work. Existing advanced settings remain available.

EN, PL and DE daily copy is included. Source-contract tests do not replace
Compose interaction, accessibility or real-phone visual verification.
