# A chamber heater that understands the print

## The idea

Printing is a sequence, not a temperature slider. Prepare the chamber. Start the
job. Keep the desired conditions. Finish deliberately. Know what happened.

SnapHeater U1 connects those steps using original Panda Breath electronics,
an Android companion and the Snapmaker U1's Moonraker interface. The phone sends
intent; Panda remembers the accepted job and keeps executing it locally.
That separation is central: putting the phone away must not remove the heater's
ability to observe print completion.

## What the project contributes

- A task-oriented Android interface, including multiple named heaters.
- A setup flow joining the heater to Wi-Fi before printer discovery/configuration.
- U1-aware AUTO and optional post-print tempering, plus independent local modes.
- An optional chamber-ventilation supervisor without installing code on the printer.
- A local history pipeline: a small rotating Panda buffer, phone-side catch-up,
  visible unrecoverable gaps and portable exports.
- Open control boundaries, documented limits and executable regression tests.

The ambition is less manual coordination and better visibility, not a claim of
more watts, higher safe temperatures or already-proven print-quality gains.
Comparisons against stock firmware should measure workflow effort, temperature
behavior and repeatability under matched conditions. No such benchmark is claimed
in this prerelease.

## Why continued development matters

An accessory becomes more useful when its behavior is inspectable and its data
is available. Open development lets makers reproduce defects, contribute language
and UI improvements, validate hardware combinations and share useful measurements.
The highest-priority next investment is supervised device qualification and
recovery testing, followed by real-world usability feedback and documentation.

## Snapmaker U1 Innovation Fund brief

This public project brief can support the author's intended submission to the
[Snapmaker U1 Innovation Fund](https://www.snapmaker.com/en-GB/innovation-fund).
The program describes community-built software, hardware and workflows extending
the U1 ecosystem. SnapHeater is an independent community accessory project in
that spirit; this page does not claim submission, acceptance, an award or endorsement.

**Short pitch:** SnapHeater U1 turns existing Panda Breath chamber-heater hardware
into a locally autonomous, printer-aware accessory with a native Android control
experience, post-print workflows and recoverable temperature history. Its open
implementation makes both capabilities and limitations inspectable, inviting
the U1 community to help validate and improve it.

## Credit and readiness

The revival rests on [plastikman's DragonBreath work](https://github.com/plastikman/DragonBreath).
Keep that credit in presentations and derivative materials. SnapHeater's app and
U1 workflows are its own contribution, not a claim to have discovered the Panda
hardware behavior independently.

Version 0.9.9 is **experimental**. Testing requires continuous supervision and
acceptance of the [risk notice](HARDWARE_LIABILITY_DISCLAIMER.md). Passing software
tests is not proof of electrical safety or readiness for unattended use.
