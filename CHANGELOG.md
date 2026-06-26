# Changelog

## v1.9.4-dev - TRIAC fan control baseline

- Added Panda Breath TRIAC fan control using GPIO7 zero-cross detection and GPIO3 gate pulses.
- Added Kconfig tuning for AC mains frequency, gate pulse width, phase delay and fan run percent.
- Routed normal fan requests and fan probe pulses through the TRIAC/zero-cross driver.
- Build-enabled normal heater output for the accepted Panda Breath map while keeping runtime safety checks and the Output Safety Latch.
- Exposed TRIAC fan parameters in REST and BLE diagnostics.

## v1.9.3-dev - Accepted Panda Breath firmware baseline

- Promoted the Panda Breath GPIO map from candidate wording to the accepted public firmware baseline.
- Added `CONFIG_SHU1_ZERO_CROSS_GPIO=7` and exposed zero-cross GPIO in REST/BLE diagnostics.
- Added the accepted K1/K2/K3 LED GPIO defaults while keeping physical button actions disabled by default.
- Replaced REST `hardware_pins` as the primary pin map field and kept `inferred_pins` as a deprecated compatibility alias.
- Kept normal heater output and GPIO probe disabled by default for public builds.

## v1.9.2-dev - ESP-IDF build readiness

- Refined Panda Breath GPIO verification candidates and kept shared GPIO7 disabled by default.
- Persisted Android mode settings in app state and added an explicit confirmation action.
- Expanded the Android app shell to cover the current firmware feature surface and added a firmware-to-app map.
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
