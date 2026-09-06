# SnapHeater U1 build and test plan

> **0.9.9 packaged prerelease:** use [installation instructions](docs/INSTALL_0.9.9.md)
> for release assets. The source build is not an electrical qualification.

Target: original Panda Breath ESP32-C3 electronics, not AirGuard 300.
This source update is not a hardware-qualified binary release.

## Build and offline verification

Use ESP-IDF 5.3.5. Repository defaults now enable the experimental functional
tester hardware path; see [tester build instructions](docs/TESTER_BUILD.md).
See [offline testing](docs/TESTING.md) for both compile
configurations and the regression suite.

Do not flash the heater-enabled compile-test configuration merely because its
build and simulations pass. The optional sdkconfig.panda-safe.defaults profile
compiles the fan/front-panel implementation with the heater disabled; it is not
an all-outputs-disabled profile.

## Hardware qualification

Read [current safety status](docs/SAFETY_STATUS.md),
[qualification and arming](docs/SAFETY_UNLOCK_PROCEDURE.md), and the
[hardware checklist](docs/HARDWARE_BRINGUP_CHECKLIST.md).

The hardware map follows the credited DragonBreath findings: SSR GPIO18,
held fan gate GPIO3, ZC GPIO7, NTC GPIO0/GPIO1 and Rref strap GPIO19.
The upstream inferred/confirmed confidence distinction is retained in
[GPIO notes](docs/gpio_verification.md). No phase-angle or PWM fan experiment
should be restored.

The old probe API rejects diagnostic pulse commands. Do not use older probe
unlock instructions. Use only the normal guarded control policy for qualified,
supervised functional checks.

## Release gates

- Actual TRIAC/SSR waveform and reset/watchdog qualification.
- Sensor and loss-of-cooling fault qualification with independent measurement.
- Interrupted OTA, installed-bootloader compatibility and stock recovery checks.
- BLE/REST lease and heartbeat behavior on real devices.
- Independent thermal protection; no unattended heater qualification.

The public regression suite and HIL tooling are available, but HIL is not an
automatic build step. See [HIL instructions](docs/HIL.md). Private dumps, local
reports and device credentials must never be committed or attached to releases.
