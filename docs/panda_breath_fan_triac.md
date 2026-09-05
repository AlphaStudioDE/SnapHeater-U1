# Panda Breath fan/TRIAC control

SnapHeater U1 follows the hardware behavior documented and implemented by
[DragonBreath](https://github.com/plastikman/DragonBreath) for original Panda
Breath V1.0/V1.0.1 electronics:

```text
GPIO3 = fan optotriac gate, HIGH means ON, LOW means OFF
GPIO7 = zero-cross input, pull-up, rising edge
```

The fan gate is a held ON/OFF level. It is **not** PWM, phase-angle control or a
short gate pulse. An ON request is applied only at the next accepted zero-cross;
an OFF request drives GPIO3 LOW immediately. Edges less than 4000 us after the
previous accepted edge are rejected as glitches. A missing zero-cross signal
makes the fan-running interlock false, so the heater SSR cannot energize.

Forbidden implementations on this board:

- fan PWM or phase chopping,
- GPTimer-delayed gate pulses,
- a plain GPIO fallback that bypasses zero-cross qualification,
- raw force-ON diagnostics.

Repository defaults keep fan and heater outputs disabled. The
`sdkconfig.panda-safe.defaults` overlay compiles the real held-gate fan path for
non-heating verification while leaving the heater output disabled. Physical
validation is still required before producing a heating-enabled build.

Source provenance: DragonBreath `docs/HARDWARE.md` and `components/pb_fan`, MIT
License. See `THIRD_PARTY_NOTICES.md`.
