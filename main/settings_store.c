/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "settings_store.h"
#include "app_config.h"
#include "safety_latch.h"
#include "event_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include "esp_mac.h"

void shu1_device_id(char *out, size_t size) {
    uint8_t mac[6];
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        if (size) out[0] = 0;
        return;
    }
    snprintf(out, size, "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static atomic_bool g_device_config_restart_required;
typedef struct {
    uint32_t version;
    char host[64];
    uint32_t port;
    char key[129];
} moonraker_record_t;
typedef struct { uint32_t version; char ssid[33], password[65]; } wifi_record_t;
void shu1_device_config_require_restart(void) {
    atomic_store(&g_device_config_restart_required, true);
}
bool shu1_device_config_restart_required(void) {
    return atomic_load(&g_device_config_restart_required);
}

static const char *TAG = "shu1_store";
static const char *NS = "app_nvs";

esp_err_t shu1_settings_store_import_stock_config_if_empty(void) {
    nvs_handle_t own;
    if (nvs_open(NS, NVS_READONLY, &own) == ESP_OK) {
        size_t own_len = 0;
        esp_err_t own_err = nvs_get_str(own, "ssid", NULL, &own_len);
        nvs_close(own);
        if (own_err == ESP_OK && own_len > 1) return ESP_OK;
    }

    nvs_handle_t stock;
    esp_err_t err = nvs_open("app_nvs", NVS_READONLY, &stock);
    if (err != ESP_OK) return err;

    shu1_device_config_t cfg;
    shu1_device_config_defaults(&cfg);
    size_t len = sizeof(cfg.wifi_ssid);
    err = nvs_get_str(stock, "ssid", cfg.wifi_ssid, &len);
    if (err != ESP_OK || cfg.wifi_ssid[0] == '\0') {
        nvs_close(stock);
        return err == ESP_OK ? ESP_ERR_NOT_FOUND : err;
    }
    len = sizeof(cfg.wifi_password);
    (void)nvs_get_str(stock, "password", cfg.wifi_password, &len);
    len = sizeof(cfg.moonraker_host);
    (void)nvs_get_str(stock, "mk_host", cfg.moonraker_host, &len);
    uint16_t port = (uint16_t)cfg.moonraker_port;
    if (nvs_get_u16(stock, "mk_port", &port) == ESP_OK && port > 0)
        cfg.moonraker_port = port;
    nvs_close(stock);

    err = shu1_settings_store_save_device_config(&cfg);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "imported stock app_nvs Wi-Fi/Moonraker configuration without altering stock keys");
    return err;
}

void shu1_device_config_defaults(shu1_device_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->wifi_ssid, sizeof(cfg->wifi_ssid), "%s", CONFIG_SHU1_WIFI_SSID);
    snprintf(cfg->wifi_password, sizeof(cfg->wifi_password), "%s", CONFIG_SHU1_WIFI_PASSWORD);
    // No implicit connection to a sample address, even in older sdkconfig files.
    cfg->moonraker_host[0]=0;
    cfg->moonraker_port = CONFIG_SHU1_MOONRAKER_PORT;
}

static esp_err_t open_rw(nvs_handle_t *h) {
    return nvs_open(NS, NVS_READWRITE, h);
}

