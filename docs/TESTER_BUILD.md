# Experimental functional tester build

Local verification for 0.9.9: build passed; 107 unittest tests and the real-mutex
safety host test passed with the tester configuration used for the full-loop
simulation. This does not establish hardware safety or successful device testing.

The default configuration now includes heater, held-gate fan and physical-panel
support for original Panda Breath electronics. This is not a hardware-qualified
release. There is no separate manual arm/verification-checkbox workflow.
Choose a mode and activate it using the normal authenticated control channel.

Heater support enabled in the binary does not mean heating on boot.
Temperature/sensor/ZC interlocks, fault latching, lease timeouts, unconditional
OFF, watchdog checks and OTA maintenance exclusion remain active. Fault recovery
does not resume a stopped session. The target ceiling is 55 C.

An existing sdkconfig retains its old values even if sdkconfig.defaults changes.
For a reproducible build, use an isolated generated configuration:

```powershell
idf.py -B build-panda-tester -D SDKCONFIG=sdkconfig.tester -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
```

Inspect build-panda-tester/config/sdkconfig.h to confirm heater, fan and physical
control are enabled and watchdog panic/rollback are enabled. Never confuse this
image with an older build directory. Building does not flash a device.

sdkconfig.panda-safe.defaults remains an explicit heater-disabled override for
non-heating qualification work, not the functional tester distribution.

Read [known safety limits](SAFETY_STATUS.md) before device testing. Keep backups,
NVS, passwords and personal device data private. Functional success alone is not
proof of electrical safety. Do not disable interlocks to obtain a passing result.
