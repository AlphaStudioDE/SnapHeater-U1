/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 *
 * Panda Breath NTC conversion adapted from plastikman/DragonBreath (MIT):
 * calibrated ADC voltage, 33/82 kOhm Rref strap and the stock 114-entry R/T table.
 */

#include "ntc.h"
#include "app_config.h"
#include <math.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "nvs.h"
#include <stdatomic.h>

#define SHU1_NTC_VSUPPLY_V 3.3f
#define SHU1_NTC_RAW_OPEN_MIN 0xFFEU
#define SHU1_NTC_RAW_SHORT_MAX 0x14U
#define SHU1_NTC_TEMP_BASE_C 12
#define SHU1_NTC_AVG_WINDOW 5
#define SHU1_NTC_OFFSET_MAX_C 5.0f

static const char *TAG = "shu1_ntc";
static adc_oneshot_unit_handle_t g_adc;
static adc_cali_handle_t g_cali[2];
static int g_rref_kohm;
static float g_window[2][SHU1_NTC_AVG_WINDOW];
static int g_window_index[2];
static int g_window_count[2];
static float g_smoothed_c[2] = {NAN, NAN};
static _Atomic int32_t g_offset_centi[2];

static const adc_channel_t g_channels[2] = {
    (adc_channel_t)CONFIG_SHU1_CHAMBER_ADC_CH,
    (adc_channel_t)CONFIG_SHU1_PTC_ADC_CH,
};

static const float g_rt_kohm[] = {
    198.7f,189.4f,180.7f,172.4f,164.5f,157.0f,149.9f,143.2f,136.8f,130.7f,
    124.9f,119.4f,114.2f,109.2f,104.5f,100.0f,95.7f,91.6f,87.8f,84.1f,
    80.6f,77.2f,74.0f,70.9f,68.0f,65.3f,62.6f,60.1f,57.7f,55.4f,
    53.2f,51.1f,49.1f,47.2f,45.3f,43.6f,41.9f,40.3f,38.8f,37.3f,
    35.9f,34.5f,33.2f,32.0f,30.8f,29.7f,28.6f,27.6f,26.6f,25.6f,
    24.7f,23.8f,23.0f,22.2f,21.4f,20.6f,19.9f,19.2f,18.6f,17.9f,
    17.3f,16.7f,16.2f,15.6f,15.1f,14.6f,14.1f,13.6f,13.2f,12.8f,
    12.4f,12.0f,11.6f,11.2f,10.9f,10.5f,10.2f,9.9f,9.6f,9.3f,
    9.0f,8.7f,8.4f,8.2f,7.9f,7.7f,7.4f,7.2f,7.0f,6.8f,
    6.6f,6.4f,6.2f,6.0f,5.9f,5.7f,5.5f,5.4f,5.2f,5.1f,
    4.9f,4.8f,4.7f,4.5f,4.4f,4.3f,4.2f,4.1f,3.9f,3.8f,
    3.7f,3.6f,3.5f,3.4f,
};

static shu1_sensor_status_t classify_sample(int raw, float volts) {
    if (raw <= (int)SHU1_NTC_RAW_SHORT_MAX) return SHU1_SENSOR_SHORT;
    if (raw >= (int)SHU1_NTC_RAW_OPEN_MIN) return SHU1_SENSOR_OPEN;
    if (!isfinite(volts) || volts <= 0.0f || volts >= SHU1_NTC_VSUPPLY_V) return SHU1_SENSOR_OPEN;
    return SHU1_SENSOR_OK;
}

static float resistance_to_temp_c(float r_kohm) {
    const int count = (int)(sizeof(g_rt_kohm) / sizeof(g_rt_kohm[0]));
    if (r_kohm >= g_rt_kohm[0]) return (float)SHU1_NTC_TEMP_BASE_C;
    if (r_kohm <= g_rt_kohm[count - 1]) return (float)(SHU1_NTC_TEMP_BASE_C + count - 1);
    for (int i = 0; i < count - 1; ++i) {
        float colder_r = g_rt_kohm[i];
        float hotter_r = g_rt_kohm[i + 1];
        if (r_kohm <= colder_r && r_kohm >= hotter_r) {
            float fraction = (colder_r - r_kohm) / (colder_r - hotter_r);
            return (float)(SHU1_NTC_TEMP_BASE_C + i) + fraction;
        }
    }
    return NAN;
}