esp_err_t shu1_settings_store_load_settings(shu1_settings_t *s) {
    if (!s) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    int32_t v = 0;
    if (nvs_get_i32(h, "profile", &v) == ESP_OK) s->material_profile = v;
    if (nvs_get_i32(h, "target", &v) == ESP_OK) s->target_temp_c = v;
    if (nvs_get_i32(h, "filter", &v) == ESP_OK) s->filter_trigger_bed_c = v;
    if (nvs_get_i32(h, "hotbed", &v) == ESP_OK) s->heater_trigger_bed_c = v;
    if (nvs_get_i32(h, "ptc_cut", &v) == ESP_OK) s->ptc_cutoff_c = v;
    if (nvs_get_i32(h, "dry_mode", &v) == ESP_OK) s->drying_mode = v;
    if (nvs_get_i32(h, "custom_t", &v) == ESP_OK) s->custom_temp_c = v;
    if (nvs_get_i32(h, "custom_h", &v) == ESP_OK) s->custom_timer_h = v;
    if (nvs_get_i32(h, "pre_t", &v) == ESP_OK) s->preheat_target_temp_c = v;
    if (nvs_get_i32(h, "pre_min", &v) == ESP_OK) s->preheat_hold_min = v;
    if (nvs_get_i32(h, "max_sess", &v) == ESP_OK) s->manual_session_max_min = v;
    if (nvs_get_i32(h, "fan_post", &v) == ESP_OK) s->fan_postrun_min = v;
    if (nvs_get_i32(h, "temp_en", &v) == ESP_OK) s->tempering_enabled = v != 0;
    if (nvs_get_i32(h, "temp_end", &v) == ESP_OK) s->tempering_end_temp_c = v;
    if (nvs_get_i32(h, "temp_min", &v) == ESP_OK) s->tempering_duration_min = v;
    if (nvs_get_i32(h, "auto_prof", &v) == ESP_OK) s->auto_material_profile_enabled = v != 0;
    if (nvs_get_i32(h, "mat_warn", &v) == ESP_OK) s->material_mismatch_warning_enabled = v != 0;
    if (nvs_get_i32(h, "pause_en", &v) == ESP_OK) s->pause_hold_enabled = v != 0;
    if (nvs_get_i32(h, "pause_st", &v) == ESP_OK) s->pause_hold_strategy = v;
    if (nvs_get_i32(h, "pause_min", &v) == ESP_OK) s->pause_hold_min = v;
    if (nvs_get_i32(h, "pause_low", &v) == ESP_OK) s->pause_lower_after_min = v;
    if (nvs_get_i32(h, "pause_by", &v) == ESP_OK) s->pause_lower_by_c = v;
    if (nvs_get_i32(h, "pause_stop", &v) == ESP_OK) s->pause_stop_after_min = v;
    if (nvs_get_i32(h, "dryout_t", &v) == ESP_OK) s->dryout_target_temp_c = v;
    if (nvs_get_i32(h, "dryout_m", &v) == ESP_OK) s->dryout_duration_min = v;
    if (nvs_get_i32(h, "sched_t", &v) == ESP_OK) s->scheduled_preheat_target_c = v;
    if (nvs_get_i32(h, "sched_h", &v) == ESP_OK) s->scheduled_preheat_hold_min = v;
    if (nvs_get_i32(h, "finish", &v) == ESP_OK) s->finish_conditioning_mode = v;
    if (nvs_get_i32(h, "keep_t", &v) == ESP_OK) s->keep_warm_temp_c = v;
    if (nvs_get_i32(h, "keep_m", &v) == ESP_OK) s->keep_warm_max_min = v;
    if (nvs_get_i32(h, "door_en", &v) == ESP_OK) s->door_sensor_enabled = v != 0;
    if (nvs_get_i32(h, "vdoor_en", &v) == ESP_OK) s->virtual_door_detection_enabled = v != 0;
    if (nvs_get_i32(h, "vdoor_win", &v) == ESP_OK) s->virtual_door_window_sec = v;
    if (nvs_get_i32(h, "vdoor_drop", &v) == ESP_OK) s->virtual_door_drop_c = v;
    if (nvs_get_i32(h, "vdoor_rate", &v) == ESP_OK) s->virtual_door_rate_c_per_min = v;
    if (nvs_get_i32(h, "vdoor_min", &v) == ESP_OK) s->virtual_door_min_base_temp_c = v;
    s->virtual_door_action = SHU1_VDOOR_ACTION_NOTIFY_ONLY; // Migrate old stop actions.
    if (nvs_get_i32(h, "health_t", &v) == ESP_OK) s->health_test_target_c = v;
    if (nvs_get_i32(h, "health_s", &v) == ESP_OK) s->health_test_duration_sec = v;
    if (nvs_get_i32(h, "warm_pred", &v) == ESP_OK) s->warmup_prediction_enabled = v != 0;
    if (nvs_get_i32(h, "soak_en", &v) == ESP_OK) s->heat_soak_enabled = v != 0;
    if (nvs_get_i32(h, "soak_min", &v) == ESP_OK) s->heat_soak_min = v;
    if (nvs_get_i32(h, "soak_band", &v) == ESP_OK) s->heat_soak_band_c = v;
    if (nvs_get_i32(h, "stab_lock", &v) == ESP_OK) s->chamber_stability_lock_enabled = v != 0;
    if (nvs_get_i32(h, "filt_en", &v) == ESP_OK) s->filter_life_counter_enabled = v != 0;
    if (nvs_get_i32(h, "filt_h", &v) == ESP_OK) s->filter_life_limit_h = v;
    if (nvs_get_i32(h, "wear_en", &v) == ESP_OK) s->heater_wear_tracking_enabled = v != 0;
    if (nvs_get_i32(h, "wear_pct", &v) == ESP_OK) s->heater_wear_warning_pct = v;
    if (nvs_get_i32(h, "air_en", &v) == ESP_OK) s->airflow_detection_enabled = v != 0;
    if (nvs_get_i32(h, "pla_en", &v) == ESP_OK) s->pla_protection_enabled = v != 0;
    if (nvs_get_i32(h, "pickup", &v) == ESP_OK) s->post_print_pickup_mode = v;
    if (nvs_get_i32(h, "pickup_m", &v) == ESP_OK) s->pickup_keep_warm_min = v;
    if (nvs_get_i32(h, "risk_en", &v) == ESP_OK) s->print_risk_enabled = v != 0;
    if (nvs_get_i32(h, "start_warn", &v) == ESP_OK) s->start_print_warning_enabled = v != 0;
    if (nvs_get_i32(h, "safety_en", &v) == ESP_OK) s->safety_score_enabled = v != 0;
    if (nvs_get_i32(h, "setup_wiz", &v) == ESP_OK) s->first_setup_wizard_enabled = v != 0;
    if (nvs_get_i32(h, "setup_step", &v) == ESP_OK) s->first_setup_step = v;
    if (nvs_get_i32(h, "setup_done", &v) == ESP_OK) s->first_setup_complete = v != 0;
    if (nvs_get_i32(h, "hist_en", &v) == ESP_OK) s->temp_history_enabled = v != 0;
    if (nvs_get_i32(h, "hist_sec", &v) == ESP_OK) s->history_sample_period_sec = v;
    if (nvs_get_i32(h, "inc_en", &v) == ESP_OK) s->incident_report_enabled = v != 0;
    if (nvs_get_i32(h, "out_latch", &v) == ESP_OK) s->output_safety_latch_enabled = v != 0;
    // The runtime output latch is deliberately volatile. Never restore an armed
    // heater-capable state from NVS after a power cycle.
    s->output_safety_latch_armed = false;
    if (nvs_get_i32(h, "h_verified", &v) == ESP_OK) s->heater_output_verified = v != 0;
    if (nvs_get_i32(h, "f_verified", &v) == ESP_OK) s->fan_output_verified = v != 0;
    if (nvs_get_i32(h, "noti_min", &v) == ESP_OK) s->notification_min_level = v;
    if (nvs_get_i32(h, "lang", &v) == ESP_OK) s->language_code = v;
    if (nvs_get_i32(h, "local", &v) == ESP_OK) s->local_only_mode = v != 0;
    if (nvs_get_i32(h, "ota_en", &v) == ESP_OK) s->ota_enabled = v != 0;
    if (nvs_get_i32(h, "sym_en", &v) == ESP_OK) s->symbiont_mode_enabled = v != 0;
    if (nvs_get_i32(h, "sym_vent", &v) == ESP_OK) s->symbiont_ventilation_allowed = v != 0;
    if (nvs_get_i32(h, "sym_safe", &v) == ESP_OK) s->symbiont_safe_control_enabled = v != 0;
    if (nvs_get_i32(h, "sym_policy", &v) == ESP_OK) s->symbiont_policy = v;
    nvs_close(h);
    // DragonBreath-compatible shared safety key: unsigned centi-degrees C.
    if (nvs_open("app_nvs", NVS_READONLY, &h) == ESP_OK) {
        uint32_t centi_c = 0;
        if (nvs_get_u32(h, "cool_rel_c", &centi_c) == ESP_OK)
            s->cool_release_c = (int)((centi_c + 50U) / 100U);
        nvs_close(h);
    }
    if (s->cool_release_c < 30) s->cool_release_c = 30;
    if (s->cool_release_c > 65) s->cool_release_c = 65;
    ESP_LOGI(TAG, "settings loaded from NVS if present");
    return ESP_OK;
}

