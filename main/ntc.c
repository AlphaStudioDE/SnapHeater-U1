/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "ntc.h"
#include "app_config.h"
#include <math.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "shu1_ntc";
static adc_oneshot_unit_handle_t g_adc = NULL;

static shu1_sensor_status_t classify_raw(int raw) {
    if (raw <= CONFIG_SHU1_ADC_OPEN_RAW_MIN) return SHU1_SENSOR_OPEN;
    if (raw >= CONFIG_SHU1_ADC_SHORT_RAW_MAX) return SHU1_SENSOR_SHORT;
    return SHU1_SENSOR_OK;
}

static float raw_to_temp_c(int raw) {
    // Generic voltage divider model: 3.3V -> series resistor -> ADC node -> NTC -> GND.
    // This is intentionally configurable because Panda Breath PCB resistor/NTC values must be verified.
    if (raw <= 0 || raw >= 4095) return NAN;
    const float r_series = (float)CONFIG_SHU1_NTC_SERIES_OHM;
    const float r0 = (float)CONFIG_SHU1_NTC_R0_OHM;
    const float beta = (float)CONFIG_SHU1_NTC_BETA;
    const float t0 = 298.15f;
    const float v_ratio = (float)raw / 4095.0f;
    const float r_ntc = r_series * v_ratio / (1.0f - v_ratio);
    if (r_ntc <= 0.0f) return NAN;
    const float inv_t = (1.0f / t0) + (1.0f / beta) * logf(r_ntc / r0);
    return (1.0f / inv_t) - 273.15f;
}

esp_err_t shu1_ntc_init(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config, &g_adc), TAG, "adc unit init failed");

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(g_adc, (adc_channel_t)CONFIG_SHU1_CHAMBER_ADC_CH, &config), TAG, "chamber adc cfg failed");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(g_adc, (adc_channel_t)CONFIG_SHU1_PTC_ADC_CH, &config), TAG, "ptc adc cfg failed");
    ESP_LOGI(TAG, "ADC ready: chamber ch=%d, ptc ch=%d", CONFIG_SHU1_CHAMBER_ADC_CH, CONFIG_SHU1_PTC_ADC_CH);
    return ESP_OK;
}

esp_err_t shu1_ntc_read(shu1_sensor_sample_t *out) {
    if (!out || !g_adc) return ESP_ERR_INVALID_ARG;
    int chamber_raw = 0;
    int ptc_raw = 0;
    esp_err_t err1 = adc_oneshot_read(g_adc, (adc_channel_t)CONFIG_SHU1_CHAMBER_ADC_CH, &chamber_raw);
    esp_err_t err2 = adc_oneshot_read(g_adc, (adc_channel_t)CONFIG_SHU1_PTC_ADC_CH, &ptc_raw);
    if (err1 != ESP_OK) return err1;
    if (err2 != ESP_OK) return err2;

    out->chamber_raw = chamber_raw;
    out->ptc_raw = ptc_raw;
    out->chamber_status = classify_raw(chamber_raw);
    out->ptc_status = classify_raw(ptc_raw);
    out->chamber_c = raw_to_temp_c(chamber_raw);
    out->ptc_c = raw_to_temp_c(ptc_raw);

    if (out->chamber_status == SHU1_SENSOR_OK && (!isfinite(out->chamber_c) || out->chamber_c < SHU1_MIN_VALID_TEMP_C || out->chamber_c > SHU1_MAX_VALID_TEMP_C)) {
        out->chamber_status = SHU1_SENSOR_INVALID;
    }
    if (out->ptc_status == SHU1_SENSOR_OK && (!isfinite(out->ptc_c) || out->ptc_c < SHU1_MIN_VALID_TEMP_C || out->ptc_c > SHU1_MAX_VALID_TEMP_C)) {
        out->ptc_status = SHU1_SENSOR_INVALID;
    }
    return ESP_OK;
}
