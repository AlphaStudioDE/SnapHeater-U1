/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

// No synchronous log sink while holding the shared control-policy guard.
#define LOG_LOCAL_LEVEL ESP_LOG_NONE
#include "physical_controls.h"
#include "app_config.h"
#include "settings_store.h"
#include "app_state.h"
#include "heater.h"
#include "safety.h"
#include "safety_latch.h"
#include "event_log.h"
#include "control_lease.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "shu1_phys";

#if CONFIG_SHU1_ENABLE_PHYSICAL_CONTROLS
typedef struct {
    const char *name;
    int id;
    int gpio;
    bool enabled;
    bool ignore_until_released;
    bool stable_pressed;
    bool raw_pressed_last;
    bool long_fired;
    int64_t raw_changed_ms;
    int64_t press_started_ms;
} shu1_button_t;

static shu1_button_t g_buttons[] = {
    {.name="power", .id=SHU1_PHYS_BTN_POWER, .gpio=CONFIG_SHU1_BUTTON_POWER_GPIO},
    {.name="auto",  .id=SHU1_PHYS_BTN_AUTO,  .gpio=CONFIG_SHU1_BUTTON_AUTO_GPIO},
    {.name="on",    .id=SHU1_PHYS_BTN_ON,    .gpio=CONFIG_SHU1_BUTTON_ON_GPIO},
    {.name="dry",   .id=SHU1_PHYS_BTN_DRY,   .gpio=CONFIG_SHU1_BUTTON_DRY_GPIO},
};
static bool g_reset_combo_active;
static int64_t g_reset_combo_started_ms;

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
    st.tempering_phase = SHU1_TEMPERING_IDLE;
    st.preheat_phase = SHU1_PREHEAT_IDLE;
    st.heat_soak_phase = SHU1_HEAT_SOAK_IDLE;
    st.health_test_phase = SHU1_HEALTH_IDLE;
    st.session_started_ms = 0;
    if (emergency) st.output_safety_latch_armed = false;
    shu1_state_update_settings_command(&st);
    shu1_safety_wake();
    if (emergency) {
        shu1_safety_wake();
        set_notification(SHU1_NOTIFY_ACTION, "physical_emergency_off", "Physical long-press OFF: heater and fan forced off");
        shu1_event_log_add("warn", "physical_emergency_off", "physical OFF long press forced all outputs off");
    } else {
        // Normal OFF stops heating intent. The safety/control loop may keep fan post-run if configured.
        set_notification(SHU1_NOTIFY_INFO, "physical_safe_off", "Physical OFF: chamber heating cycles stopped");
        shu1_event_log_add("info", "physical_safe_off", "physical OFF button stopped user cycles");
    }
}

static bool output_latch_allows_start(void) {
    if (!shu1_control_start_allowed()) {
        set_notification(SHU1_NOTIFY_ACTION, "physical_start_blocked", "Physical start blocked: fault or maintenance active");
        shu1_event_log_add("warn", "physical_start_blocked", "physical start blocked by fault or maintenance");
        return false;
    }
    return true;
}

static void start_auto_mode(void) {
    if (!output_latch_allows_start()) return;
    (void)shu1_control_claim(SHU1_CONTROL_PHYSICAL, true,
                             SHU1_CONTROL_REVISION_ANY, NULL);
    shu1_settings_t st = shu1_state_get_settings();
    shu1_settings_stop(&st);
    st.scheduled_preheat_enabled = false;
    st.scheduled_preheat_start_ms = 0;
    st.work_mode = SHU1_MODE_AUTO;
    st.work_on = true;
    if (st.session_started_ms == 0) st.session_started_ms = esp_timer_get_time() / 1000;
    shu1_state_update_settings_command(&st);
    shu1_safety_wake();
    set_notification(SHU1_NOTIFY_INFO, "physical_auto", "Physical AUTO: Auto/Symbiont-aware mode requested");
    shu1_event_log_add("info", "physical_auto", "physical AUTO button requested auto mode");
}

static void start_dry_mode(void) {
    if (!output_latch_allows_start()) return;
    (void)shu1_control_claim(SHU1_CONTROL_PHYSICAL, true,
                             SHU1_CONTROL_REVISION_ANY, NULL);
    shu1_settings_t st = shu1_state_get_settings();
    shu1_settings_stop(&st);
    st.scheduled_preheat_enabled = false;
    st.scheduled_preheat_start_ms = 0;
    st.work_mode = SHU1_MODE_DRYING;
    st.drying_running = true;
    int hours = st.drying_mode == SHU1_DRYING_CUSTOM ? st.custom_timer_h : 12;
    if (hours < 1) hours = 1;
    if (hours > 12) hours = 12;
    int minutes=hours*60;
    int limit=st.manual_session_max_min;
    if (limit<1 || limit>720) limit=720;
    if (minutes>limit) minutes=limit;
    st.drying_end_ms = esp_timer_get_time() / 1000 + (int64_t)minutes * 60000;
    st.work_on = true;
    if (st.session_started_ms == 0) st.session_started_ms = esp_timer_get_time() / 1000;
    shu1_state_update_settings_command(&st);
    shu1_safety_wake();
    set_notification(SHU1_NOTIFY_INFO, "physical_dry", "Physical DRY: filament drying requested");
    shu1_event_log_add("info", "physical_dry", "physical DRY button requested drying mode");
}