esp_err_t shu1_settings_store_save_settings(const shu1_settings_t *s) {
    if (!s) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    if (err == ESP_OK) err = nvs_set_i32(h, "profile", s->material_profile);
    if (err == ESP_OK) err = nvs_set_i32(h, "target", s->target_temp_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "filter", s->filter_trigger_bed_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "hotbed", s->heater_trigger_bed_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "ptc_cut", s->ptc_cutoff_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "dry_mode", s->drying_mode);
    if (err == ESP_OK) err = nvs_set_i32(h, "custom_t", s->custom_temp_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "custom_h", s->custom_timer_h);
    if (err == ESP_OK) err = nvs_set_i32(h, "pre_t", s->preheat_target_temp_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "pre_min", s->preheat_hold_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "max_sess", s->manual_session_max_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "fan_post", s->fan_postrun_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "temp_en", s->tempering_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "temp_end", s->tempering_end_temp_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "temp_min", s->tempering_duration_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "auto_prof", s->auto_material_profile_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "mat_warn", s->material_mismatch_warning_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "pause_en", s->pause_hold_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "pause_st", s->pause_hold_strategy);
    if (err == ESP_OK) err = nvs_set_i32(h, "pause_min", s->pause_hold_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "pause_low", s->pause_lower_after_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "pause_by", s->pause_lower_by_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "pause_stop", s->pause_stop_after_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "dryout_t", s->dryout_target_temp_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "dryout_m", s->dryout_duration_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "sched_t", s->scheduled_preheat_target_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "sched_h", s->scheduled_preheat_hold_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "finish", s->finish_conditioning_mode);
    if (err == ESP_OK) err = nvs_set_i32(h, "keep_t", s->keep_warm_temp_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "keep_m", s->keep_warm_max_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "door_en", s->door_sensor_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "vdoor_en", s->virtual_door_detection_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "vdoor_win", s->virtual_door_window_sec);
    if (err == ESP_OK) err = nvs_set_i32(h, "vdoor_drop", s->virtual_door_drop_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "vdoor_rate", s->virtual_door_rate_c_per_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "vdoor_min", s->virtual_door_min_base_temp_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "vdoor_act", s->virtual_door_action);
    if (err == ESP_OK) err = nvs_set_i32(h, "health_t", s->health_test_target_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "health_s", s->health_test_duration_sec);
    if (err == ESP_OK) err = nvs_set_i32(h, "warm_pred", s->warmup_prediction_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "soak_en", s->heat_soak_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "soak_min", s->heat_soak_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "soak_band", s->heat_soak_band_c);
    if (err == ESP_OK) err = nvs_set_i32(h, "stab_lock", s->chamber_stability_lock_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "filt_en", s->filter_life_counter_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "filt_h", s->filter_life_limit_h);
    if (err == ESP_OK) err = nvs_set_i32(h, "wear_en", s->heater_wear_tracking_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "wear_pct", s->heater_wear_warning_pct);
    if (err == ESP_OK) err = nvs_set_i32(h, "air_en", s->airflow_detection_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "pla_en", s->pla_protection_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "pickup", s->post_print_pickup_mode);
    if (err == ESP_OK) err = nvs_set_i32(h, "pickup_m", s->pickup_keep_warm_min);
    if (err == ESP_OK) err = nvs_set_i32(h, "risk_en", s->print_risk_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "start_warn", s->start_print_warning_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "safety_en", s->safety_score_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "setup_wiz", s->first_setup_wizard_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "setup_step", s->first_setup_step);
    if (err == ESP_OK) err = nvs_set_i32(h, "setup_done", s->first_setup_complete ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "hist_en", s->temp_history_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "hist_sec", s->history_sample_period_sec);
    if (err == ESP_OK) err = nvs_set_i32(h, "inc_en", s->incident_report_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "out_latch", s->output_safety_latch_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "out_arm", 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "h_verified", s->heater_output_verified ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "f_verified", s->fan_output_verified ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "noti_min", s->notification_min_level);
    if (err == ESP_OK) err = nvs_set_i32(h, "lang", s->language_code);
    if (err == ESP_OK) err = nvs_set_i32(h, "local", s->local_only_mode ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "ota_en", s->ota_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "sym_en", s->symbiont_mode_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "sym_vent", s->symbiont_ventilation_allowed ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "sym_safe", s->symbiont_safe_control_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(h, "sym_policy", s->symbiont_policy);
    if (err == ESP_OK) {
        int cool_c = s->cool_release_c;
        if (cool_c < 30) cool_c = 30;
        if (cool_c > 65) cool_c = 65;
        err = nvs_set_u32(h, "cool_rel_c", (uint32_t)cool_c * 100U);
        if (err == ESP_OK) err = nvs_commit(h);
    }
    nvs_close(h);
    if (err == ESP_OK) shu1_event_log_add("info", "settings_saved", "user settings saved to NVS");
    return err;
}

esp_err_t shu1_settings_store_load_device_config(shu1_device_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;
    shu1_device_config_defaults(cfg);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t len = sizeof(cfg->wifi_ssid);
    nvs_get_str(h, "ssid", cfg->wifi_ssid, &len);
    len = sizeof(cfg->wifi_password);
    nvs_get_str(h, "password", cfg->wifi_password, &len);
    wifi_record_t wifi_record={0};size_t wifi_size=sizeof(wifi_record);
    esp_err_t wifi_err=nvs_get_blob(h,"wifi_config",&wifi_record,&wifi_size);
    if (wifi_err==ESP_OK && wifi_size==sizeof(wifi_record) && wifi_record.version==1 &&
        memchr(wifi_record.ssid,0,sizeof(wifi_record.ssid)) && memchr(wifi_record.password,0,sizeof(wifi_record.password))) {
        memcpy(cfg->wifi_ssid,wifi_record.ssid,sizeof(cfg->wifi_ssid));
        memcpy(cfg->wifi_password,wifi_record.password,sizeof(cfg->wifi_password));
    } else if (wifi_err!=ESP_ERR_NVS_NOT_FOUND) {
        memset(cfg,0,sizeof(*cfg));nvs_close(h);return ESP_ERR_INVALID_STATE;
    }
    len = sizeof(cfg->moonraker_host);
    nvs_get_str(h, "mk_host", cfg->moonraker_host, &len);
    uint16_t port = (uint16_t)cfg->moonraker_port;
    if (nvs_get_u16(h, "mk_port", &port) == ESP_OK) cfg->moonraker_port = port;
    moonraker_record_t mr={0};size_t mr_size=sizeof(mr);
    esp_err_t mr_err=nvs_get_blob(h,"mk_config",&mr,&mr_size);
    if(mr_err==ESP_OK && mr_size==sizeof(mr) && mr.version==1 &&
        memchr(mr.host,0,sizeof(mr.host)) && memchr(mr.key,0,sizeof(mr.key)) && mr.port>=1 && mr.port<=65535) {
        snprintf(cfg->moonraker_host,sizeof(cfg->moonraker_host),"%s",mr.host);
        cfg->moonraker_port=(int)mr.port;
        snprintf(cfg->moonraker_api_key,sizeof(cfg->moonraker_api_key),"%s",mr.key);
    } else if(mr_err!=ESP_ERR_NVS_NOT_FOUND) {
        cfg->moonraker_host[0]=0;cfg->moonraker_api_key[0]=0;
        nvs_close(h);return ESP_ERR_INVALID_STATE; // Never fall back to a different legacy printer on corrupt new config.
    }
    nvs_close(h);
    return ESP_OK;
}

esp_err_t shu1_settings_store_save_device_config(const shu1_device_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    wifi_record_t record={.version=1};
    snprintf(record.ssid,sizeof(record.ssid),"%s",cfg->wifi_ssid);
    snprintf(record.password,sizeof(record.password),"%s",cfg->wifi_password);
    err=nvs_set_blob(h,"wifi_config",&record,sizeof(record));
    memset(&record,0,sizeof(record));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) shu1_event_log_add("info", "device_config_saved", "Wi-Fi credentials saved as one NVS record");
    return err;
}

