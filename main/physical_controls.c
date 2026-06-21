/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "physical_controls.h"
#include "app_config.h"
#include "app_state.h"
#include "heater.h"
#include "event_log.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "shu1_phys";

typedef struct {
    const char *name;
    int id;
    int gpio;
    bool enabled;
    bool stable_pressed;
    bool raw_pressed_last;
    bool long_fired;
    int64_t raw_changed_ms;
    int64_t press_started_ms;
} shu1_button_t;

static shu1_button_t g_buttons[] = {
    {"auto", SHU1_PHYS_BTN_AUTO, CONFIG_SHU1_BUTTON_AUTO_GPIO, false, false, false, false, 0, 0},
    {"on", SHU1_PHYS_BTN_ON, CONFIG_SHU1_BUTTON_ON_GPIO, false, false, false, false, 0, 0},
    {"off", SHU1_PHYS_BTN_OFF, CONFIG_SHU1_BUTTON_OFF_GPIO, false, false, false, false, 0, 0},
    {"generic", SHU1_PHYS_BTN_GENERIC, CONFIG_SHU1_BUTTON_GENERIC_GPIO, false, false, false, false, 0, 0},
};

static bool valid_gpio(int gpio) {
    return gpio >= 0 && gpio <= 21; // ESP32-C3 module/package dependent. Verify on real PCB.
}

static int active_level(bool on) {
    return CONFIG_SHU1_LED_ACTIVE_HIGH ? (on ? 1 : 0) : (on ? 0 : 1);
}

static bool raw_to_pressed(int raw) {
    return CONFIG_SHU1_BUTTON_ACTIVE_LOW ? (raw == 0) : (raw != 0);
}

static void gpio_write_if_valid(int gpio, bool on) {
    if (valid_gpio(gpio)) {
        gpio_set_level((gpio_num_t)gpio, active_level(on));
    }
}

static esp_err_t configure_output(int gpio) {
    if (!valid_gpio(gpio)) return ESP_OK;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    gpio_write_if_valid(gpio, false);
    return err;
}

static esp_err_t configure_input(int gpio) {
    if (!valid_gpio(gpio)) return ESP_OK;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = CONFIG_SHU1_BUTTON_ACTIVE_LOW ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = CONFIG_SHU1_BUTTON_ACTIVE_LOW ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io);
}

static void set_notification(int level, const char *code, const char *message) {
    shu1_runtime_t rt = shu1_state_get_runtime();
    rt.notification_level = level;
    snprintf(rt.notification_code, sizeof(rt.notification_code), "%s", code ? code : "physical_control");
    snprintf(rt.notification_message, sizeof(rt.notification_message), "%s", message ? message : "Physical control event");
    rt.notification_ms = esp_timer_get_time() / 1000;
    shu1_state_update_runtime(&rt);
}

static void stop_all_user_cycles(bool emergency) {
    shu1_settings_t st = shu1_state_get_settings();
    st.work_on = false;
    st.drying_running = false;
    st.preheat_running = false;
    st.dryout_running = false;
    st.health_test_running = false;
    st.scheduled_preheat_enabled = false;
    st.keep_warm_active = false;
    st.pickup_active = false;
    st.resume_recover_active = false;
    st.tempering_phase = SHU1_TEMPERING_IDLE;
    st.preheat_phase = SHU1_PREHEAT_IDLE;
    st.heat_soak_phase = SHU1_HEAT_SOAK_IDLE;
    st.health_test_phase = SHU1_HEALTH_IDLE;
    st.session_started_ms = 0;
    shu1_state_update_settings(&st);
    if (emergency) {
        shu1_heater_force_off();
        set_notification(SHU1_NOTIFY_ACTION, "physical_emergency_off", "Physical long-press OFF: heater and fan forced off");
        shu1_event_log_add("warn", "physical_emergency_off", "physical OFF long press forced all outputs off");
    } else {
        // Normal OFF stops heating intent. The safety/control loop may keep fan post-run if configured.
        set_notification(SHU1_NOTIFY_INFO, "physical_safe_off", "Physical OFF: chamber heating cycles stopped");
        shu1_event_log_add("info", "physical_safe_off", "physical OFF button stopped user cycles");
    }
}

static bool output_latch_allows_start(void) {
    shu1_settings_t st = shu1_state_get_settings();
    shu1_runtime_t rt = shu1_state_get_runtime();
    if (!st.output_safety_latch_enabled) return true;
    if (st.output_safety_latch_armed && rt.output_safety_latch_ready) return true;
    set_notification(SHU1_NOTIFY_ACTION, "physical_start_blocked", "Physical start blocked: output safety latch is not ready");
    shu1_event_log_add("warn", "physical_start_blocked", "physical start blocked because output safety latch is not armed/ready");
    return false;
}

static void start_auto_mode(void) {
    if (!output_latch_allows_start()) return;
    shu1_settings_t st = shu1_state_get_settings();
    st.work_mode = SHU1_MODE_AUTO;
    st.work_on = true;
    if (st.session_started_ms == 0) st.session_started_ms = esp_timer_get_time() / 1000;
    shu1_state_update_settings(&st);
    set_notification(SHU1_NOTIFY_INFO, "physical_auto", "Physical AUTO: Auto/Symbiont-aware mode requested");
    shu1_event_log_add("info", "physical_auto", "physical AUTO button requested auto mode");
}

