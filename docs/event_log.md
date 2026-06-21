# Event Log

Firmware v0.5.0 adds a small in-RAM event ring buffer.

It stores the last 32 relevant events, such as:

- boot/settings loaded
- material profile applied
- preheat complete
- drying complete
- session timeout
- fault changes
- factory reset
- device config saved

REST endpoint:

```text
GET /api/events
```

Example response:

```json
{
  "count": 4,
  "events": [
    {"seq":1,"ms":120,"level":"info","code":"boot","message":"event log initialized"}
  ]
}
```

The log is not persisted across reboot. It is meant for live diagnostics and Android app history.
