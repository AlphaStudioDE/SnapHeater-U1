# Public safety status — September 2026

SnapHeater U1 targets original Panda Breath electronics, not AirGuard 300.
This is a development update, not a hardware-qualified or certified release.
Do not treat successful compilation or simulated tests as permission to energize
a mains-powered heater. Repository defaults keep heater and fan control disabled.

## Foundation and attribution

The revival depends on the public Panda Breath discoveries of
[plastikman / DragonBreath](https://github.com/plastikman/DragonBreath).
The reviewed reference is pinned in [Third-Party Notices](../THIRD_PARTY_NOTICES.md);
this is not a claim of continuous synchronization with upstream.
SnapHeater retains its own U1 workflows and local control implementation.
Neither identical hardware assumptions nor attribution imply upstream approval
or a guarantee that SnapHeater behaves identically to DragonBreath.

## Current safeguards

- Held active-high fan gate, zero-cross-qualified ON, immediate commanded OFF;
  no phase-angle gate pulses. Heater GPIO18, fan GPIO3, zero-cross GPIO7.
- Sensor validity checks, instantaneous 85 C chamber / 105 C PTC hard limits,
  board-dependent PTC foldback, and a 55 C target ceiling.
- Explicit output arming, hazardous-fault latch and SSR cutoff before fault NVS
  writes. A failed persistence operation retains the RAM block and is retried.
- REST, BLE, physical controls and printer-driven workflows share arbitration.
  Control channels remain available; conflicting commands cannot independently
  own an active session. Leases/revisions protect commands; OFF has priority.
- OTA maintenance excludes competing starts. The inactive app slot is validated
  before boot selection; application builds require rollback support.
- Watchdog registration failures inhibit heating for that boot.
- No production demo state or direct heater-probe bypass.

## Zero-cross loss and recovery

After valid ZC has been seen during an armed session, its loss stops and disarms
heating and latches a fault. Returning ZC does not restart heating; cooling remains
requested. Clearing the ZC fault requires returned ZC and the existing stopped,
valid-sensor/temperature checks. Clearing does not arm or start a new session.
Old clear requests cannot clear a newly raised fault.

The existing 100 ms presence timeout is also used to record gaps between accepted
edges, so signal recovery between control ticks does not erase a detected gap.
This is not a 100 ms cutoff guarantee: software reaction occurs in the control
task (nominally every 500 ms). ZC is not fan tachometry or proof of airflow.

## Verification and unresolved limits

Offline verification includes 34 unittest tests, with 29 production-control-loop
simulation scenarios, and a separate real-mutex safety host test. ESP-IDF 5.3.5
builds for ESP32-C3 have passed in default and hardware-path compile configurations.
Android `assembleDebug --offline` also passed.
See [testing instructions](TESTING.md). No energized-device qualification is
claimed by these results.

Known limits remain:

- Plausible frozen sensor values near the target are not diagnosed reliably.
- Real TRIAC/SSR waveforms, resets, watchdog behavior and loss of cooling need
  physical qualification with independent protection.
- SDK rollback simulation does not establish that an installed stock bootloader
  supports rollback. Stock recovery and interrupted OTA need device-specific
  verification; partition geometry alone is insufficient.
- A failed NVS write cannot guarantee fault retention across loss of power.
- Android transport compilation does not prove real-device BLE timing or OTA.

Independent thermal protection and safe mains-work practices remain necessary.
No flash dumps, device credentials, working audit archives or binary release
images accompany this source update.