static void start_manual_mode(void) {
    if (!output_latch_allows_start()) return;
    shu1_settings_t st = shu1_state_get_settings();
    st.work_mode = SHU1_MODE_POWER_ON;
    st.work_on = true;
    if (st.session_started_ms == 0) st.session_started_ms = esp_timer_get_time() / 1000;
    shu1_state_update_settings(&st);
    set_notification(SHU1_NOTIFY_INFO, "physical_manual", "Physical ON: manual chamber hold requested");
    shu1_event_log_add("info", "physical_manual", "physical ON button requested manual chamber hold");
}

static void acknowledge_notifications(void) {
    shu1_settings_t st = shu1_state_get_settings();
    st.preheat_complete_pending = false;
    st.tempering_complete_pending = false;
    st.dryout_complete_pending = false;
    st.health_test_complete_pending = false;
    st.material_mismatch_pending = false;
    st.virtual_door_open_pending = false;
    st.door_open_pending = false;
    st.session_timeout_pending = false;
    st.heat_soak_complete_pending = false;
    st.filter_life_warning_pending = false;
    st.heater_wear_warning_pending = false;
    st.airflow_warning_pending = false;
    st.pla_protection_pending = false;
    st.print_risk_warning_pending = false;
    st.start_print_warning_pending = false;
    st.setup_warning_pending = false;
    st.incident_report_pending = false;
    st.symbiont_notification_pending = false;
    shu1_state_update_settings(&st);
    set_notification(SHU1_NOTIFY_INFO, "physical_ack", "Physical button acknowledged pending notifications");
    shu1_event_log_add("info", "physical_ack", "physical generic button acknowledged warnings/completions");
}

static void handle_button_event(shu1_button_t *btn, bool long_press) {
    if (!btn) return;
    ESP_LOGI(TAG, "button %s %s", btn->name, long_press ? "long" : "short");
    switch (btn->id) {
        case SHU1_PHYS_BTN_AUTO:
            if (long_press) {
                shu1_settings_t st = shu1_state_get_settings();
                st.symbiont_mode_enabled = !st.symbiont_mode_enabled;
                st.symbiont_notification_pending = true;
                shu1_state_update_settings(&st);
                set_notification(SHU1_NOTIFY_INFO, "physical_symbiont_toggle", st.symbiont_mode_enabled ? "U1 Symbiont Mode enabled from physical AUTO long press" : "U1 Symbiont Mode disabled from physical AUTO long press");
                shu1_event_log_add("info", "physical_symbiont_toggle", "physical AUTO long press toggled U1 Symbiont Mode");
            } else {
                start_auto_mode();
            }
            break;
        case SHU1_PHYS_BTN_ON:
            if (long_press) {
                shu1_settings_t st = shu1_state_get_settings();
                st.preheat_running = true;
                st.preheat_target_temp_c = st.target_temp_c;
                st.preheat_hold_min = st.preheat_hold_min > 0 ? st.preheat_hold_min : 15;
                st.preheat_phase = SHU1_PREHEAT_HEATING;
                st.preheat_complete_pending = false;
                st.work_mode = SHU1_MODE_PREHEAT;
                st.work_on = true;
                if (st.session_started_ms == 0) st.session_started_ms = esp_timer_get_time() / 1000;
                shu1_state_update_settings(&st);
                set_notification(SHU1_NOTIFY_INFO, "physical_preheat", "Physical ON long press: preheat/hold requested");
                shu1_event_log_add("info", "physical_preheat", "physical ON long press requested preheat/hold");
            } else {
                start_manual_mode();
            }
            break;
        case SHU1_PHYS_BTN_OFF:
            stop_all_user_cycles(long_press);
            break;
        case SHU1_PHYS_BTN_GENERIC:
        default:
            if (long_press) stop_all_user_cycles(true);
            else acknowledge_notifications();
            break;
    }
}

static bool fault_is_serious(shu1_heater_fault_t f) {
    return f != SHU1_HEATER_OK && f != SHU1_HEATER_DISABLED_BY_BUILD && f != SHU1_HEATER_DISABLED_BY_PROBE_LOCK;
}

static bool pending_warning(const shu1_settings_t *st) {
    return st->material_mismatch_pending || st->pla_protection_pending || st->virtual_door_open_pending ||
           st->filter_life_warning_pending || st->heater_wear_warning_pending || st->airflow_warning_pending ||
           st->print_risk_warning_pending || st->start_print_warning_pending || st->setup_warning_pending ||
           st->incident_report_pending || st->symbiont_notification_pending;
}

