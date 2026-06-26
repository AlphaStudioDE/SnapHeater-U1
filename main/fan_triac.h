/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t shu1_fan_triac_init(void);
void shu1_fan_triac_set(bool on);
void shu1_fan_triac_force_off(void);
bool shu1_fan_triac_is_active(void);

#ifdef __cplusplus
}
#endif