static float clamp_offset(float offset_c) {
    if (!isfinite(offset_c)) return 0.0f;
    if (offset_c < -SHU1_NTC_OFFSET_MAX_C) return -SHU1_NTC_OFFSET_MAX_C;
    if (offset_c > SHU1_NTC_OFFSET_MAX_C) return SHU1_NTC_OFFSET_MAX_C;
    return offset_c;
}

static void load_offsets(void) {
    nvs_handle_t h;
    if (nvs_open("app_nvs", NVS_READONLY, &h) != ESP_OK) return;
    int32_t centi = 0;
    if (nvs_get_i32(h, "ntc_off_ch", &centi) == ESP_OK)
        atomic_store(&g_offset_centi[0], (int32_t)lroundf(clamp_offset((float)centi / 100.0f) * 100.0f));
    if (nvs_get_i32(h, "ntc_off_ptc", &centi) == ESP_OK)
        atomic_store(&g_offset_centi[1], (int32_t)lroundf(clamp_offset((float)centi / 100.0f) * 100.0f));
    nvs_close(h);
}

static float push_smoothed(int channel, float temp_c) {
    g_window[channel][g_window_index[channel]] = temp_c;
    g_window_index[channel] = (g_window_index[channel] + 1) % SHU1_NTC_AVG_WINDOW;
    if (g_window_count[channel] < SHU1_NTC_AVG_WINDOW) g_window_count[channel]++;
    float total = 0.0f;
    for (int i = 0; i < g_window_count[channel]; ++i) total += g_window[channel][i];
    g_smoothed_c[channel] = total / (float)g_window_count[channel];
    return g_smoothed_c[channel];
}

static bool g_ntc_ready;

static esp_err_t detect_rref_kohm(int *out) {
    gpio_config_t strap = {
        .pin_bit_mask = 1ULL << CONFIG_SHU1_RREF_STRAP_GPIO,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    strap.pull_up_en = GPIO_PULLUP_ENABLE;
    strap.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&strap), TAG, "Rref pull-up failed");
    esp_rom_delay_us(2000);
    int with_pullup = gpio_get_level((gpio_num_t)CONFIG_SHU1_RREF_STRAP_GPIO);

    strap.pull_up_en = GPIO_PULLUP_DISABLE;
    strap.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&strap), TAG, "Rref pull-down failed");
    esp_rom_delay_us(2000);
    int with_pulldown = gpio_get_level((gpio_num_t)CONFIG_SHU1_RREF_STRAP_GPIO);

    strap.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&strap), TAG, "Rref pull release failed");
    if (with_pullup == with_pulldown) {
        int rref = with_pullup == 0 ? 82 : 33;
        ESP_LOGI(TAG, "Rref strap GPIO%d=%d -> %d kOhm",
                 CONFIG_SHU1_RREF_STRAP_GPIO, with_pullup, rref);
        *out = rref;
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Rref strap GPIO%d floating (PU=%d PD=%d); fail-safe 33 kOhm",
             CONFIG_SHU1_RREF_STRAP_GPIO, with_pullup, with_pulldown);
    *out = 33;
    return ESP_OK;
}

