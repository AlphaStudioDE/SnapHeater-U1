/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "heater.h"
#include "app_state.h"
#include "fan_triac.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "shu1_heater";

static bool valid_gpio(int gpio) {
    return gpio >= 0 && gpio <= 21; // ESP32-C3 package dependent; verify actual module pins.
}

static int gpio_level_for(bool on, bool active_high) {
    return active_high ? (on ? 1 : 0) : (on ? 0 : 1);
}

static void write_gpio_if_valid(int gpio, bool on, bool active_high) {
    if (valid_gpio(gpio)) gpio_set_level((gpio_num_t)gpio, gpio_level_for(on, active_high));
}

static esp_err_t configure_output(int gpio) {
    if (!valid_gpio(gpio)) return ESP_OK;
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io_conf);
}

esp_err_t shu1_heater_init(void) {
    ESP_RETURN_ON_ERROR(configure_output(CONFIG_SHU1_HEATER_GPIO), TAG, "heater gpio config failed");
    ESP_RETURN_ON_ERROR(configure_output(CONFIG_SHU1_STATUS_LED_GPIO), TAG, "status led gpio config failed");
    ESP_RETURN_ON_ERROR(shu1_fan_triac_init(), TAG, "fan triac init failed");
    shu1_heater_force_off();
    ESP_LOGW(TAG, "normal heater output is %s", CONFIG_SHU1_ENABLE_HEATER_OUTPUT ? "ENABLED" : "DISABLED / DRY-RUN");
    ESP_LOGW(TAG, "diagnostic GPIO probe API is %s", CONFIG_SHU1_ENABLE_GPIO_PROBE ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

void shu1_heater_set(bool heater_on, bool fan_on) {
#if CONFIG_SHU1_ENABLE_HEATER_OUTPUT
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, heater_on, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
#else
    (void)heater_on;
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, false, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
#endif
    shu1_fan_triac_set(fan_on);
    write_gpio_if_valid(CONFIG_SHU1_STATUS_LED_GPIO, heater_on || fan_on, true);

    shu1_runtime_t rt = shu1_state_get_runtime();
#if CONFIG_SHU1_ENABLE_HEATER_OUTPUT
    rt.heater_output_on = heater_on;
#else
    rt.heater_output_on = false;
#endif
    rt.fan_output_on = fan_on;
    shu1_state_update_runtime(&rt);
}

void shu1_heater_force_off(void) {
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, false, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
    shu1_fan_triac_force_off();
    write_gpio_if_valid(CONFIG_SHU1_STATUS_LED_GPIO, false, true);

    shu1_runtime_t rt = shu1_state_get_runtime();
    rt.heater_output_on = false;
    rt.fan_output_on = false;
    shu1_state_update_runtime(&rt);
}

esp_err_t shu1_heater_probe_pulse(shu1_output_t output, int duration_ms) {
#if !CONFIG_SHU1_ENABLE_GPIO_PROBE
    (void)output;
    (void)duration_ms;
    return ESP_ERR_INVALID_STATE;
#else
    if (output == SHU1_OUTPUT_HEATER) {
        if (!valid_gpio(CONFIG_SHU1_HEATER_GPIO)) return ESP_ERR_INVALID_ARG;
        if (duration_ms < 50) duration_ms = 50;
        if (duration_ms > CONFIG_SHU1_MAX_HEATER_PROBE_MS) duration_ms = CONFIG_SHU1_MAX_HEATER_PROBE_MS;
        ESP_LOGW(TAG, "HEATER PROBE PULSE: GPIO%d for %d ms", CONFIG_SHU1_HEATER_GPIO, duration_ms);
        // Keep fan on during heater pulse if fan GPIO is available.
        shu1_fan_triac_set(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, true, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, false, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
        vTaskDelay(pdMS_TO_TICKS(500));
        shu1_fan_triac_set(false);
        return ESP_OK;
    }
    if (output == SHU1_OUTPUT_FAN) {
        if (!valid_gpio(CONFIG_SHU1_FAN_GPIO)) return ESP_ERR_INVALID_ARG;
        if (duration_ms < 100) duration_ms = 100;
        if (duration_ms > CONFIG_SHU1_MAX_FAN_PROBE_MS) duration_ms = CONFIG_SHU1_MAX_FAN_PROBE_MS;
        ESP_LOGW(TAG, "FAN PROBE PULSE: GPIO%d for %d ms", CONFIG_SHU1_FAN_GPIO, duration_ms);
        shu1_fan_triac_set(true);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        shu1_fan_triac_set(false);
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
#endif
}
