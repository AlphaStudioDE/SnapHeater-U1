# SnapHeater U1 v1.9.2-dev - ESP-IDF build readiness

This update records the first successful public firmware skeleton build.

## Build baseline

- Target: ESP32-C3
- ESP-IDF: v5.3.5
- Flash size: 4 MB
- Partition layout: Panda Breath-style dual OTA layout
- Heater output default: disabled
- GPIO probe default: disabled

## Changes

- Set `sdkconfig.defaults` to use a 4 MB flash size.
- Fixed a compiler warning in the AUTO context state update.
- Ensured shared application configuration is available where runtime state uses it.
- Included generated ESP-IDF configuration before local fallback defaults.
- Updated ADC attenuation naming for ESP-IDF v5.3.

## Safety note

This build result does not validate physical heater, fan, sensor or GPIO behavior.
Hardware tests must still follow the staged bring-up plan with heater output locked.
