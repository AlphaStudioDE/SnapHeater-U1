# Changelog

## v1.9.2-dev - ESP-IDF build readiness

- Added mode-specific Android settings panels after selecting a workflow.
- Preserved Android app session, tab and mock state across screen rotation.
- Restored semantic status colors for Android temperature and safety values.
- Reworked the Android app palette toward black, white and orange SnapScreen-style visuals.
- Added the Android app connection entry screen with Demo mode flow.
- Added the Android Gradle Wrapper and verified `assembleDebug` with a local Android SDK.
- Added Android app build setup notes for the first local Gradle sync/build.
- Added project acknowledgements for community help and participation.
- Added a hardware liability disclaimer for Panda Breath-style and DIY heater use.
- Added a generic original-firmware restoration image and back-to-original flashing instructions.
- Split the Android app prototype into model, data, BLE, theme, component and screen layers.
- Added a SnapScreen-like mobile UI flow document and EN/PL/DE localization resource start.
- Added an iOS target placeholder so mobile work can remain organized under `apps/`.
- Added an Android companion app Compose UI prototype under `apps/android/SnapHeaterU1`.
- Added DIY reference hardware documentation with BOM, wiring diagram and validation checklist.
- Added DIY sourcing examples with budget-oriented part families and AliExpress search links.
- Added an example 24 V / 300 W DIY shopping cart with expected budget prices.
- Added a staged safety unlock procedure for GPIO probe, heater probe and normal heater output.
- Added a hardware bring-up checklist for the first physical Panda Breath-style board session.
- Verified the firmware skeleton builds for ESP32-C3 with ESP-IDF v5.3.5.
- Set the default flash size to 4 MB to match the Panda Breath-style partition layout.
- Fixed build issues around configuration defaults, temperature history sizing and compiler warnings.
- Updated ADC attenuation naming for ESP-IDF v5.3 compatibility.
- Kept normal heater output and GPIO probe disabled by default.

## v1.9.1-dev - Attribution and origin readiness

- Added AUTHORS.md, NOTICE.md, PROJECT_ORIGIN.md and BRANDING_AND_ATTRIBUTION.md.
- Added MIT SPDX headers to source files.
- Clarified project origin, upstream authorship and fork attribution guidance.
- Kept the project under MIT while documenting attribution expectations and contest/community clarity.

## v1.9.0-dev - BIN analysis and restore readiness

- Added flash backup/restore documentation.
- Added clean-room binary findings notes.
- Added BIN analysis limits and hardware-verification checklist.

## v1.8.0-dev - GitHub public package preparation

- Added GitHub-oriented README.
- Added Mermaid architecture diagrams.
- Added dual-package structure in the release ZIP: working project and public GitHub project.
- Added public-facing project highlights.
- Added repository hygiene files such as `.gitignore`.
- Removed internal research notes from the public folder.

## v1.7.0-dev - Test readiness

- Added central board configuration header.
- Added feature matrix.
- Added build and test plan.

## Earlier development milestones

The firmware skeleton includes the following major feature groups:

- Snapmaker U1 / Moonraker integration
- BLE control layer
- Physical controls and status LEDs
- Preheat, hold, drying and tempering workflows
- Material-aware chamber logic
- Anti-warp and large-print features
- Safe overnight, safety latch and setup validation
- Virtual door/open-lid detection
- Heater health, filter life and energy estimate
- Demo/showcase mode
- U1 Symbiont Mode
