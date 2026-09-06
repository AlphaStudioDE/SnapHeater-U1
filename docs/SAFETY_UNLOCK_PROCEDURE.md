# Safety qualification and arming

Current user flow: select a mode and activate it. No separate arm command or
verification checkboxes are required. The firmware derives internal session
intent from an admitted work request and checks runtime conditions itself.
The hardware qualification guidance below is for engineering validation, not
an app onboarding ritual. A fault clear never starts a new work request.

This is a development firmware, not a hardware-qualified release.
Read [current safety status](SAFETY_STATUS.md) before any device work.

## No probe bypass

The legacy diagnostic pulse function rejects requests for both outputs.
Changing the GPIO probe build option does not create an authorized output path.
Do not follow older fan/heater probe instructions. Functional checks must use
the normal control policy, with all interlocks intact.

## Qualification gates

1. Compile and run [offline tests](TESTING.md) with outputs disabled for the
   initial firmware build. Compilation alone does not qualify hardware.
2. Independently verify the exact board revision, output polarity, sensor
   readings, thermal protection and safe recovery procedure.
3. Qualify fan/ZC behavior and physical airflow using appropriate isolated
   measurement equipment and a competent mains-hardware operator.
4. Qualify heater control, shutdown, reset and fault behavior under supervision,
   with independent temperature measurement and emergency disconnection.
5. Only then consider activating a normal, low-target heating session.
   Never disable firmware interlocks to make a test pass.

## Firmware arming is not a hardware certificate

Legacy verification flags are operator records, not prerequisites for activation.
They do not measure fan rotation, verify an intact TRIAC, or validate a bootloader.
A valid sensor reading and ZC signal are necessary but insufficient evidence.

## Fault recovery

OFF remains available regardless of session ownership. A hazardous fault stops
and disarms the session. Address its cause before requesting a clear. For
zero-cross loss, ZC must be present again before clearing; clearing still does
not rearm or start heating. A watchdog registration failure is inhibited for
that boot and cannot be cleared through ordinary fault reset.

Keep stock backups private. Do not rely on partition geometry alone as proof
of rollback or recovery compatibility.
