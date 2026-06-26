/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "fan_triac.h"
#include "app_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "shu1_fan_triac";

static gptimer_handle_t g_gate_timer;
static portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool g_initialized;
static volatile bool g_requested_on;
static volatile bool g_pulse_active;
static volatile bool g_gate_high;

static bool valid_gpio(int gpio) {
    return gpio >= 0 && gpio <= 21;
}

static int gate_level(bool on) {
    return CONFIG_SHU1_FAN_ACTIVE_HIGH ? (on ? 1 : 0) : (on ? 0 : 1);
}

static void IRAM_ATTR write_gate(bool on) {
    if (valid_gpio(CONFIG_SHU1_FAN_GPIO)) {
        gpio_set_level((gpio_num_t)CONFIG_SHU1_FAN_GPIO, gate_level(on));
    }
}

static uint32_t half_cycle_us(void) {
    int hz = CONFIG_SHU1_AC_MAINS_HZ;
    if (hz < 45) hz = 50;
    if (hz > 65) hz = 60;
    return (uint32_t)(1000000UL / (2UL * (uint32_t)hz));
}

static uint32_t gate_delay_us(void) {
    int duty = CONFIG_SHU1_FAN_TRIAC_RUN_PERCENT;
    if (duty < 1) duty = 1;
    if (duty > 100) duty = 100;

    uint32_t min_delay = CONFIG_SHU1_FAN_TRIAC_MIN_DELAY_US;
    uint32_t pulse = CONFIG_SHU1_FAN_TRIAC_GATE_PULSE_US;
    uint32_t half = half_cycle_us();
    uint32_t max_delay = (half > pulse + 200U) ? (half - pulse - 200U) : min_delay;
    uint32_t span = (max_delay > min_delay) ? (max_delay - min_delay) : 0;
    return min_delay + ((span * (uint32_t)(100 - duty)) / 100U);
}

static bool IRAM_ATTR gate_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    (void)edata;
    (void)user_ctx;
    BaseType_t hp_task_woken = pdFALSE;

    portENTER_CRITICAL_ISR(&g_lock);
    if (!g_requested_on) {
        write_gate(false);
        g_gate_high = false;
        g_pulse_active = false;
        gptimer_stop(timer);
        portEXIT_CRITICAL_ISR(&g_lock);
        return hp_task_woken == pdTRUE;
    }

    if (!g_gate_high) {
        write_gate(true);
        g_gate_high = true;
        static gptimer_alarm_config_t pulse_alarm = {0};
        pulse_alarm.alarm_count = CONFIG_SHU1_FAN_TRIAC_GATE_PULSE_US;
        pulse_alarm.reload_count = 0;
        pulse_alarm.flags.auto_reload_on_alarm = false;
        gptimer_set_raw_count(timer, 0);
        gptimer_set_alarm_action(timer, &pulse_alarm);
    } else {
        write_gate(false);
        g_gate_high = false;
        g_pulse_active = false;
        gptimer_stop(timer);
    }
    portEXIT_CRITICAL_ISR(&g_lock);
    return hp_task_woken == pdTRUE;
}

static void IRAM_ATTR zero_cross_isr(void *arg) {
    (void)arg;
    if (!g_initialized || !g_requested_on || !g_gate_timer) return;

    portENTER_CRITICAL_ISR(&g_lock);
    if (g_pulse_active) {
        portEXIT_CRITICAL_ISR(&g_lock);
        return;
    }
    g_pulse_active = true;
    g_gate_high = false;
    write_gate(false);

    static gptimer_alarm_config_t delay_alarm = {0};
    delay_alarm.alarm_count = gate_delay_us();
    delay_alarm.reload_count = 0;
    delay_alarm.flags.auto_reload_on_alarm = false;
    gptimer_set_raw_count(g_gate_timer, 0);
    gptimer_set_alarm_action(g_gate_timer, &delay_alarm);
    esp_err_t err = gptimer_start(g_gate_timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        g_pulse_active = false;
    }
    portEXIT_CRITICAL_ISR(&g_lock);
}

esp_err_t shu1_fan_triac_init(void) {
#if !CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL
    ESP_LOGI(TAG, "TRIAC fan control disabled; fan GPIO will be driven as a plain output");
    return ESP_OK;
#else
    if (!valid_gpio(CONFIG_SHU1_FAN_GPIO)) {
        ESP_LOGW(TAG, "TRIAC fan control disabled; invalid fan GPIO%d", CONFIG_SHU1_FAN_GPIO);
        return ESP_OK;
    }
    if (!valid_gpio(CONFIG_SHU1_ZERO_CROSS_GPIO)) {
        ESP_LOGW(TAG, "TRIAC fan control disabled; invalid zero-cross GPIO%d", CONFIG_SHU1_ZERO_CROSS_GPIO);
        return ESP_OK;
    }

    gpio_config_t gate = {
        .pin_bit_mask = 1ULL << CONFIG_SHU1_FAN_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&gate), TAG, "fan gate gpio config failed");
    write_gate(false);

    gpio_config_t zc = {
        .pin_bit_mask = 1ULL << CONFIG_SHU1_ZERO_CROSS_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = CONFIG_SHU1_ZERO_CROSS_RISING_EDGE ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&zc), TAG, "zero-cross gpio config failed");

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
        .intr_priority = 0,
    };
    ESP_RETURN_ON_ERROR(gptimer_new_timer(&timer_config, &g_gate_timer), TAG, "gate timer create failed");
    gptimer_event_callbacks_t cbs = {
        .on_alarm = gate_timer_cb,
    };
    ESP_RETURN_ON_ERROR(gptimer_register_event_callbacks(g_gate_timer, &cbs, NULL), TAG, "gate timer callback failed");
    ESP_RETURN_ON_ERROR(gptimer_enable(g_gate_timer), TAG, "gate timer enable failed");

    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        return isr_err;
    }
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add((gpio_num_t)CONFIG_SHU1_ZERO_CROSS_GPIO, zero_cross_isr, NULL), TAG, "zero-cross isr add failed");

    g_initialized = true;
    ESP_LOGI(TAG, "TRIAC fan ready: gate GPIO%d, zero-cross GPIO%d, %d Hz, duty %d%%",
             CONFIG_SHU1_FAN_GPIO, CONFIG_SHU1_ZERO_CROSS_GPIO,
             CONFIG_SHU1_AC_MAINS_HZ, CONFIG_SHU1_FAN_TRIAC_RUN_PERCENT);
    return ESP_OK;
#endif
}

void shu1_fan_triac_set(bool on) {
#if !CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL
    write_gate(on);
#else
    portENTER_CRITICAL(&g_lock);
    g_requested_on = on;
    if (!on) {
        write_gate(false);
        g_gate_high = false;
        g_pulse_active = false;
        if (g_gate_timer) gptimer_stop(g_gate_timer);
    }
    portEXIT_CRITICAL(&g_lock);
#endif
}

void shu1_fan_triac_force_off(void) {
    shu1_fan_triac_set(false);
}

bool shu1_fan_triac_is_active(void) {
    return g_requested_on;
}
