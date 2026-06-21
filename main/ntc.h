/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "app_state.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float chamber_c;
    float ptc_c;
    int chamber_raw;
    int ptc_raw;
    shu1_sensor_status_t chamber_status;
    shu1_sensor_status_t ptc_status;
} shu1_sensor_sample_t;

esp_err_t shu1_ntc_init(void);
esp_err_t shu1_ntc_read(shu1_sensor_sample_t *out);

#ifdef __cplusplus
}
#endif