static void update_indicator_leds(void) {
    const int64_t now_ms = esp_timer_get_time() / 1000;
    const bool slow = ((now_ms / 700) % 2) == 0;
    const bool fast = ((now_ms / 200) % 2) == 0;

    shu1_settings_t st = shu1_state_get_settings();
    shu1_runtime_t rt = shu1_state_get_runtime();

    bool fault = fault_is_serious(rt.heater_fault) || rt.notification_level >= SHU1_NOTIFY_CRITICAL;
    bool warn = pending_warning(&st) || rt.notification_level == SHU1_NOTIFY_WARNING || rt.notification_level == SHU1_NOTIFY_ACTION;
    bool active = st.work_on || st.preheat_running || st.drying_running || st.dryout_running || st.health_test_running || st.keep_warm_active || st.pickup_active;

    bool auto_led = (st.work_mode == SHU1_MODE_AUTO) && active;
    bool on_led = active && (st.work_mode != SHU1_MODE_AUTO);
    bool off_led = !active;

    // Error LED: fast blink on critical/fault, slow blink on warnings/pending notifications.
    gpio_write_if_valid(CONFIG_SHU1_LED_ERROR_GPIO, fault ? fast : (warn ? slow : false));
    gpio_write_if_valid(CONFIG_SHU1_LED_AUTO_GPIO, auto_led ? true : ((st.work_mode == SHU1_MODE_AUTO) ? slow : false));
    gpio_write_if_valid(CONFIG_SHU1_LED_ON_GPIO, on_led ? true : (rt.fan_output_on ? slow : false));
    gpio_write_if_valid(CONFIG_SHU1_LED_OFF_GPIO, off_led ? true : (rt.fan_output_on ? slow : false));

    // Optional connectivity indicators; when we cannot verify status, keep them off.
    shu1_printer_state_t pr = shu1_state_get_printer();
    gpio_write_if_valid(CONFIG_SHU1_LED_WIFI_GPIO, pr.moonraker_connected ? true : slow);
    gpio_write_if_valid(CONFIG_SHU1_LED_BLE_GPIO, CONFIG_SHU1_ENABLE_BLE ? true : false);
}

static void poll_buttons(void) {
    int64_t now_ms = esp_timer_get_time() / 1000;
    for (size_t i = 0; i < sizeof(g_buttons) / sizeof(g_buttons[0]); ++i) {
        shu1_button_t *b = &g_buttons[i];
        if (!b->enabled) continue;
        bool raw_pressed = raw_to_pressed(gpio_get_level((gpio_num_t)b->gpio));
        if (raw_pressed != b->raw_pressed_last) {
            b->raw_pressed_last = raw_pressed;
            b->raw_changed_ms = now_ms;
        }
        if ((now_ms - b->raw_changed_ms) < CONFIG_SHU1_PHYSICAL_DEBOUNCE_MS) continue;
        if (raw_pressed != b->stable_pressed) {
            b->stable_pressed = raw_pressed;
            if (raw_pressed) {
                b->press_started_ms = now_ms;
                b->long_fired = false;
            } else {
                if (!b->long_fired) handle_button_event(b, false);
                b->press_started_ms = 0;
                b->long_fired = false;
            }
        }
        if (b->stable_pressed && !b->long_fired && b->press_started_ms > 0 &&
            (now_ms - b->press_started_ms) >= CONFIG_SHU1_PHYSICAL_LONG_PRESS_MS) {
            b->long_fired = true;
            handle_button_event(b, true);
        }
    }
}

static void physical_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "physical controls task started");
    while (1) {
        poll_buttons();
        update_indicator_leds();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SHU1_PHYSICAL_TASK_PERIOD_MS));
    }
}

esp_err_t shu1_physical_controls_start(void) {
#if !CONFIG_SHU1_ENABLE_PHYSICAL_CONTROLS
    ESP_LOGI(TAG, "physical controls disabled by config");
    return ESP_OK;
#else
    ESP_LOGW(TAG, "physical controls enabled; button/LED pins must be verified on the real Panda Breath PCB");

    configure_output(CONFIG_SHU1_LED_AUTO_GPIO);
    configure_output(CONFIG_SHU1_LED_ON_GPIO);
    configure_output(CONFIG_SHU1_LED_OFF_GPIO);
    configure_output(CONFIG_SHU1_LED_ERROR_GPIO);
    configure_output(CONFIG_SHU1_LED_WIFI_GPIO);
    configure_output(CONFIG_SHU1_LED_BLE_GPIO);

    for (size_t i = 0; i < sizeof(g_buttons) / sizeof(g_buttons[0]); ++i) {
        if (valid_gpio(g_buttons[i].gpio)) {
            g_buttons[i].enabled = true;
            configure_input(g_buttons[i].gpio);
            bool raw_pressed = raw_to_pressed(gpio_get_level((gpio_num_t)g_buttons[i].gpio));
            g_buttons[i].raw_pressed_last = raw_pressed;
            g_buttons[i].stable_pressed = raw_pressed;
            g_buttons[i].raw_changed_ms = esp_timer_get_time() / 1000;
            ESP_LOGW(TAG, "button %-7s mapped to GPIO%d", g_buttons[i].name, g_buttons[i].gpio);
        } else {
            ESP_LOGI(TAG, "button %-7s disabled; GPIO not set", g_buttons[i].name);
        }
    }

    update_indicator_leds();
    BaseType_t ok = xTaskCreate(physical_task, "shu1_phys", 4096, NULL, 5, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
#endif
}