static void start_manual_mode(void) {
    if (!output_latch_allows_start()) return;
    (void)shu1_control_claim(SHU1_CONTROL_PHYSICAL, true,
                             SHU1_CONTROL_REVISION_ANY, NULL);
    shu1_settings_t st = shu1_state_get_settings();
    shu1_settings_stop(&st);
    st.scheduled_preheat_enabled = false;
    st.scheduled_preheat_start_ms = 0;
    st.work_mode = SHU1_MODE_POWER_ON;
    st.work_on = true;
    if (st.session_started_ms == 0) st.session_started_ms = esp_timer_get_time() / 1000;
    shu1_state_update_settings_command(&st);
    shu1_safety_wake();
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
    shu1_state_update_settings_command(&st);
    shu1_safety_wake();
    set_notification(SHU1_NOTIFY_INFO, "physical_ack", "Physical button acknowledged pending notifications");
    shu1_event_log_add("info", "physical_ack", "physical generic button acknowledged warnings/completions");
}

static void handle_button_event(shu1_button_t *btn, bool long_press) {
    if (!btn) return;
    SHU1_CONTROL_GUARD(policy_guard);
    ESP_LOGI(TAG, "button %s %s", btn->name, long_press ? "long" : "short");
    shu1_runtime_t rt = shu1_state_get_runtime();
    rt.physical_last_button = btn->id;
    shu1_state_update_runtime(&rt);
    // Any physical action wins over a remote lease. OFF remains unconditional;
    // a following local start claims a fresh physical session below.
    shu1_control_release_any();

    if (long_press) {
        bool was_latched = shu1_safety_latch_is_set();
        stop_all_user_cycles(true);
        if (btn->id == SHU1_PHYS_BTN_POWER && was_latched) {
            shu1_safety_latch_request_clear();
            set_notification(SHU1_NOTIFY_ACTION, "physical_fault_clear_requested",
                             "Power long press requested a safe persistent-fault clear");
        } else {
            shu1_safety_latch_trip_volatile(SHU1_HEATER_PANIC_OFF);
            set_notification(SHU1_NOTIFY_ACTION, "physical_panic_latched",
                             "Physical long press latched per-boot panic-off; explicit safe clear required");
        }
        return;
    }

    switch (btn->id) {
        case SHU1_PHYS_BTN_POWER: {
            shu1_settings_t st = shu1_state_get_settings();
            if (st.work_on) stop_all_user_cycles(false);
            else if (st.work_mode == SHU1_MODE_AUTO) start_auto_mode();
            else if (st.work_mode == SHU1_MODE_DRYING) start_dry_mode();
            else start_manual_mode();
            break;
        }
        case SHU1_PHYS_BTN_AUTO:
            if (shu1_state_get_settings().work_on &&
                shu1_state_get_settings().work_mode == SHU1_MODE_AUTO) stop_all_user_cycles(false);
            else start_auto_mode();
            break;
        case SHU1_PHYS_BTN_ON:
            if (shu1_state_get_settings().work_on &&
                shu1_state_get_settings().work_mode == SHU1_MODE_POWER_ON) stop_all_user_cycles(false);
            else start_manual_mode();
            break;
        case SHU1_PHYS_BTN_DRY:
            if (shu1_state_get_settings().work_on &&
                shu1_state_get_settings().work_mode == SHU1_MODE_DRYING) stop_all_user_cycles(false);
            else start_dry_mode();
            break;
        default:
            acknowledge_notifications();
            break;
    }
}

static bool fault_is_serious(shu1_heater_fault_t f) {
    return f != SHU1_HEATER_OK && f != SHU1_HEATER_DISABLED_BY_BUILD && f != SHU1_HEATER_DISABLED_BY_PROBE_LOCK;
}

