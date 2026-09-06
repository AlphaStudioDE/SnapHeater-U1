/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include "cJSON.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t shu1_moonraker_start(void);
// Authenticated dedicated request; caller holds the control policy guard.
esp_err_t shu1_moonraker_setup_request(const cJSON *root);
bool shu1_moonraker_setup_status(cJSON *root);

#ifdef __cplusplus
}
#endif
