/*
 * SnapHeater U1 — stock Panda Breath fan control.
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 *
 * Hardware requirements established by plastikman/DragonBreath:
 * held active-HIGH gate, ON at validated ZC, immediate OFF; never PWM.
 * Local implementation; no upstream source is included or called.
 */
#include "fan_triac.h"
#include "app_config.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#define SHU1_ZC_GLITCH_REJECT_US 4000U
#define SHU1_ZC_PRESENT_MAX_AGE_US 100000U
static bool g_initialized;

esp_err_t shu1_fan_triac_preinit_off(void) {
#if CONFIG_SHU1_FAN_GPIO == 3
    esp_err_t err = gpio_set_level((gpio_num_t)CONFIG_SHU1_FAN_GPIO, 0);
    if (err != ESP_OK) return err;
    const gpio_config_t gate = {
        .pin_bit_mask = 1ULL << CONFIG_SHU1_FAN_GPIO, .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&gate);
    if (err != ESP_OK) return err;
    return gpio_set_level((gpio_num_t)CONFIG_SHU1_FAN_GPIO, 0);
#else
    return ESP_OK;
#endif
}

#if CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL
static portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool g_requested_on, g_applied_on;
static volatile uint64_t g_zc_edges, g_zc_rejected_edges;
static volatile uint64_t g_zc_signal_gaps;
static volatile int64_t g_zc_last_edge_us;
static volatile uint32_t g_zc_last_period_us, g_zc_min_period_us, g_zc_max_period_us;

static void IRAM_ATTR zero_cross_isr(void *arg) {
    (void)arg;
    int64_t now_us = esp_timer_get_time();
    int64_t previous_us = g_zc_last_edge_us;
    if (previous_us != 0) {
        if (now_us <= previous_us ||
            (uint64_t)(now_us - previous_us) < SHU1_ZC_GLITCH_REJECT_US) {
            ++g_zc_rejected_edges;
            return;
        }
        uint64_t interval_us = (uint64_t)(now_us - previous_us);
        if (interval_us > SHU1_ZC_PRESENT_MAX_AGE_US) ++g_zc_signal_gaps;
        uint32_t period_us = interval_us > UINT32_MAX ? UINT32_MAX : (uint32_t)interval_us;
        g_zc_last_period_us = period_us;
        if (!g_zc_min_period_us || period_us < g_zc_min_period_us) g_zc_min_period_us = period_us;
        if (period_us > g_zc_max_period_us) g_zc_max_period_us = period_us;
    }
    g_zc_last_edge_us = now_us;
    ++g_zc_edges;
    bool requested = g_requested_on;
    if (requested != g_applied_on) {
        gpio_set_level((gpio_num_t)CONFIG_SHU1_FAN_GPIO, requested ? 1 : 0);
        g_applied_on = requested;
    }
}
#endif

esp_err_t shu1_fan_triac_init(void) {
    if (g_initialized) return ESP_OK; // Do not reset a live ISR.
    esp_err_t err = shu1_fan_triac_preinit_off();
    if (err != ESP_OK) return err;
#if CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL
    g_requested_on = g_applied_on = false;
    g_zc_edges = g_zc_rejected_edges = 0;
    g_zc_signal_gaps = 0;
    g_zc_last_edge_us = 0;
    g_zc_last_period_us = g_zc_min_period_us = g_zc_max_period_us = 0;
    const gpio_config_t zc = {
        .pin_bit_mask = 1ULL << CONFIG_SHU1_ZERO_CROSS_GPIO, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    err = gpio_config(&zc);
    if (err != ESP_OK) return err;
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = gpio_isr_handler_add((gpio_num_t)CONFIG_SHU1_ZERO_CROSS_GPIO, zero_cross_isr, NULL);
    if (err != ESP_OK) return err;
    g_initialized = true;
#endif
    return ESP_OK;
}

void shu1_fan_triac_set(bool on) {
#if CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL
    // On single-core C3 this excludes the GPIO ISR while applying immediate OFF.
    portENTER_CRITICAL(&g_lock);
    g_requested_on = on && g_initialized;
    if (!g_requested_on) {
        gpio_set_level((gpio_num_t)CONFIG_SHU1_FAN_GPIO, 0);
        g_applied_on = false;
    }
    portEXIT_CRITICAL(&g_lock);
#else
    (void)on;
#endif
}

void shu1_fan_triac_force_off(void) { shu1_fan_triac_set(false); }

bool shu1_fan_triac_is_active(void) {
#if CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL
    return g_initialized && g_requested_on;
#else
    return false;
#endif
}

bool shu1_fan_triac_is_running(void) {
#if CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL
    portENTER_CRITICAL(&g_lock);
    int64_t last = g_zc_last_edge_us;
    bool applied = g_applied_on;
    portEXIT_CRITICAL(&g_lock);
    int64_t now = esp_timer_get_time();
    return g_initialized && applied && last > 0 && now >= last &&
           now - last <= SHU1_ZC_PRESENT_MAX_AGE_US;
#else
    return false;
#endif
}

shu1_zero_cross_stats_t shu1_fan_triac_zero_cross_stats(void) {
    shu1_zero_cross_stats_t stats = {0};
#if CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL
    portENTER_CRITICAL(&g_lock);
    stats.edge_count = g_zc_edges;
    stats.rejected_edge_count = g_zc_rejected_edges;
    stats.signal_gap_count = g_zc_signal_gaps;
    stats.last_period_us = g_zc_last_period_us;
    stats.min_period_us = g_zc_min_period_us;
    stats.max_period_us = g_zc_max_period_us;
    int64_t last = g_zc_last_edge_us;
    portEXIT_CRITICAL(&g_lock);
    int64_t now = esp_timer_get_time();
    stats.last_edge_ms = last > 0 ? last / 1000 : 0;
    stats.signal_present = last > 0 && now >= last && now - last <= SHU1_ZC_PRESENT_MAX_AGE_US;
#endif
    return stats;
}