esp_err_t shu1_ntc_init(void) {
    g_ntc_ready = false;
    if (CONFIG_SHU1_RREF_STRAP_GPIO < 0 || CONFIG_SHU1_RREF_STRAP_GPIO > 21) return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(detect_rref_kohm(&g_rref_kohm), TAG, "Rref detection failed");
    load_offsets();
    adc_oneshot_unit_init_cfg_t init_config = {.unit_id = ADC_UNIT_1, .ulp_mode = ADC_ULP_MODE_DISABLE};
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config, &g_adc), TAG, "adc unit init failed");
    adc_oneshot_chan_cfg_t channel_config = {.bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12};
    for (int i = 0; i < 2; ++i) {
        ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(g_adc, g_channels[i], &channel_config), TAG, "adc channel config failed");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1, .chan = g_channels[i], .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_RETURN_ON_ERROR(adc_cali_create_scheme_curve_fitting(&cali_config, &g_cali[i]), TAG, "adc calibration init failed");
    }
    g_ntc_ready = true;
    ESP_LOGI(TAG, "stock NTC conversion ready: chamber ADC%d, PTC ADC%d, Rref=%d kOhm",
             CONFIG_SHU1_CHAMBER_ADC_CH, CONFIG_SHU1_PTC_ADC_CH, g_rref_kohm);
    return ESP_OK;
}

static esp_err_t read_channel(int index, int *raw_out, float *instant_out,
                              float *smoothed_out, shu1_sensor_status_t *status_out) {
    int raw = 0;
    int millivolts = 0;
    ESP_RETURN_ON_ERROR(adc_oneshot_read(g_adc, g_channels[index], &raw), TAG, "adc read failed");
    ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(g_cali[index], raw, &millivolts), TAG, "adc calibration failed");
    float volts = (float)millivolts / 1000.0f;
    shu1_sensor_status_t status = classify_sample(raw, volts);
    float temp = NAN;
    if (status == SHU1_SENSOR_OK) {
        float resistance_kohm = (float)g_rref_kohm * volts / (SHU1_NTC_VSUPPLY_V - volts);
        temp = resistance_to_temp_c(resistance_kohm) +
               (float)atomic_load(&g_offset_centi[index]) / 100.0f;
        if (!isfinite(temp)) status = SHU1_SENSOR_INVALID;
    }
    *raw_out = raw;
    *instant_out = temp;
    *smoothed_out = status == SHU1_SENSOR_OK ? push_smoothed(index, temp) : NAN;
    *status_out = status;
    return ESP_OK;
}

esp_err_t shu1_ntc_read(shu1_sensor_sample_t *out) {
    if (!out || !g_ntc_ready) return ESP_ERR_INVALID_STATE;
    out->chamber_status = out->ptc_status = SHU1_SENSOR_INVALID;
    out->chamber_c = out->ptc_c = NAN;
    out->chamber_instant_c = out->ptc_instant_c = NAN;
    // Always inspect both channels; a failed chamber read cannot hide PTC overtemp.
    if (read_channel(0, &out->chamber_raw, &out->chamber_instant_c,
                     &out->chamber_c, &out->chamber_status) != ESP_OK)
        out->chamber_status = SHU1_SENSOR_INVALID;
    if (read_channel(1, &out->ptc_raw, &out->ptc_instant_c,
                     &out->ptc_c, &out->ptc_status) != ESP_OK)
        out->ptc_status = SHU1_SENSOR_INVALID;
    return ESP_OK;
}

int shu1_ntc_rref_kohm(void) { return g_rref_kohm; }

float shu1_ntc_smoothed_c(int channel) {
    return channel >= 0 && channel < 2 ? g_smoothed_c[channel] : NAN;
}

float shu1_ntc_get_offset_c(int channel) {
    return channel >= 0 && channel < 2
        ? (float)atomic_load(&g_offset_centi[channel]) / 100.0f : 0.0f;
}

esp_err_t shu1_ntc_set_offset_c(int channel, float offset_c) {
    if (channel < 0 || channel >= 2) return ESP_ERR_INVALID_ARG;
    offset_c = clamp_offset(offset_c);
    nvs_handle_t h;
    esp_err_t err = nvs_open("app_nvs", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    int32_t centi = (int32_t)lroundf(offset_c * 100.0f);
    err = nvs_set_i32(h, channel == 0 ? "ntc_off_ch" : "ntc_off_ptc", centi);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) atomic_store(&g_offset_centi[channel], centi);
    return err;
}
