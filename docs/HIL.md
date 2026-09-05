# SnapHeater U1 hardware-in-the-loop tests

The HIL runner and scenario mechanics are adapted from
[`plastikman/DragonBreath`](https://github.com/plastikman/DragonBreath), credited in
[Third-Party Notices](../THIRD_PARTY_NOTICES.md). SnapHeater scenarios use its own authenticated REST/BLE-era
contract rather than pretending the DragonBreath `DBHIL` command protocol is
identical.

## Non-destructive suite

Run against a development board or an installed device with outputs kept off:

```powershell
python tools/hil.py --url http://DEVICE-IP --token YOUR_REST_TOKEN --target devboard --suite
```

For a real Panda, select `--target panda`. Its suite contains only safe-stop,
read-only pin/status/event checks and OTA inspection: no positive-heat command
and no OTA write. The runner additionally refuses any positive heat request on
Panda unless `--allow-heater` is explicitly supplied for a separately selected
qualification scenario.

The default suite verifies:

- unconditional safe stop and OFF;
- authoritative revisions, REST ownership and exact lease heartbeat;
- stale-revision and wrong-lease rejection;
- OTA status and the disabled-OTA write barrier.

Each run writes `report.json` and the complete `http.jsonl` transcript under
`hil-results/<timestamp>/`.

## Destructive development-board OTA cycle

The OTA cycle is restricted to the `devboard` target and skipped unless both an
image and explicit write authorization are supplied:

```powershell
python tools/hil.py --url http://DEVICE-IP --token web --target devboard `
  --scenario tests/hil/scenarios/ota-cycle.json `
  --firmware build/SnapHeater_U1.bin --allow-ota-write
```

It writes the selected application image to the inactive slot, observes the
authenticated response, waits for reboot, and verifies that the new application
passes its startup health gate. Do not use this scenario on a Panda containing
the only stock fallback image.

Rollback-on-crash still requires a deliberately non-healthy but structurally
valid qualification image on a disposable development board; it cannot be
safely simulated by corrupting a user's inactive stock slot.