static void update_indicator_leds(void) {
    const int64_t now_ms = esp_timer_get_time() / 1000;
    const bool slow = ((now_ms / 700) % 2) == 0;
    const bool fast = ((now_ms / 200) % 2) == 0;

    shu1_settings_t st = shu1_state_get_settings();
    shu1_runtime_t rt = shu1_state_get_runtime();

    bool fault = fault_is_serious(rt.heater_fault) || rt.notification_level >= SHU1_NOTIFY_CRITICAL;
    bool active = st.work_on || st.preheat_running || st.drying_running || st.dryout_running || st.health_test_running || st.keep_warm_active || st.pickup_active;

    bool auto_led = (st.work_mode == SHU1_MODE_AUTO) && active;
    bool on_led = active && st.work_mode != SHU1_MODE_AUTO && st.work_mode != SHU1_MODE_DRYING;
    bool dry_led = active && st.work_mode == SHU1_MODE_DRYING;

    // Error LED: fast blink on critical/fault, slow blink on warnings/pending notifications.
    gpio_write_if_valid(CONFIG_SHU1_LED_AUTO_GPIO, auto_led ? true : ((st.work_mode == SHU1_MODE_AUTO) ? slow : false));
    gpio_write_if_valid(CONFIG_SHU1_LED_ON_GPIO, on_led ? true : (rt.fan_output_on ? slow : false));
    gpio_write_if_valid(CONFIG_SHU1_LED_DRY_GPIO, dry_led);
    if (CONFIG_SHU1_ENABLE_POWER_LED)
        gpio_write_if_valid(CONFIG_SHU1_LED_POWER_GPIO, fault ? fast : true);
}

static void poll_buttons(void) {
    int64_t now_ms = esp_timer_get_time() / 1000;
    for (size_t i = 0; i < sizeof(g_buttons) / sizeof(g_buttons[0]); ++i) {
        shu1_button_t *b = &g_buttons[i];
        if (!b->enabled) continue;
        bool raw_pressed = raw_to_pressed(gpio_get_level((gpio_num_t)b->gpio));
        if (b->ignore_until_released) {
            if (!raw_pressed) {
                b->ignore_until_released = false;
                b->raw_pressed_last = false;
                b->stable_pressed = false;
                b->raw_changed_ms = now_ms;
            }
            continue;
        }
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

    // DragonBreath recovery gesture: Power+Auto, both debounced-held for 5 s.
    // Mark both consumed as soon as the combo forms so neither 2 s long-press nor
    // the trailing release can also emit its ordinary action.
    const bool combo = g_buttons[0].enabled && g_buttons[1].enabled &&
                       g_buttons[0].stable_pressed && g_buttons[1].stable_pressed;
    if (combo && !g_reset_combo_active) {
        g_reset_combo_active = true;
        g_reset_combo_started_ms = now_ms;
        g_buttons[0].long_fired = true;
        g_buttons[1].long_fired = true;
    } else if (!combo) {
        g_reset_combo_active = false;
        g_reset_combo_started_ms = 0;
    }
    if (g_reset_combo_active && now_ms - g_reset_combo_started_ms >= 5000) {
        // Attempt once per hold, using the common cold/idle reset.
        g_reset_combo_started_ms = INT64_MAX;
        esp_err_t err = shu1_settings_store_factory_reset();
        if (err != ESP_OK)
            ESP_LOGW(TAG, "factory reset rejected/failed: %s", esp_err_to_name(err));
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
#endif

esp_err_t shu1_physical_controls_start(void) {
#if !CONFIG_SHU1_ENABLE_PHYSICAL_CONTROLS
    ESP_LOGI(TAG, "physical controls disabled by config");
    return ESP_OK;
#else
    ESP_LOGW(TAG, "physical controls enabled; button/LED pins must be verified on the real Panda Breath PCB");

    configure_output(CONFIG_SHU1_LED_AUTO_GPIO);
    configure_output(CONFIG_SHU1_LED_ON_GPIO);
    configure_output(CONFIG_SHU1_LED_DRY_GPIO);
    if (CONFIG_SHU1_ENABLE_POWER_LED) configure_output(CONFIG_SHU1_LED_POWER_GPIO);

    for (size_t i = 0; i < sizeof(g_buttons) / sizeof(g_buttons[0]); ++i) {
        if (valid_gpio(g_buttons[i].gpio)) {
            g_buttons[i].enabled = true;
            configure_input(g_buttons[i].gpio);
            bool raw_pressed = raw_to_pressed(gpio_get_level((gpio_num_t)g_buttons[i].gpio));
            g_buttons[i].raw_pressed_last = raw_pressed;
            g_buttons[i].stable_pressed = raw_pressed;
            g_buttons[i].ignore_until_released = raw_pressed;
            g_buttons[i].raw_changed_ms = esp_timer_get_time() / 1000;
            ESP_LOGW(TAG, "button %-7s mapped to GPIO%d", g_buttons[i].name, g_buttons[i].gpio);
        } else {
            ESP_LOGI(TAG, "button %-7s disabled; GPIO not set", g_buttons[i].name);
        }
    }

    update_indicator_leds();
    BaseType_t ok = xTaskCreate(physical_task, "shu1_phys", CONFIG_SHU1_PHYSICAL_TASK_STACK_BYTES, NULL, 4, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
#endif
}
