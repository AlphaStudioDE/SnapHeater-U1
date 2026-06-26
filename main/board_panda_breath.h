/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

/*
 * SnapHeater U1 - Panda Breath board map
 *
 * This file is the single human-readable place for the accepted Panda Breath
 * hardware pin map used by SnapHeater U1. Values are intentionally routed
 * through Kconfig so they can be changed from `idf.py menuconfig` without
 * touching feature code.
 *
 * Safety policy:
 * - Normal heater output is build-enabled for the accepted Panda Breath map.
 * - Runtime safety checks and the Output Safety Latch still guard heating.
 * - GPIO7 is the zero-cross detector net and must not be configured as a
 *   generic button.
 * - GPIO0/GPIO1 are ADC sensor inputs and should not be reused as buttons.
 * - Physical button semantics are disabled by default until the exact K1/K2/K3
 *   behavior is handled in firmware.
 * - This header is a project organization layer. It does not replace safe
 *   bench testing before energizing a 300 W heater.
 */

#include "app_config.h"

// -----------------------------------------------------------------------------
// Accepted Panda Breath core outputs / inputs
// -----------------------------------------------------------------------------
#define SHU1_BOARD_HEATER_GPIO              CONFIG_SHU1_HEATER_GPIO       // GPIO18
#define SHU1_BOARD_FAN_GPIO                 CONFIG_SHU1_FAN_GPIO          // GPIO3
#define SHU1_BOARD_ZERO_CROSS_GPIO          CONFIG_SHU1_ZERO_CROSS_GPIO   // GPIO7
#define SHU1_BOARD_BUTTON_GPIO              CONFIG_SHU1_BUTTON_GPIO       // legacy generic button, disabled by default
#define SHU1_BOARD_STATUS_LED_GPIO          CONFIG_SHU1_STATUS_LED_GPIO   // unknown / optional

#define SHU1_BOARD_CHAMBER_ADC_CH           CONFIG_SHU1_CHAMBER_ADC_CH    // ADC1_CH0 / GPIO0
#define SHU1_BOARD_PTC_ADC_CH               CONFIG_SHU1_PTC_ADC_CH        // ADC1_CH1 / GPIO1

#define SHU1_BOARD_HEATER_ACTIVE_HIGH       CONFIG_SHU1_HEATER_ACTIVE_HIGH
#define SHU1_BOARD_FAN_ACTIVE_HIGH          CONFIG_SHU1_FAN_ACTIVE_HIGH

// -----------------------------------------------------------------------------
// Panda Breath-style physical buttons
// -----------------------------------------------------------------------------
#define SHU1_BOARD_BUTTON_AUTO_GPIO         CONFIG_SHU1_BUTTON_AUTO_GPIO
#define SHU1_BOARD_BUTTON_ON_GPIO           CONFIG_SHU1_BUTTON_ON_GPIO
#define SHU1_BOARD_BUTTON_OFF_GPIO          CONFIG_SHU1_BUTTON_OFF_GPIO
#define SHU1_BOARD_BUTTON_GENERIC_GPIO      CONFIG_SHU1_BUTTON_GENERIC_GPIO

#define SHU1_BOARD_BUTTON_ACTIVE_LOW        CONFIG_SHU1_BUTTON_ACTIVE_LOW

// -----------------------------------------------------------------------------
// Panda Breath-style LED/backlight indicators
// -----------------------------------------------------------------------------
#define SHU1_BOARD_LED_AUTO_GPIO            CONFIG_SHU1_LED_AUTO_GPIO
#define SHU1_BOARD_LED_ON_GPIO              CONFIG_SHU1_LED_ON_GPIO
#define SHU1_BOARD_LED_OFF_GPIO             CONFIG_SHU1_LED_OFF_GPIO
#define SHU1_BOARD_LED_ERROR_GPIO           CONFIG_SHU1_LED_ERROR_GPIO
#define SHU1_BOARD_LED_WIFI_GPIO            CONFIG_SHU1_LED_WIFI_GPIO
#define SHU1_BOARD_LED_BLE_GPIO             CONFIG_SHU1_LED_BLE_GPIO

#define SHU1_BOARD_LED_ACTIVE_HIGH          CONFIG_SHU1_LED_ACTIVE_HIGH

// -----------------------------------------------------------------------------
// Pin map status notes
// -----------------------------------------------------------------------------
#define SHU1_BOARD_PIN_STATUS_HEATER        "accepted_panda_breath_map"
#define SHU1_BOARD_PIN_STATUS_FAN           "accepted_panda_breath_map"
#define SHU1_BOARD_PIN_STATUS_ZERO_CROSS    "accepted_panda_breath_map"
#define SHU1_BOARD_PIN_STATUS_CHAMBER_ADC   "accepted_panda_breath_map"
#define SHU1_BOARD_PIN_STATUS_PTC_ADC       "accepted_panda_breath_map"
#define SHU1_BOARD_PIN_STATUS_BUTTONS       "disabled_until_button_semantics_are_supported"
#define SHU1_BOARD_PIN_STATUS_LEDS          "accepted_panda_breath_map_optional_feedback"
