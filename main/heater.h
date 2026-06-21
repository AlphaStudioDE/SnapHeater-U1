/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "app_state.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t shu1_heater_init(void);
void shu1_heater_set(bool heater_on, bool fan_on);
void shu1_heater_force_off(void);
esp_err_t shu1_heater_probe_pulse(shu1_output_t output, int duration_ms);

#ifdef __cplusplus
}
#endif
