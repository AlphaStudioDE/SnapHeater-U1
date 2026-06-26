/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "sdkconfig.h"

#define SHU1_FW_NAME        "SnapHeater U1"
#define SHU1_FW_VERSION     "1.9.4-dev"
#define SHU1_DEFAULT_MATERIAL_MISMATCH_WARNING_ENABLED 1
#define SHU1_PROJECT_URL    "https://github.com/AlphaStudioDE/SnapHeater-U1"

#define SHU1_API_PORT       80

// ESP-IDF only defines boolean CONFIG_* symbols when they are enabled.
// These guards allow normal C expressions such as `if (CONFIG_...)` to compile
// even when a bool option is disabled in menuconfig.
#ifndef CONFIG_SHU1_ENABLE_HEATER_OUTPUT
#define CONFIG_SHU1_ENABLE_HEATER_OUTPUT 0
#endif
#ifndef CONFIG_SHU1_ENABLE_GPIO_PROBE
#define CONFIG_SHU1_ENABLE_GPIO_PROBE 0
#endif
#ifndef CONFIG_SHU1_HEATER_ACTIVE_HIGH
#define CONFIG_SHU1_HEATER_ACTIVE_HIGH 0
#endif
#ifndef CONFIG_SHU1_FAN_ACTIVE_HIGH
#define CONFIG_SHU1_FAN_ACTIVE_HIGH 0
#endif
#ifndef CONFIG_SHU1_ZERO_CROSS_GPIO
#define CONFIG_SHU1_ZERO_CROSS_GPIO -1
#endif
#ifndef CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL
#define CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL 0
#endif
#ifndef CONFIG_SHU1_AC_MAINS_HZ
#define CONFIG_SHU1_AC_MAINS_HZ 50
#endif
#ifndef CONFIG_SHU1_ZERO_CROSS_RISING_EDGE
#define CONFIG_SHU1_ZERO_CROSS_RISING_EDGE 0
#endif
#ifndef CONFIG_SHU1_FAN_TRIAC_RUN_PERCENT
#define CONFIG_SHU1_FAN_TRIAC_RUN_PERCENT 100
#endif
#ifndef CONFIG_SHU1_FAN_TRIAC_MIN_DELAY_US
#define CONFIG_SHU1_FAN_TRIAC_MIN_DELAY_US 200
#endif
#ifndef CONFIG_SHU1_FAN_TRIAC_GATE_PULSE_US
#define CONFIG_SHU1_FAN_TRIAC_GATE_PULSE_US 100
#endif
#ifndef CONFIG_SHU1_ENABLE_BLE
#define CONFIG_SHU1_ENABLE_BLE 0
#endif
#ifndef CONFIG_SHU1_BLE_REQUIRE_PIN_FOR_CONTROL
#define CONFIG_SHU1_BLE_REQUIRE_PIN_FOR_CONTROL 0
#endif


#ifndef CONFIG_SHU1_DEFAULT_FAN_POSTRUN_MIN
#define CONFIG_SHU1_DEFAULT_FAN_POSTRUN_MIN 5
#endif
#ifndef CONFIG_SHU1_DEFAULT_TEMPERING_ENABLED
#define CONFIG_SHU1_DEFAULT_TEMPERING_ENABLED 0
#endif
#ifndef CONFIG_SHU1_DEFAULT_TEMPERING_END_TEMP_C
#define CONFIG_SHU1_DEFAULT_TEMPERING_END_TEMP_C 0
#endif
#ifndef CONFIG_SHU1_DEFAULT_TEMPERING_DURATION_MIN
#define CONFIG_SHU1_DEFAULT_TEMPERING_DURATION_MIN 30
#endif


// Work modes reconstructed as user-facing behavior and reimplemented from-scratch.
#define SHU1_MODE_AUTO      1
#define SHU1_MODE_POWER_ON  2
#define SHU1_MODE_DRYING    3
#define SHU1_MODE_PREHEAT   4
#define SHU1_MODE_DRY_OUT   5
#define SHU1_MODE_HEALTH_TEST 6

#define SHU1_DRYING_PLA     1
#define SHU1_DRYING_PETG    2
#define SHU1_DRYING_ABS     3
#define SHU1_DRYING_CUSTOM  4

