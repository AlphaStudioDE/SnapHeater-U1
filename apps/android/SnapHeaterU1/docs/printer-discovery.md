# Printer discovery and autonomous execution

Discovery is adapted from coroNET OS 2, by Damian Borkowski (MIT):
`src/printer/PrinterService.cpp` and `src/wifi/WifiService.cpp`.
The reference project was read only, not modified.

The Android wizard checks the last configured IPv4 endpoint, then
`_snapmaker._tcp` (Moonraker port 7125) and `_moonraker._tcp` (advertised port).
TXT IP and device_name/machine_type are supported, with resolved-address fallback.
Candidates must pass read-only `/printer/info` JSON verification.
`/printer/objects/query` checks print_stats.state, heater_bed.temperature/target,
and webhooks.state. A result with unavailable control data is labelled as such.
Authentication-required printers are not bypassed; current firmware has no
Moonraker API-key configuration.

Fallback preserves coroNET target priorities: saved endpoint, gateway, phone
neighbours +/-12, common host numbers, then a quick set of up to 60 hosts.
The final scan checks the remaining local /24 addresses (also constrained to the
actual Wi-Fi subnet). Connections bind to the phone Wi-Fi network, not cellular.
TCP timeout is 140 ms, HTTP 900 ms, mDNS discovery window 1200 ms and resolution
budget 1800 ms. Android NSD replaces the ESP-specific mDNS lifecycle/settle code.
Scan deadline is 65 seconds, response bodies are bounded, redirects disabled.
Cancellation stops discovery and further probes; an in-flight HTTP read remains
bounded by its socket/body deadlines. Manual host entry remains available.

Discovery on the phone is configuration assistance, not telemetry forwarding.
Selecting a result only fills host/port. Saving sends those settings to Panda.
After restart Panda runs its own Moonraker client independently of the phone.
Only Panda-reported fresh data unlocks printer-dependent modes.

An accepted heating job continues after phone disconnection. The five-minute
lease limits command authority, not execution: expired remote authority hands
off to local_job. OFF and revision checks remain; limits and faults still stop
heating. Reboot does not automatically resume a heating session.
