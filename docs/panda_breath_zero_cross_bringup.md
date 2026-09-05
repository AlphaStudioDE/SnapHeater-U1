# Panda Breath Zero-Cross Bring-Up

This note documents the SnapHeater U1 observation-only zero-cross test for
BIQU/BIGTREETECH Panda Breath V1.0.1 hardware.

## Hardware Assumption

Accepted map:

- `GPIO7` = `IO07/ZERO`, zero-cross detector input.
- `GPIO3` = TRIAC fan gate path.
- `GPIO18` = PTC heater relay drive.
- `GPIO0` = TH0 chamber/warehouse ADC.
- `GPIO1` = TH1 PTC ADC.
- `GPIO19` = 33k/82k NTC reference-resistor strap.

The zero-cross input is expected to come from the TLP785 optocoupler path.
GPIO7 is dedicated to zero-cross detection and is not a panel-button input.

## What Firmware Reports

`GET /api/status` includes:

```json
{
  "runtime": {
    "zero_cross_edges": 12345,
    "zero_cross_rejected_edges": 2,
    "zero_cross_edges_per_sec": 100,
    "zero_cross_last_period_us": 10000,
    "zero_cross_min_period_us": 9900,
    "zero_cross_max_period_us": 10100,
    "zero_cross_last_edge_ms": 123456,
    "zero_cross_signal_present": true
  }
}
```

On 50 Hz mains, a healthy detector should normally report about `100`
edges/second if both half-cycles are visible. A one-edge-per-cycle detector may
report about `50` edges/second.

## Safe Test Rules

- Do not connect mains while the board is open unless the device is physically
  secured and you are prepared for live AC hazards.
- Do not touch the PCB, probes, connectors or wiring while mains is present.
- Do not enable `/api/probe` for this test.
- Do not command `work_on=true`.
- Do not arm the Output Safety Latch.
- Stop if heater or fan output reports on unexpectedly.

## Pass Criteria

The zero-cross stage passes when:

- `zero_cross_signal_present=true` with mains present.
- `zero_cross_edges_per_sec` is stable near the expected mains-derived rate.
- `zero_cross_last_period_us` is stable near `10000` us on 50 Hz mains, or
  near `8333` us on 60 Hz mains, when both half-cycles are visible.
- Chamber and PTC ADC readings remain plausible.
- Heater and fan outputs remain off.

Edges closer than 4000 us to the last accepted edge are counted as rejected
glitches and do not qualify fan startup.

Only after this should fan TRIAC output testing be considered.
