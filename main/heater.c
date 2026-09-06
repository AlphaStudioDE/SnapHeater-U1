/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "heater.h"
#include "app_state.h"
#include "fan_triac.h"
#include "safety_latch.h"
#include "session_journal.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

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

esp_err_t shu1_heater_preinit_off(void) {
    // Establish physical OFF levels before NVS or application-state loading.
    // This function deliberately does not touch shared state or install the ZC ISR.
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, false, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
    ESP_RETURN_ON_ERROR(configure_output(CONFIG_SHU1_HEATER_GPIO), TAG, "early heater off failed");
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, false, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
    ESP_RETURN_ON_ERROR(shu1_fan_triac_preinit_off(), TAG, "early fan off failed");
    return ESP_OK;
}

esp_err_t shu1_heater_init(void) {
    // Preload the safe heater level before enabling the GPIO output.
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, false, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
    ESP_RETURN_ON_ERROR(configure_output(CONFIG_SHU1_HEATER_GPIO), TAG, "heater gpio config failed");
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, false, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
    ESP_RETURN_ON_ERROR(configure_output(CONFIG_SHU1_STATUS_LED_GPIO), TAG, "status led gpio config failed");
    ESP_RETURN_ON_ERROR(shu1_fan_triac_init(), TAG, "fan triac init failed");
    shu1_heater_force_off();
    ESP_LOGW(TAG, "normal heater output is %s", CONFIG_SHU1_ENABLE_HEATER_OUTPUT ? "ENABLED" : "DISABLED / DRY-RUN");
    ESP_LOGW(TAG, "diagnostic GPIO probe API is %s", CONFIG_SHU1_ENABLE_GPIO_PROBE ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

void shu1_heater_set(bool heater_on, bool fan_on) {
    // Runtime output application must never call a UART/log sink, even after ON.
    // Status/history record applied outputs from their own low-priority tasks.
    // Request airflow first. ON is applied by the fan ISR at the next validated
    // zero-cross; OFF is immediate. The SSR is never allowed on until that has happened.
    if (!heater_on || !fan_on) shu1_heater_cut_power();
    shu1_fan_triac_set(fan_on);
#if CONFIG_SHU1_ENABLE_HEATER_OUTPUT
    bool heater_interlock_ok = fan_on && shu1_fan_triac_is_running();
    bool physical_heater_on = heater_on && heater_interlock_ok &&
        shu1_session_journal_ready() &&
        !shu1_safety_latch_is_set() && !shu1_safety_latch_is_inhibited();
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, physical_heater_on, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
#else
    (void)heater_on;
    bool physical_heater_on = false;
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, false, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
#endif
    write_gpio_if_valid(CONFIG_SHU1_STATUS_LED_GPIO, physical_heater_on || fan_on, true);

    shu1_runtime_t rt = shu1_state_get_runtime();
#if CONFIG_SHU1_ENABLE_HEATER_OUTPUT
    rt.heater_output_on = physical_heater_on;
#else
    rt.heater_output_on = false;
#endif
    rt.fan_output_on = shu1_fan_triac_is_running();
    shu1_state_update_runtime(&rt);
}

void shu1_heater_cut_power(void) {
    write_gpio_if_valid(CONFIG_SHU1_HEATER_GPIO, false, CONFIG_SHU1_HEATER_ACTIVE_HIGH);
    shu1_runtime_t rt = shu1_state_get_runtime();
    rt.heater_output_on = false;
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
    // Diagnostic pulses must not be a second writer that can steal safety airflow.
    // Use the normal control policy for supervised functional checks.
    (void)output;
    (void)duration_ms;
    return ESP_ERR_INVALID_STATE;
}
