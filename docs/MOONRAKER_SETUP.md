# Tested Moonraker configuration

The Android printer wizard accepts host, port and an optional masked API key.
The key remains in transient UI/request memory, not phone preferences, saved
instance state, status responses or diagnostic exports. Re-enter it when editing
a protected connection. An empty key explicitly selects unauthenticated access.

Panda performs the connection test itself. It checks `/server/info`, then the
printer object query for `print_stats.state`, finite bed temperature/target and
`webhooks.state=ready`. Only after this probe does it switch WebSocket clients.
It requires a subscription and fresh complete control data on the new socket
before saving the candidate. A reachable IP alone is insufficient.

## Dedicated BLE / REST command

Use the authenticated settings command, with the current control revision:

```json
{
  "expected_revision": 42,
  "printer_setup": {
    "id": "0123456789abcdef0123456789abcdef",
    "host": "printer.local",
    "port": 7125,
    "api_key": ""
  }
}
```

An optional `lease_id` is accepted. Do not mix job, Wi-Fi or other settings into
this command. Legacy direct `moonraker_host`, `moonraker_port` and
`moonraker_api_key` writes are rejected. Host is a DNS name or IPv4 address, not
a URL/path; maximum 63 characters. Port is an integer 1–65535. Key permits at
most 128 ASCII letters, digits, hyphens or underscores. ID is 32 hex characters.

Status includes `printer_setup: {id, phase, key_set}` without the secret.
In-progress phases are `testing`, `connecting`, `saving`; success is `succeeded`.
Failures include `auth_failed`, `unreachable`, `printer_not_ready`,
`missing_data`, `websocket_failed`, `storage_failed` and `client_stop_failed`.
Match the operation ID; another operation's success is not confirmation.

## Exclusion and persistence

Admission rejects active jobs, paused jobs, scheduled starts, active outputs
and an existing maintenance operation. The reservation blocks new heating and
conflicting configuration/OTA requests across transports. Thermal protection
and OFF remain available. Network waits do not hold the control-policy mutex.

Failed HTTP probes leave the old socket and NVS untouched. Failed WebSocket
verification restores the old client configuration. A failed save attempts to
restore the old record; failed rollback or client teardown invokes heating
inhibition. Restoration does not guarantee that the previous printer is online.
Host/port/key are stored together as the versioned `mk_config` NVS blob. Legacy
host/port are read only when the new blob is absent; a malformed new blob fails
closed instead of silently selecting a legacy printer. Factory reset erases it.

Android waits up to 45 seconds without replaying the write. Transport loss or
timeout is an uncertain outcome, not proof of rollback: Panda continues the
operation independently. Reconnect and inspect status. SDK client teardown can
wait longer in a pathological failure; maintenance must not be released merely
because the phone timed out.

## Security and validation limits

The key uses `X-Api-Key` on HTTP requests and the WebSocket handshake, following
[Moonraker authorization](https://moonraker.readthedocs.io/en/latest/external_api/authorization/).
HTTP redirects are disabled. Discovery does not broadcast the key to scanned
hosts. Use only a trusted LAN: the current HTTP/WS transport is not encrypted,
and this feature does not add NVS encryption. This key can grant broad printer
access, not just read-only telemetry. No automatic printer IP rediscovery is
introduced.

Host simulations cover validation, job exclusion, failed authentication,
WebSocket verification and persistence rollback. Builds and host tests are not
real BLE/LAN tests with a protected Moonraker, nor power-loss qualification of
NVS or electrical/hardware safety certification.