#define SHU1_PROFILE_CUSTOM 0
#define SHU1_PROFILE_PLA    1
#define SHU1_PROFILE_PETG   2
#define SHU1_PROFILE_ABS    3
#define SHU1_PROFILE_ASA    4
#define SHU1_PROFILE_TPU    5
#define SHU1_PROFILE_NYLON  6
#define SHU1_PROFILE_PC     7

#define SHU1_PREHEAT_IDLE       0
#define SHU1_PREHEAT_HEATING    1
#define SHU1_PREHEAT_HOLDING    2
#define SHU1_PREHEAT_COMPLETE   3

#define SHU1_CONTROL_PERIOD_MS       500
#define SHU1_SENSOR_PERIOD_MS        500
#define SHU1_MOONRAKER_RETRY_MS      5000

// Snapmaker U1 / Moonraker integration tuning inherited from coroNET field behavior.
#define SHU1_MOONRAKER_WS_RECONNECT_MS        5000
#define SHU1_MOONRAKER_WS_STALE_MS           15000
#define SHU1_MOONRAKER_HTTP_TIMEOUT_MS         200
#define SHU1_MOONRAKER_AUTODETECT_RETRY_MS    8000
#define SHU1_MOONRAKER_OBJECT_LIST_BUF       16384
#define SHU1_PRINTER_STALE_MS                22000
#define SHU1_CHAMBER_TEMP_EMA_ALPHA           0.25f

#define SHU1_HEATER_HYST_C           2.0f
#define SHU1_MIN_VALID_TEMP_C       -20.0f
#define SHU1_MAX_VALID_TEMP_C       180.0f

// Conservative local PTC safety. Original firmware evidence suggested a PTC safety region
// around ~104 C. Keep configurable and verify with real hardware.
#define SHU1_DEFAULT_PTC_CUTOFF_C   104.0f
#define SHU1_PTC_HARD_CUTOFF_C      130.0f

// Heater-abnormal from-scratch detector inspired by older vendor debug strings:
// "PTC heating start detect", "temp rise too low", "heater abnormal".
// The logic below is ours: if heat is requested for a continuous window and neither
// PTC nor chamber temperature rises enough, we fault and turn the heater off.
#define SHU1_RISE_DETECT_DELAY_MS       15000
#define SHU1_RISE_DETECT_WINDOW_MS      60000
#define SHU1_MIN_PTC_RISE_C             5.0f
#define SHU1_MIN_CHAMBER_RISE_C         1.0f

// Fan policy.
// In v0.7 this is user-configurable in minutes; the macro is only the default.
#define SHU1_DEFAULT_FAN_POSTRUN_MIN    CONFIG_SHU1_DEFAULT_FAN_POSTRUN_MIN
#define SHU1_FAN_ALWAYS_ON_WITH_HEAT    1

// Optional session watchdog for any user-started heater mode.
#define SHU1_DEFAULT_MAX_SESSION_MIN     240

// Preheat/hold mode. Hold countdown starts only after chamber reaches target.
#define SHU1_PREHEAT_REACHED_BAND_C     0.5f

// Auto-mode print-aware policy.
// Preheat, drying and manual power-on modes are intentionally independent of U1 state.
#define SHU1_AUTO_PAUSE_ERROR_HOLD_ENABLED 1
#define SHU1_DEFAULT_TEMPERING_ENABLED     CONFIG_SHU1_DEFAULT_TEMPERING_ENABLED
#define SHU1_DEFAULT_TEMPERING_END_TEMP_C  CONFIG_SHU1_DEFAULT_TEMPERING_END_TEMP_C
#define SHU1_DEFAULT_TEMPERING_DURATION_MIN CONFIG_SHU1_DEFAULT_TEMPERING_DURATION_MIN
#define SHU1_TEMPERING_IDLE       0
#define SHU1_TEMPERING_ACTIVE     1
#define SHU1_TEMPERING_COMPLETE   2

// v1.0 feature pack: user/app selected smart modes.
#define SHU1_FINISH_FAST_COOLDOWN   0
#define SHU1_FINISH_NORMAL_COOLDOWN 1
#define SHU1_FINISH_TEMPERING       2
#define SHU1_FINISH_KEEP_WARM       3

#define SHU1_PAUSE_HOLD_KEEP        0
#define SHU1_PAUSE_HOLD_LOWER       1
#define SHU1_PAUSE_HOLD_STOP_AFTER  2

