# Third-Party Notices

SnapHeater U1 is an ESP32-C3 chamber-heater firmware framework with Snapmaker U1 integration through Moonraker.

## DragonBreath

The Panda Breath hardware layer uses public hardware discoveries and selected
implementation behavior from
[DragonBreath](https://github.com/plastikman/DragonBreath) by `plastikman`,
including the documented GPIO map and its confidence labels, held-gate
fan/zero-cross model, reference-resistor
detection, NTC conversion table and thermal safety thresholds.

SnapHeater U1 explicitly acknowledges `plastikman`, the author and maintainer of
DragonBreath, as the person whose reverse-engineering, hardware investigation,
validation and public documentation made the hardware-focused revival of this
project possible. DragonBreath's published findings form the essential basis of
the revived Panda Breath hardware layer.

The current findings were checked against DragonBreath commit
`25c831c032d459eb5c573ba6b69022e242ea7bec`. The experimental direct inclusion
of upstream fan/ZC and PID-policy modules was selectively reverted.
SnapHeater uses local implementations again; this is not a claim of clean-room
authorship of all earlier adapted work. Adapted portions retain their documented
provenance (`5e2f668271121c9bb2f09c5701309ca2c411f9b3` and later audit updates).
The retained upstream MIT notice is in `docs/licenses/DragonBreath-MIT.txt`.
DragonBreath is distributed under
the MIT License. Its copyright and license remain with its author; adapted
portions in this repository retain attribution.

## dragon-core PID and OTA reference

The generic PID math in `main/dc_pid.c` and `main/dc_pid.h` is adapted from the
MIT-licensed [`dragon-core`](https://github.com/justinh-rahb/dragon-core)
component `dc_pid`, release `v0.32.0`, commit
`4e041d864763d468a50e9649807827dd83dd54bc`. Its copyright and license remain
with its author.

The upstream license is retained in [dragon-core-MIT.txt](docs/licenses/dragon-core-MIT.txt).

The SnapHeater OTA streaming/validation sequence also follows the MIT-licensed
`dc_portal` implementation at that pinned dragon-core revision. The HIL runner's
scenario substitution, nested assertions and report structure are adapted from
the MIT-licensed DragonBreath HIL tooling; its transport and command contract
were rewritten for SnapHeater REST.

## External ecosystems referenced

The project is designed to interoperate with:

- ESP-IDF / ESP32-C3 development tools,
- Moonraker / Klipper style APIs,
- Snapmaker U1 exposed printer status objects,
- Android BLE clients.

Those projects and ecosystems are not bundled as proprietary source code in this repository and remain under their respective licenses.

## Independent implementation note

The project does not include proprietary firmware source code, private keys,
certificates, NVS data or vendor-confidential files. The DragonBreath-derived
knowledge above comes from its publicly licensed source repository.

## User responsibility

Before enabling physical heater output, users must validate the target hardware, GPIO mapping, sensor readings, output polarity, fuse/protection behavior and thermal limits.
