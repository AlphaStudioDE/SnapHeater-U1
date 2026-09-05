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
    /* Smoothed values reproduce DragonBreath's five-sample control/telemetry path. */
    float chamber_c;
    float ptc_c;
    /* Fresh values are reserved for hard cutoffs and sensor-fault decisions. */
    float chamber_instant_c;
    float ptc_instant_c;
    int chamber_raw;
    int ptc_raw;
    shu1_sensor_status_t chamber_status;
    shu1_sensor_status_t ptc_status;
} shu1_sensor_sample_t;

esp_err_t shu1_ntc_init(void);
esp_err_t shu1_ntc_read(shu1_sensor_sample_t *out);
int shu1_ntc_rref_kohm(void);
float shu1_ntc_smoothed_c(int channel);
float shu1_ntc_get_offset_c(int channel);
esp_err_t shu1_ntc_set_offset_c(int channel, float offset_c);

#ifdef __cplusplus
}
#endif
