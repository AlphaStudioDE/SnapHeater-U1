# Panda history synchronization

Implemented locally on 2026-09-06. Requires matching Android and firmware;
not a hardware/radio qualification or a publication.

## Transport

- REST: GET /api/history?after=123. Missing cursor means zero. Only decimal
  unsigned 32-bit cursors are accepted. Malformed input returns 400; an
  unavailable recorder/allocation failure returns 503. It is read-only and
  has the same LAN visibility as status. Responses use Cache-Control: no-store.
- BLE: characteristic 7b2f1004-4a6f-4c2d-9a1e-2f4f53485531. Write exactly
  four unsigned little-endian cursor bytes **with response**, then read JSON.
  This write selects data only: it does not unlock controls, obtain a lease
  or change any actuator. A successful cursor write freezes the whole response
  for subsequent long-read fragments. Memory is released on another query or
  disconnect. Querying requires the current connection handle.
- Every page contains at most 32 records and fits the existing 4096-byte GATT
  read limit. The frozen BLE page is extra bounded RAM, separate from the
  11,520-byte ring storage, task stack and JSON serialization allocations.

Response fields:

| Field | Meaning |
| --- | --- |
| device_id | Full 12-hex-digit Panda identity; never the short display suffix |
| boot | Random 16-hex-digit recorder boot identity |
| now_s | Device uptime at the page snapshot |
| first / last | Oldest / newest retained sequence; empty ring is first=1,last=0 |
| next | Cursor after this page, not necessarily the newest retained sequence |
| gap | Requested earlier samples have already been overwritten |
| samples | Arrays: [sequence, uptime_s, chamber_C, ptc_C, target_C, flags, fault] |

Temperatures may be null. Flags: bit 0 heater output, bit 1 fan output, bit 2
user pause. OFF/pause target is zero. A bad sensor status is serialized as null,
not a believable zero-temperature measurement. Fault is the firmware enum.
after > last resets the firmware query to zero; Android also checks boot ID,
so an old boot cursor cannot silently skip the early part of a new boot.

## Android storage and recovery

The version-2 SQLite schema preserves the old phone-only table. New samples
are keyed by (device_id, boot, sequence). The cursor and all rows/gaps of one
page commit in the same transaction. Interrupted/failed writes do not advance
the cursor. A stale in-flight request cannot overwrite a newer saved cursor.

Each boot has one fixed wall-clock anchor, estimated from phone request time,
round-trip midpoint and device uptime. Reconnects never shift already saved
measurements. These timestamps are estimates, not a synchronized Panda RTC;
clock changes and unusual network delay can distort alignment between boots.

On a boot mismatch Android fetches from zero before ingesting the new session.
Boot boundaries, missing overwritten ranges and invalid sensor values are
recorded explicitly. Unknown loss at the end of the previous boot is marked
as a restart, not assigned a fabricated count. Overwritten samples cannot be
recovered from Panda. The chart does not connect lines across boots, sequence
gaps, null readings or intervals longer than 30 seconds.

Old phone rows remain stored; Panda data is authoritative over synchronized
boot coverage, where overlapping old phone samples are hidden from graph/CSV
to avoid double plots. Outside that coverage old phone data remains visible.
For old firmware without history support, live phone sampling remains a
fallback until the first successful history page for that device.

One page is fetched between foreground control polls (at most two requests on
boot change). Catch-up deliberately does not run an unbounded fetch loop.
GATT reads are serialized per device; control and status requests can cancel
an advisory history read. Synchronization never sends heating commands or
heartbeats. When the app is backgrounded, the existing optional service reads
events, not history; history is filled on return/reconnect within the ring's
retention window. App death/restart resumes from the saved cursor.

The History tab shows catch-up status and loss/restart markers. CSV includes
sample and gap rows, boot/sequence identities, null sensor cells and known
missing counts (-1 means unknown at reboot). The latest eight markers are
listed on screen; CSV includes all stored markers.

## Verification

Local run: 68 Python tests and 12 Kotlin tests passed, along with the
real-mutex safety host test, Android APK build and ESP-IDF tester build.
Android lint completed with zero errors and 129 warnings.

Kotlin tests exercise strict page validation, replay identity, null readings,
contiguous paging, overwrite, empty pages, changed boot with a nonzero cursor,
wrong-device rejection and clock anchoring. Desktop SQLite tests execute the
production DDL and SELECT/export SQL, including deduplication, legacy overlap,
gap/null export and transaction rollback. These are not Android SQLite
instrumentation tests.

C tests execute the production recorder serializer and HTTP cursor parser,
check ring byte budget/overwrite/read limit and rejection of invalid cursors.
The broader firmware regressions cover OFF, faults, OTA maintenance and pause.
No real Panda was flashed or actuated for these checks; actual BLE/LAN transfers,
Android lifecycle/storage interruptions and power-cycle behavior need a device
integration test.