#define SHU1_HEALTH_IDLE      0
#define SHU1_HEALTH_PRE_FAN   1
#define SHU1_HEALTH_HEATING   2
#define SHU1_HEALTH_COOLDOWN  3
#define SHU1_HEALTH_COMPLETE  4
#define SHU1_HEALTH_FAILED    5

#define SHU1_HEALTH_RESULT_NONE          0
#define SHU1_HEALTH_RESULT_OK            1
#define SHU1_HEALTH_RESULT_WEAK_HEATER   2
#define SHU1_HEALTH_RESULT_SENSOR_FAULT  3
#define SHU1_HEALTH_RESULT_ABORTED       4

#define SHU1_DRYOUT_DEFAULT_TEMP_C       45
#define SHU1_DRYOUT_DEFAULT_MIN          20
#define SHU1_KEEP_WARM_DEFAULT_TEMP_C    35
#define SHU1_DEFAULT_STABILITY_BAND_C     3
#define SHU1_ENERGY_HEATER_WATT          300
#define SHU1_HEALTH_TEST_MAX_TARGET_C     45
#define SHU1_HEALTH_TEST_DEFAULT_SEC     90
#define SHU1_HEALTH_MIN_PTC_RISE_C        4
#define SHU1_HEALTH_MIN_CHAMBER_RISE_C  0.5f

// v1.1 software-only virtual door/top-cover detection.
// This infers probable chamber opening from a sudden chamber-temperature drop.
// It is intentionally configurable because real U1 tests should calibrate the thresholds.
#define SHU1_VDOOR_DEFAULT_ENABLED             1
#define SHU1_VDOOR_DEFAULT_WINDOW_SEC          60
#define SHU1_VDOOR_DEFAULT_DROP_C              4
#define SHU1_VDOOR_DEFAULT_RATE_C_PER_MIN      4
#define SHU1_VDOOR_DEFAULT_MIN_BASE_TEMP_C     35
#define SHU1_VDOOR_ACTION_NOTIFY_ONLY          0
#define SHU1_VDOOR_ACTION_STOP_CONDITIONING    1
#define SHU1_VDOOR_ACTION_STOP_HEATER          2
#define SHU1_VDOOR_DEFAULT_ACTION              SHU1_VDOOR_ACTION_STOP_CONDITIONING



// v1.3 extended intelligent chamber functions.
#define SHU1_HEAT_SOAK_IDLE       0
#define SHU1_HEAT_SOAK_WAITING    1
#define SHU1_HEAT_SOAK_HOLDING    2
#define SHU1_HEAT_SOAK_COMPLETE   3
#define SHU1_DEFAULT_HEAT_SOAK_MIN        10
#define SHU1_DEFAULT_HEAT_SOAK_BAND_C      2
#define SHU1_DEFAULT_STABILITY_LOCK_MIN    3
#define SHU1_DEFAULT_FILTER_LIFE_H       120
#define SHU1_DEFAULT_HEATER_WEAR_WARN_PCT 35
#define SHU1_AIRFLOW_DEFAULT_ENABLED       1
#define SHU1_AIRFLOW_PTC_DELTA_C          45.0f
#define SHU1_AIRFLOW_WINDOW_MS         120000
#define SHU1_AIRFLOW_MIN_CHAMBER_RISE_C    0.5f
#define SHU1_PLA_PROTECTION_MAX_C         40
#define SHU1_PICKUP_OFF                    0
#define SHU1_PICKUP_KEEP_WARM_UNTIL_OPEN   1
#define SHU1_PICKUP_NOTIFY_ONLY            2
#define SHU1_RISK_LOW      0
#define SHU1_RISK_MEDIUM  50
#define SHU1_RISK_HIGH    75
#define SHU1_DEMO_IDLE      0
#define SHU1_DEMO_RUNNING   1
#define SHU1_SAFETY_SCORE_MIN_TO_UNLOCK 80


// v1.4 productization, safety and U1 Symbiont Mode.
#define SHU1_SETUP_STEP_BLE_CONNECTED      1
#define SHU1_SETUP_STEP_WIFI_CONFIGURED    2
#define SHU1_SETUP_STEP_MOONRAKER_READY    3
#define SHU1_SETUP_STEP_SENSORS_OK         4
#define SHU1_SETUP_STEP_FAN_VERIFIED       5
#define SHU1_SETUP_STEP_HEATER_VERIFIED    6
#define SHU1_SETUP_STEP_COMPLETE           7

