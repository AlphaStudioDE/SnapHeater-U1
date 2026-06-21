/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "app_config.h"
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
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "SnapHeater_U1";

void app_main(void) {
    ESP_LOGI(TAG, "%s %s booting", SHU1_FW_NAME, SHU1_FW_VERSION);
    ESP_LOGW(TAG, "Development firmware. Verify Panda Breath PCB pins before enabling heater output.");
    ESP_LOGW(TAG, "Inferred pins: heater GPIO%d, fan GPIO%d, chamber ADC%d, PTC ADC%d, button GPIO%d",
             CONFIG_SHU1_HEATER_GPIO, CONFIG_SHU1_FAN_GPIO,
             CONFIG_SHU1_CHAMBER_ADC_CH, CONFIG_SHU1_PTC_ADC_CH, CONFIG_SHU1_BUTTON_GPIO);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    shu1_event_log_init();
    shu1_state_init();
    shu1_settings_t boot_settings = shu1_state_get_settings();
    if (shu1_settings_store_load_settings(&boot_settings) == ESP_OK) {
        boot_settings.work_on = false;
        boot_settings.drying_running = false;
        boot_settings.preheat_running = false;
        boot_settings.session_started_ms = 0;
        shu1_state_update_settings(&boot_settings);
        shu1_event_log_add("info", "settings_loaded", "persistent settings loaded; outputs remain off after boot");
    } else {
        shu1_event_log_add("info", "settings_defaults", "using default settings");
    }
    ESP_ERROR_CHECK(shu1_heater_init());
    ESP_ERROR_CHECK(shu1_ntc_init());

    ESP_ERROR_CHECK(shu1_ble_start());
    ESP_ERROR_CHECK(shu1_physical_controls_start());

    ret = shu1_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi failed. REST API and Moonraker client will not start yet.");
        // Later: AP provisioning / captive portal.
    } else {
        ESP_ERROR_CHECK(shu1_api_server_start());
        ESP_ERROR_CHECK(shu1_moonraker_start());
    }

    ESP_ERROR_CHECK(shu1_safety_start());
    ESP_LOGI(TAG, "boot complete");
}
