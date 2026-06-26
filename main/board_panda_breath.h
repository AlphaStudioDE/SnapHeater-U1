/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

/*
 * SnapHeater U1 - Panda Breath board map
 *
 * This file is the single human-readable place for all currently known or
 * planned Panda Breath hardware connections.  Values are intentionally routed
 * through Kconfig so they can be changed from `idf.py menuconfig` without
 * touching feature code.
 *
 * Safety policy:
 * - Heater/fan/ADC pins below are candidate mappings and must be verified on a
 *   real PCB before live heater output is enabled.
 * - GPIO7 is treated as a zero-cross detector candidate shared with K1 button
 *   behavior; do not implement it as a simple button until pulse-width handling
 *   is validated.
 * - Detailed button/backlight pins are deliberately left as placeholders. They
 *   default to -1/disabled until PCB traces or measurements confirm the real
 *   GPIOs.
 * - This header must not be treated as a proof of hardware wiring. It is a
 *   project organization layer and a checklist for later verification.
 */

#include "app_config.h"

// -----------------------------------------------------------------------------
// Inferred core outputs / inputs
// -----------------------------------------------------------------------------
#define SHU1_BOARD_HEATER_GPIO              CONFIG_SHU1_HEATER_GPIO       // candidate: GPIO18
#define SHU1_BOARD_FAN_GPIO                 CONFIG_SHU1_FAN_GPIO          // candidate: GPIO3
#define SHU1_BOARD_BUTTON_GPIO              CONFIG_SHU1_BUTTON_GPIO       // legacy generic button, disabled by default
#define SHU1_BOARD_STATUS_LED_GPIO          CONFIG_SHU1_STATUS_LED_GPIO   // unknown / optional

#define SHU1_BOARD_CHAMBER_ADC_CH           CONFIG_SHU1_CHAMBER_ADC_CH    // candidate: ADC1_CH0 / GPIO0
#define SHU1_BOARD_PTC_ADC_CH               CONFIG_SHU1_PTC_ADC_CH        // candidate: ADC1_CH1 / GPIO1

#define SHU1_BOARD_HEATER_ACTIVE_HIGH       CONFIG_SHU1_HEATER_ACTIVE_HIGH
#define SHU1_BOARD_FAN_ACTIVE_HIGH          CONFIG_SHU1_FAN_ACTIVE_HIGH

// -----------------------------------------------------------------------------
// Panda Breath-style physical buttons - candidates until PCB verification
// -----------------------------------------------------------------------------
#define SHU1_BOARD_BUTTON_AUTO_GPIO         CONFIG_SHU1_BUTTON_AUTO_GPIO
#define SHU1_BOARD_BUTTON_ON_GPIO           CONFIG_SHU1_BUTTON_ON_GPIO
#define SHU1_BOARD_BUTTON_OFF_GPIO          CONFIG_SHU1_BUTTON_OFF_GPIO
#define SHU1_BOARD_BUTTON_GENERIC_GPIO      CONFIG_SHU1_BUTTON_GENERIC_GPIO

#define SHU1_BOARD_BUTTON_ACTIVE_LOW        CONFIG_SHU1_BUTTON_ACTIVE_LOW

// -----------------------------------------------------------------------------
// Panda Breath-style LED/backlight indicators - candidates/placeholders by design
// -----------------------------------------------------------------------------
#define SHU1_BOARD_LED_AUTO_GPIO            CONFIG_SHU1_LED_AUTO_GPIO
#define SHU1_BOARD_LED_ON_GPIO              CONFIG_SHU1_LED_ON_GPIO
#define SHU1_BOARD_LED_OFF_GPIO             CONFIG_SHU1_LED_OFF_GPIO
#define SHU1_BOARD_LED_ERROR_GPIO           CONFIG_SHU1_LED_ERROR_GPIO
#define SHU1_BOARD_LED_WIFI_GPIO            CONFIG_SHU1_LED_WIFI_GPIO
#define SHU1_BOARD_LED_BLE_GPIO             CONFIG_SHU1_LED_BLE_GPIO

#define SHU1_BOARD_LED_ACTIVE_HIGH          CONFIG_SHU1_LED_ACTIVE_HIGH

// -----------------------------------------------------------------------------
// Verification status notes
// -----------------------------------------------------------------------------
#define SHU1_BOARD_PIN_STATUS_HEATER        "candidate_verify_on_pcb"
#define SHU1_BOARD_PIN_STATUS_FAN           "candidate_verify_on_pcb"
#define SHU1_BOARD_PIN_STATUS_CHAMBER_ADC   "candidate_verify_on_pcb"
#define SHU1_BOARD_PIN_STATUS_PTC_ADC       "candidate_verify_on_pcb"
#define SHU1_BOARD_PIN_STATUS_BUTTONS       "unknown_or_partial_verify_on_pcb"
#define SHU1_BOARD_PIN_STATUS_LEDS          "unknown_placeholders_disabled_by_default"