#define SHU1_NOTIFY_INFO       1
#define SHU1_NOTIFY_WARNING    2
#define SHU1_NOTIFY_CRITICAL   3
#define SHU1_NOTIFY_ACTION     4

#define SHU1_LANG_EN 0
#define SHU1_LANG_DE 1
#define SHU1_LANG_PL 2

#define SHU1_HISTORY_SLOTS 120
#define SHU1_DEFAULT_HISTORY_SAMPLE_SEC 30

#define SHU1_OTA_IDLE          0
#define SHU1_OTA_AVAILABLE     1
#define SHU1_OTA_DOWNLOADING   2
#define SHU1_OTA_READY         3
#define SHU1_OTA_FAILED        4

#define SHU1_SYMBIONT_POLICY_READ_ONLY      0
#define SHU1_SYMBIONT_POLICY_CLIMATE_SAFE   1

#define SHU1_DEFAULT_LOCAL_ONLY_MODE        1
#define SHU1_DEFAULT_OUTPUT_LATCH_ENABLED   1

// v1.6 physical control layer: Panda Breath-style buttons and indicator LEDs.
// Unknown LED/button GPIOs are intentionally configurable and default to disabled (-1).
#ifndef CONFIG_SHU1_ENABLE_PHYSICAL_CONTROLS
#define CONFIG_SHU1_ENABLE_PHYSICAL_CONTROLS 0
#endif
#ifndef CONFIG_SHU1_BUTTON_ACTIVE_LOW
#define CONFIG_SHU1_BUTTON_ACTIVE_LOW 1
#endif
#ifndef CONFIG_SHU1_LED_ACTIVE_HIGH
#define CONFIG_SHU1_LED_ACTIVE_HIGH 1
#endif
#ifndef CONFIG_SHU1_BUTTON_AUTO_GPIO
#define CONFIG_SHU1_BUTTON_AUTO_GPIO -1
#endif
#ifndef CONFIG_SHU1_BUTTON_ON_GPIO
#define CONFIG_SHU1_BUTTON_ON_GPIO -1
#endif
#ifndef CONFIG_SHU1_BUTTON_OFF_GPIO
#define CONFIG_SHU1_BUTTON_OFF_GPIO -1
#endif
#ifndef CONFIG_SHU1_BUTTON_GENERIC_GPIO
#define CONFIG_SHU1_BUTTON_GENERIC_GPIO -1
#endif
#ifndef CONFIG_SHU1_LED_AUTO_GPIO
#define CONFIG_SHU1_LED_AUTO_GPIO -1
#endif
#ifndef CONFIG_SHU1_LED_ON_GPIO
#define CONFIG_SHU1_LED_ON_GPIO -1
#endif
#ifndef CONFIG_SHU1_LED_OFF_GPIO
#define CONFIG_SHU1_LED_OFF_GPIO -1
#endif
#ifndef CONFIG_SHU1_LED_ERROR_GPIO
#define CONFIG_SHU1_LED_ERROR_GPIO -1
#endif
#ifndef CONFIG_SHU1_LED_WIFI_GPIO
#define CONFIG_SHU1_LED_WIFI_GPIO -1
#endif
#ifndef CONFIG_SHU1_LED_BLE_GPIO
#define CONFIG_SHU1_LED_BLE_GPIO -1
#endif
#ifndef CONFIG_SHU1_PHYSICAL_DEBOUNCE_MS
#define CONFIG_SHU1_PHYSICAL_DEBOUNCE_MS 45
#endif
#ifndef CONFIG_SHU1_PHYSICAL_LONG_PRESS_MS
#define CONFIG_SHU1_PHYSICAL_LONG_PRESS_MS 2000
#endif
#ifndef CONFIG_SHU1_PHYSICAL_TASK_PERIOD_MS
#define CONFIG_SHU1_PHYSICAL_TASK_PERIOD_MS 50
#endif

#define SHU1_PHYS_BTN_NONE     0
#define SHU1_PHYS_BTN_AUTO     1
#define SHU1_PHYS_BTN_ON       2
#define SHU1_PHYS_BTN_OFF      3
#define SHU1_PHYS_BTN_GENERIC  4

#define SHU1_PHYS_LED_MODE_OFF       0
#define SHU1_PHYS_LED_MODE_SOLID     1
#define SHU1_PHYS_LED_MODE_BLINK_SLOW 2
#define SHU1_PHYS_LED_MODE_BLINK_FAST 3
