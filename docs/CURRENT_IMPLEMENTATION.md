# Current implementation — 0.9.9 testing prerelease

This source snapshot targets original Panda Breath hardware. Use matching
Android and firmware versions. Credit to plastikman / DragonBreath remains in
AUTHORS.md and THIRD_PARTY_NOTICES.md; no upstream approval is implied.

## Implemented and offline checked

- OFF cannot be revived by Keep Warm or a stale printer-completion edge.
  BLE rejects embedded raw NUL bytes before command admission. All REST routes
  close after one request without draining leftover bodies, with receive budgets
  covering slow headers too. See [release notes](RELEASE_0.9.9.md).

- JSON inputs are complete objects with at most 16 nesting levels; cJSON itself
  is built with the same depth limit. REST JSON bodies have a 2-second total
  receive budget and 1-second socket waits. Rejected/incomplete requests close
  their connection instead of draining unlimited trailing data. Network OFF is
  still not an instantaneous or hard-real-time transport guarantee.
- Started OTA uploads mark their inactive slot as pending in NVS before writes.
  Only complete SHA-256, ESP-image and project-identity validation clears the
  marker. Boot-inactive refuses a pending slot after reboot too. Factory settings
  reset does not erase these records; pre-existing stock slots remain supported.
- Physical AUTO/ON/DRY replace the prior paused/ramping workflow. Drying duration
  is capped to the configured safety session limit, also shown by Android.
- Wi-Fi retries the same runtime network every 10 seconds after the initial
  retry burst. Credentials are saved as one versioned NVS blob, not independent
  SSID/password writes. Legacy credentials remain readable. Moonraker has no
  implicit sample host; a saved/imported printer configuration is required.
- Android fences delayed control/status replies across OFF and heater selection.
  Job, resume and schedule requests retain the revision observed by the user;
  transports do not substitute a newer revision after an intervening OFF.
  The UI distinguishes RAM-active/pending settings, confirmed persistence,
  persistence failure and unavailable persistence status.
- Firmware 0.9.9 and Android 0.9.9 (version code 3) identify this change set.
  ESP-IDF 5.3.5 and WebSocket client 1.7.0 are pinned with a dependency lock.
  See [build provenance](BUILD_PROVENANCE.md); pinning is not a claim of a
  bit-for-bit reproducible release or hardware qualification.

- Safety-path logging and flash isolation: see [blocking-I/O fix](SAFETY_IO.md).
  Settings apply in RAM immediately; durable storage waits for safe idle.
  A durable session marker precedes SSR admission; interrupted heating requires
  explicit safe fault clear after reboot. See the I/O document for limitations.

- Localized Android controls, per-mode icons, saved heaters and editable names.
- Separate visual preview without physical transport/control.
- BLE Wi-Fi provisioning with scan, connection deadline and explicit errors.
- Phone-assisted printer discovery; Panda executes its own Moonraker client.
- Optional Moonraker API key and tested configuration changes without reboot.
  Panda verifies HTTP and WebSocket control data before persistence; failed
  candidates retain the previous configuration. Active/paused/scheduled jobs
  prevent configuration changes. See [connection setup](MOONRAKER_SETUP.md).
- Printer-dependent AUTO and AUTO + Tempering; standalone manual tempering.
- Shared BLE/REST job commands; preferences separate from activation.
- Preheat/hold, exact-minute drying, scheduler delay/target/hold and cancellation.
- REST token provisioning over BLE, encrypted local credential storage and
  read-only access verification.
- Accepted jobs continue after phone disconnection; OFF and fault interlocks
  remain authoritative. No automatic heating restart after a reboot.
- Virtual-door detection is advisory in all modes. The latest pending event
  survives phone disconnection in RAM, not Panda reboot.
- Firmware inactive-slot OTA exists with validation and maintenance exclusion.
- User heating pause/resume above Stop: heater off, retained job and frozen task
  clocks. OFF/fault clears pause; resume cannot extend the safety session limit.
- Phone-private temperature history with BLE/REST catch-up from the Panda ring,
  a separate History tab (2 h / 24 h), streaming CSV export and whitelisted JSON
  diagnostic report export. Invalid sensor readings are not recorded as zero.
- Android LAN OTA file picker, explicit confirmation, REST authentication,
  device-identity and slot-capacity checks, progress and returned SHA-256 check.
  No automatic retry or automatic heating after the update.
- Panda cumulative heater/filter (fan) output-runtime estimates, read over BLE
  and REST, and a once-per-device-per-day Android connection summary.
- Read-only 32-event advisory queue with boot identity and sequence numbers,
  Android receipt deduplication and optional foreground-service background
  monitoring of one explicitly selected Panda. Event codes accompany localized
  warning/completion titles. No heater commands are sent by this service.

Verification details for this local change are recorded in
[the delivery notes](LOCAL_HISTORY_PAUSE_UPDATE.md). Offline checks do not
qualify radio delivery, Android background behavior, electrical waveforms or
hardware OTA recovery.

## Partial or deferred

- Anti-Warp, Large Print Protection, Safe Overnight, recipes and Showcase
  metadata were removed, not completed. Old NVS keys are inert and not loaded.
- Legacy Smart Resume was removed (configuration, recovery marker, API/BLE
  fields and Android toggle). User heating pause/resume remains available.
  Old resume_en/resume_m NVS keys are inert; unrelated settings are preserved.
- Symbiont printer ventilation supervision is implemented and host-tested;
  actual Moonraker/top-cover integration is not yet qualified. See
  [Symbiont](SYMBIONT.md) for the curve, protocol and unavoidable network limits.
- Panda history uses a 720 x 16-byte internal-RAM ring sampled every 10 seconds
  (nominal 120 minutes, 11.25 KiB sample storage). BLE/REST synchronization is
  implemented with persistent per-boot cursors, deduplication and gap markers;
  real radio/Android integration testing remains outstanding.
- New heater usage increments count only applied
  SSR ON time with a fresh valid unrounded chamber reading >= 35 C. Filter usage
  counts applied fan ON time at all temperatures. Existing NVS totals are retained
  unchanged; old hours cannot be retroactively filtered. Energy estimates remain
  independent and include warm-up. Runtime totals are checkpointed only when
  strict maintenance admission is
  possible (idle, no active outputs or fault latch, fresh valid sensors below
  30 C). Unchanged totals are not written; failed writes retry no faster than
  once a minute. Sudden power loss can lose the unsaved current job, not merely
  five minutes. The old percentage wear indicators are not lifetime measurements.
- Background notification delivery is not guaranteed. Permission/channel
  settings, Android process management, disconnection, queue overwrite and
  Panda restart can lose events. A local receipt does not prove human reading.
- Panda does not automatically rediscover a printer after IP changes.
- NTC acquisition freshness and passive dual-raw-invariance detection are
  implemented; this is not universal detection of plausible frozen readings.
  See [sensor diagnostics](SENSOR_DIAGNOSTICS.md) for thresholds and limitations.
- Installed bootloader compatibility, interrupted OTA/stock recovery, actual
  TRIAC/SSR waveforms, reset/watchdog and loss-of-cooling behavior require
  device-specific qualification.

Older feature matrices and UI maps are historical plans, not evidence that all
listed functions are complete. No private working archives, credentials, local
build products or new firmware/APK binaries are included in this update.
