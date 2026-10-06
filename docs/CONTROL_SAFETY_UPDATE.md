# Control corrections included in experimental release 0.9.10

Reference checked 2026-10-06: DragonBreath
`f311a22c39a3e3aef92d10f0f6c3f2d5d63e02b9`, with
[PR 103](https://github.com/plastikman/DragonBreath/pull/103) and
[PR 107](https://github.com/plastikman/DragonBreath/pull/107).
Thanks to plastikman for the original Panda Breath hardware investigation.

## Changes

- Local chamber regulator now follows upstream's default 1 C ON/OFF hysteresis,
  not the old 10-second PID window and static 40%/70% approach caps. Starts with
  demand OFF; enables strictly below target minus 1 C, cuts at/above target and
  preserves demand inside the band. Inactive/invalid jobs reset demand. Foldback
  and all other safety conditions override it. No minimum dwell delays OFF.
- The 55 C target limit, 85 C chamber / 105 C element hard limits, 33k/82k
  foldback limits, held-gate fan/ZC implementation, ADC conversion and OTA
  partition layout are unchanged. We did not import experimental upstream PID
  or raise thermal limits to match individual upstream bench experiments.
- Airflow advisory observes a 120-second warm-up window across SSR OFF phases,
  requiring at least 30 seconds of applied logical ON evidence. OFF/pause,
  faults, invalid values, observation gaps and reaching the target region reset
  observation. It warns if element-minus-chamber is at least 45 C and chamber
  rise below 0.5 C. This is a heuristic warning, NOT measured airflow and NOT an
  independent airflow shutdown. Pending warnings are not repeatedly logged.
- Frozen-reading evidence is per-channel, and continuous ON qualifies without
  six SSR transitions. One changing channel cannot cancel the other's warning.
  The existing 5-minute autonomous cutoff deadline and immediate hard-fault
  handling are retained. See [sensor diagnostics](SENSOR_DIAGNOSTICS.md).

## What these changes cannot prove or repair

Software cannot establish actual fan rotation, detect every sensor giving a
plausible but wrong/noisy reading, or switch off a power component failed short.
Gate state + ZC is electrical-command evidence only. Real reset/watchdog/SSR/TRIAC
waveforms, thermal behavior and the replacement unit's board revision remain
unqualified. A safety task that is not executing cannot cut the output itself.

OTA still writes the inactive application slot, validates image structure/size/
identity and SHA-256, and excludes heating/cooling and concurrent starts. It does
not replace the stock bootloader. Application rollback flags and host simulations
do not prove that the installed stock bootloader will recover an unconfirmed
image. We do not replace a bootloader speculatively or promise stock recovery.

Reports of similar fan failures on stock firmware are evidence that a hardware
fault is possible, not proof of the cause in a particular unit.

## Verification scope

Verified locally: 114 Python unittest tests, including 67 production-control-loop
scenarios, plus the separate real-mutex OFF/maintenance/SSR-before-NVS test.
ESP-IDF 5.3.5 compiled the ESP32-C3 tester configuration successfully in a separate
build directory. Android 0.9.10 (version code 6) passed 28 unit tests and assembleDebug,
with disconnected-device preview disabled. The companion update names the
suspected sensor in the warning dialog, notifications and event history. Missing
sensor identity from older firmware is explicitly unspecified, never guessed.
The identity field is read-only and cannot change the warning deadline.

Host tests execute production policies and the control loop with fake ADC, time,
GPIO and NVS. They test logic, not energized hardware or real thermal response.
The new 0.9.10 assets include these corrections; older 0.9.9 assets are unchanged.
Read [installation and recovery](INSTALL_0.9.10.md) and the hardware risk notice.
Passing these tests does not establish safe operation on your physical device.
