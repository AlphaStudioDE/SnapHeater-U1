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
 * hardware pin map used by SnapHeater U1. Kconfig may disable optional pins;
 * panda_hardware_guard.h rejects remapping onto another circuit. The local
 * fan driver uses this checked stock configuration.
 *
 * Safety policy:
 * - Normal heater output remains build-disabled in the public defaults.
 * - Runtime safety checks and the Output Safety Latch still guard heating.
 * - GPIO7 is the zero-cross detector net and must not be configured as a
 *   generic button.
 * - GPIO0/GPIO1 are ADC sensor inputs and should not be reused as buttons.
 * - GPIO9/GPIO8/GPIO2 are strapping pins. A button held during boot is ignored
 *   until it has first been released.
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
#define SHU1_BOARD_BUTTON_POWER_GPIO        CONFIG_SHU1_BUTTON_POWER_GPIO // GPIO9, strap
#define SHU1_BOARD_BUTTON_AUTO_GPIO         CONFIG_SHU1_BUTTON_AUTO_GPIO  // GPIO8, strap
#define SHU1_BOARD_BUTTON_ON_GPIO           CONFIG_SHU1_BUTTON_ON_GPIO    // GPIO10
#define SHU1_BOARD_BUTTON_DRY_GPIO          CONFIG_SHU1_BUTTON_DRY_GPIO   // GPIO2, strap

#define SHU1_BOARD_BUTTON_ACTIVE_LOW        CONFIG_SHU1_BUTTON_ACTIVE_LOW

// -----------------------------------------------------------------------------
// Panda Breath-style LED/backlight indicators
// -----------------------------------------------------------------------------
#define SHU1_BOARD_LED_AUTO_GPIO            CONFIG_SHU1_LED_AUTO_GPIO
#define SHU1_BOARD_LED_ON_GPIO              CONFIG_SHU1_LED_ON_GPIO
#define SHU1_BOARD_LED_DRY_GPIO             CONFIG_SHU1_LED_DRY_GPIO
#define SHU1_BOARD_LED_POWER_GPIO           CONFIG_SHU1_LED_POWER_GPIO

#define SHU1_BOARD_LED_ACTIVE_HIGH          CONFIG_SHU1_LED_ACTIVE_HIGH

// -----------------------------------------------------------------------------
// Pin map status notes
// -----------------------------------------------------------------------------
#define SHU1_BOARD_PIN_STATUS_HEATER        "dragonbreath_inferred_continuity_required"
#define SHU1_BOARD_PIN_STATUS_FAN           "dragonbreath_confirmed"
#define SHU1_BOARD_PIN_STATUS_ZERO_CROSS    "dragonbreath_confirmed"
#define SHU1_BOARD_PIN_STATUS_CHAMBER_ADC   "dragonbreath_inferred_continuity_required"
#define SHU1_BOARD_PIN_STATUS_PTC_ADC       "dragonbreath_inferred_continuity_required"
#define SHU1_BOARD_PIN_STATUS_BUTTONS       "dragonbreath_map_optional_default_off"
#define SHU1_BOARD_PIN_STATUS_LEDS          "accepted_panda_breath_map_optional_feedback"