esp_err_t shu1_settings_store_save_moonraker(const shu1_device_config_t *cfg) {
    if(!cfg) return ESP_ERR_INVALID_ARG;
    moonraker_record_t record={.version=1,.port=(uint32_t)cfg->moonraker_port};
    snprintf(record.host,sizeof(record.host),"%s",cfg->moonraker_host);
    snprintf(record.key,sizeof(record.key),"%s",cfg->moonraker_api_key);
    nvs_handle_t h;esp_err_t err=open_rw(&h);
    if(err==ESP_OK) {
        err=nvs_set_blob(h,"mk_config",&record,sizeof(record));
        if(err==ESP_OK) err=nvs_commit(h);
        nvs_close(h);
    }
    memset(&record,0,sizeof(record));
    return err;
}

esp_err_t shu1_settings_store_set_control_token(const char *token) {
    if (!token || strlen(token) < 16 || strlen(token) > 64) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "ctl_token", token);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void reset_restart_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1200));
    esp_restart();
}

esp_err_t shu1_settings_store_factory_reset(void) {
    SHU1_CONTROL_GUARD(guard);
    if (!shu1_control_maintenance_begin()) return ESP_ERR_INVALID_STATE;
    shu1_control_guard_end(&guard); // Reservation stays active; safety loop must keep running.
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err == ESP_OK) {
        // Preserve fault_latch/fault_code, NTC calibration and REST credential.
        // Never erase the whole shared stock NVS partition or namespace.
        static const char *keys[] = {
            "wifi_config",
            "mk_config",
            "profile", "target", "filter", "hotbed", "ptc_cut", "dry_mode", "custom_t", "custom_h", "pre_t", "pre_min", "max_sess", "fan_post", "temp_en", "temp_end", "temp_min", "auto_prof", "mat_warn", "antiwarp", "largeprt", "night", "pause_en", "pause_st", "pause_min", "pause_low", "pause_by", "pause_stop", "dryout_t", "dryout_m", "sched_t", "sched_h", "finish", "keep_t", "keep_m", "door_en", "vdoor_en", "vdoor_win", "vdoor_drop", "vdoor_rate", "vdoor_min", "vdoor_act", "health_t", "health_s", "warm_pred", "soak_en", "soak_min", "soak_band", "stab_lock", "filt_en", "filt_h", "wear_en", "wear_pct", "air_en", "pla_en", "resume_en", "resume_m", "pickup", "pickup_m", "risk_en", "start_warn", "recipes", "recipe", "safety_en", "setup_wiz", "setup_step", "setup_done", "hist_en", "hist_sec", "inc_en", "out_latch", "out_arm", "h_verified", "f_verified", "noti_min", "lang", "local", "ota_en", "showcase", "sym_en", "sym_vent", "sym_safe", "sym_policy", "cool_rel_c", "ssid", "password", "mk_host", "mk_port"
        };
        for (size_t i = 0; err == ESP_OK && i < sizeof(keys) / sizeof(keys[0]); ++i) {
            err = nvs_erase_key(h, keys[i]);
            if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
        }
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    // A partial failure stays inhibited, but does not unexpectedly reboot.
    if (err != ESP_OK) return err;
    // Successful reset remains in maintenance until restart.
    if (xTaskCreate(reset_restart_task, "shu1_reset", 2048, NULL, 5, NULL) != pdPASS)
        return ESP_ERR_NO_MEM; // stays inhibited; explicit power cycle required
    return err;
}
