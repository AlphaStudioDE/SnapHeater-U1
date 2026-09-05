/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t shu1_safety_start(void);
// Wake the sole output-writing control task after a cross-task stop/latch request.
void shu1_safety_wake(void);
// Wait until the safety task has armed the task watchdog and completed one
// fail-closed control iteration. OTA images must not be accepted before this.
esp_err_t shu1_safety_wait_healthy(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
