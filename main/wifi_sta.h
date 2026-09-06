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

esp_err_t shu1_wifi_start(void);
// Called with policy guard held. Dedicated BLE provisioning only.
esp_err_t shu1_wifi_setup_request(const cJSON *request);
bool shu1_wifi_status_json(cJSON *root);

#ifdef __cplusplus
}
#endif
