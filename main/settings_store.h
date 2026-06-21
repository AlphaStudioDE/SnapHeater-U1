/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char wifi_ssid[33];
    char wifi_password[65];
    char moonraker_host[64];
    int moonraker_port;
} shu1_device_config_t;

void shu1_device_config_defaults(shu1_device_config_t *cfg);
esp_err_t shu1_settings_store_load_settings(shu1_settings_t *settings);
esp_err_t shu1_settings_store_save_settings(const shu1_settings_t *settings);
esp_err_t shu1_settings_store_load_device_config(shu1_device_config_t *cfg);
esp_err_t shu1_settings_store_save_device_config(const shu1_device_config_t *cfg);
esp_err_t shu1_settings_store_factory_reset(void);

#ifdef __cplusplus
}
#endif
