/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t shu1_ble_start(void);
void shu1_ble_notify_status_now(void);

#ifdef __cplusplus
}
#endif
