/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "app_config.h"
#include "firmware_build_guard.h"
#include "app_state.h"
#include "wifi_sta.h"
#include "ntc.h"
#include "heater.h"
#include "safety.h"
#include "api_server.h"
#include "moonraker_client.h"
#include "ble_control.h"
#include "event_log.h"
#include "settings_store.h"
#include "physical_controls.h"
#include "safety_latch.h"
#include "control_lease.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"

static const char *TAG = "SnapHeater_U1";

static void sanitize_boot_settings(shu1_settings_t *st) {
    st->work_on = false;
    st->drying_running = false;
    st->drying_end_ms = 0;
    st->preheat_running = false;
    st->preheat_phase = SHU1_PREHEAT_IDLE;
    st->preheat_hold_start_ms = 0;
    st->preheat_end_ms = 0;
    st->preheat_complete_pending = false;
    st->dryout_running = false;
    st->dryout_end_ms = 0;
    st->dryout_complete_pending = false;
    st->health_test_running = false;
    st->health_test_phase = SHU1_HEALTH_IDLE;
    st->health_test_result = SHU1_HEALTH_RESULT_ABORTED;
    st->health_test_complete_pending = false;
    st->tempering_enabled = false;
    st->tempering_phase = SHU1_TEMPERING_IDLE;
    st->tempering_start_ms = 0;
    st->tempering_end_ms = 0;
    st->tempering_current_target_c = 0;
    st->tempering_complete_pending = false;
    st->scheduled_preheat_enabled = false;
    st->scheduled_preheat_start_ms = 0;
    st->scheduled_preheat_started_pending = false;
    st->keep_warm_active = false;
    st->keep_warm_end_ms = 0;
    st->pickup_active = false;
    st->pickup_pending = false;
    st->resume_recover_active = false;
    st->session_started_ms = 0;
    st->output_safety_latch_armed = false;
    st->work_mode = SHU1_MODE_AUTO;
}

void app_main(void) {
    ESP_LOGI(TAG, "%s %s booting", SHU1_FW_NAME, SHU1_FW_VERSION);
    ESP_LOGW(TAG, "Panda Breath hardware build: heater=%s fan-held-gate=%s; boot state is OFF and safety latch is disarmed.",
             CONFIG_SHU1_ENABLE_HEATER_OUTPUT ? "enabled" : "disabled",
             CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL ? "enabled" : "disabled");
    ESP_LOGI(TAG, "Accepted Panda Breath pins: heater GPIO%d, fan GPIO%d, zero-cross GPIO%d, chamber ADC%d, PTC ADC%d",
             CONFIG_SHU1_HEATER_GPIO, CONFIG_SHU1_FAN_GPIO,
             CONFIG_SHU1_ZERO_CROSS_GPIO, CONFIG_SHU1_CHAMBER_ADC_CH, CONFIG_SHU1_PTC_ADC_CH);

    ESP_ERROR_CHECK(shu1_heater_preinit_off());

    esp_err_t ret = nvs_flash_init();
    // Never auto-erase NVS: that could destroy a persisted hazardous-fault latch.
    // A corrupt/full store is fail-closed and requires deliberate service action.
    const bool nvs_ready = ret == ESP_OK;

    shu1_event_log_init();
    shu1_state_init();
    ret = shu1_settings_store_import_stock_config_if_empty();
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "stock configuration import skipped: %s", esp_err_to_name(ret));
    }
    esp_err_t latch_err = shu1_safety_latch_init();
    if (latch_err != ESP_OK) {
        ESP_LOGE(TAG, "persistent fault store unreadable; heater remains latched OFF");
    }
    ESP_ERROR_CHECK(shu1_control_lease_init());
    shu1_settings_t boot_settings = shu1_state_get_settings();
    if (shu1_settings_store_load_settings(&boot_settings) == ESP_OK) {
        sanitize_boot_settings(&boot_settings);
        shu1_state_update_settings(&boot_settings);
        shu1_event_log_add("info", "settings_loaded", "persistent settings loaded; outputs remain off after boot");
    } else {
        shu1_event_log_add("info", "settings_defaults", "using default settings");
    }
    ESP_ERROR_CHECK(shu1_heater_init());
    const esp_err_t ntc_err = shu1_ntc_init();
    if (!nvs_ready || ntc_err != ESP_OK) shu1_safety_latch_inhibit();

    // Thermal supervision precedes potentially blocking services.
    ESP_ERROR_CHECK(shu1_safety_start());
    ESP_ERROR_CHECK(shu1_safety_wait_healthy(3000));

    if (shu1_ble_start() != ESP_OK) ESP_LOGE(TAG, "BLE unavailable; thermal supervision remains active");
    if (shu1_physical_controls_start() != ESP_OK) ESP_LOGE(TAG, "Panel unavailable; thermal supervision remains active");

    ret = shu1_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi failed. REST API and Moonraker client will not start yet.");
        // Later: AP provisioning / captive portal.
    } else {
        if (shu1_api_server_start() != ESP_OK) ESP_LOGE(TAG, "REST unavailable");
        if (shu1_moonraker_start() != ESP_OK) ESP_LOGE(TAG, "Moonraker unavailable");
    }

    // The watched safety loop already completed a pass before services started.
    // Rollback also requires a compatible bootloader actually installed on the PCB.
    ret = nvs_ready && ntc_err == ESP_OK && !shu1_safety_latch_is_inhibited()
        ? esp_ota_mark_app_valid_cancel_rollback() : ESP_ERR_INVALID_STATE;
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "could not mark OTA image valid: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "boot complete");
}
