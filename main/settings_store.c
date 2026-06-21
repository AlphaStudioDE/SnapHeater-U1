/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "settings_store.h"
#include "app_config.h"
#include "event_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "shu1_store";
static const char *NS = "shu1";

void shu1_device_config_defaults(shu1_device_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->wifi_ssid, sizeof(cfg->wifi_ssid), "%s", CONFIG_SHU1_WIFI_SSID);
    snprintf(cfg->wifi_password, sizeof(cfg->wifi_password), "%s", CONFIG_SHU1_WIFI_PASSWORD);
    snprintf(cfg->moonraker_host, sizeof(cfg->moonraker_host), "%s", CONFIG_SHU1_MOONRAKER_HOST);
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
    if (nvs_get_i32(h, "antiwarp", &v) == ESP_OK) s->anti_warp_enabled = v != 0;
    if (nvs_get_i32(h, "largeprt", &v) == ESP_OK) s->large_print_protection_enabled = v != 0;
    if (nvs_get_i32(h, "night", &v) == ESP_OK) s->safe_overnight_enabled = v != 0;
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
    if (nvs_get_i32(h, "vdoor_act", &v) == ESP_OK) s->virtual_door_action = v;
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
    if (nvs_get_i32(h, "resume_en", &v) == ESP_OK) s->smart_resume_enabled = v != 0;
    if (nvs_get_i32(h, "resume_m", &v) == ESP_OK) s->resume_recover_min = v;
    if (nvs_get_i32(h, "pickup", &v) == ESP_OK) s->post_print_pickup_mode = v;
    if (nvs_get_i32(h, "pickup_m", &v) == ESP_OK) s->pickup_keep_warm_min = v;
    if (nvs_get_i32(h, "risk_en", &v) == ESP_OK) s->print_risk_enabled = v != 0;
    if (nvs_get_i32(h, "start_warn", &v) == ESP_OK) s->start_print_warning_enabled = v != 0;
    if (nvs_get_i32(h, "recipes", &v) == ESP_OK) s->local_recipes_enabled = v != 0;
    if (nvs_get_i32(h, "recipe", &v) == ESP_OK) s->active_recipe_slot = v;
    if (nvs_get_i32(h, "demo", &v) == ESP_OK) s->demo_mode_enabled = v != 0;
    if (nvs_get_i32(h, "safety_en", &v) == ESP_OK) s->safety_score_enabled = v != 0;
    if (nvs_get_i32(h, "setup_wiz", &v) == ESP_OK) s->first_setup_wizard_enabled = v != 0;
    if (nvs_get_i32(h, "setup_step", &v) == ESP_OK) s->first_setup_step = v;
    if (nvs_get_i32(h, "setup_done", &v) == ESP_OK) s->first_setup_complete = v != 0;
    if (nvs_get_i32(h, "hist_en", &v) == ESP_OK) s->temp_history_enabled = v != 0;
    if (nvs_get_i32(h, "hist_sec", &v) == ESP_OK) s->history_sample_period_sec = v;
    if (nvs_get_i32(h, "inc_en", &v) == ESP_OK) s->incident_report_enabled = v != 0;
    if (nvs_get_i32(h, "out_latch", &v) == ESP_OK) s->output_safety_latch_enabled = v != 0;
    if (nvs_get_i32(h, "out_arm", &v) == ESP_OK) s->output_safety_latch_armed = v != 0;
    if (nvs_get_i32(h, "h_verified", &v) == ESP_OK) s->heater_output_verified = v != 0;
    if (nvs_get_i32(h, "f_verified", &v) == ESP_OK) s->fan_output_verified = v != 0;
    if (nvs_get_i32(h, "noti_min", &v) == ESP_OK) s->notification_min_level = v;
    if (nvs_get_i32(h, "lang", &v) == ESP_OK) s->language_code = v;
    if (nvs_get_i32(h, "local", &v) == ESP_OK) s->local_only_mode = v != 0;
    if (nvs_get_i32(h, "ota_en", &v) == ESP_OK) s->ota_enabled = v != 0;
    if (nvs_get_i32(h, "showcase", &v) == ESP_OK) s->contest_showcase_mode_enabled = v != 0;
    if (nvs_get_i32(h, "sym_en", &v) == ESP_OK) s->symbiont_mode_enabled = v != 0;
    if (nvs_get_i32(h, "sym_vent", &v) == ESP_OK) s->symbiont_ventilation_allowed = v != 0;
    if (nvs_get_i32(h, "sym_safe", &v) == ESP_OK) s->symbiont_safe_control_enabled = v != 0;
    if (nvs_get_i32(h, "sym_policy", &v) == ESP_OK) s->symbiont_policy = v;
    nvs_close(h);
    ESP_LOGI(TAG, "settings loaded from NVS if present");
    return ESP_OK;
}

esp_err_t shu1_settings_store_save_settings(const shu1_settings_t *s) {
    if (!s) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    nvs_set_i32(h, "profile", s->material_profile);
    nvs_set_i32(h, "target", s->target_temp_c);
    nvs_set_i32(h, "filter", s->filter_trigger_bed_c);
    nvs_set_i32(h, "hotbed", s->heater_trigger_bed_c);
    nvs_set_i32(h, "ptc_cut", s->ptc_cutoff_c);
    nvs_set_i32(h, "dry_mode", s->drying_mode);
    nvs_set_i32(h, "custom_t", s->custom_temp_c);
    nvs_set_i32(h, "custom_h", s->custom_timer_h);
    nvs_set_i32(h, "pre_t", s->preheat_target_temp_c);
    nvs_set_i32(h, "pre_min", s->preheat_hold_min);
    nvs_set_i32(h, "max_sess", s->manual_session_max_min);
    nvs_set_i32(h, "fan_post", s->fan_postrun_min);
    nvs_set_i32(h, "temp_en", s->tempering_enabled ? 1 : 0);
    nvs_set_i32(h, "temp_end", s->tempering_end_temp_c);
    nvs_set_i32(h, "temp_min", s->tempering_duration_min);
    nvs_set_i32(h, "auto_prof", s->auto_material_profile_enabled ? 1 : 0);
    nvs_set_i32(h, "mat_warn", s->material_mismatch_warning_enabled ? 1 : 0);
    nvs_set_i32(h, "antiwarp", s->anti_warp_enabled ? 1 : 0);
    nvs_set_i32(h, "largeprt", s->large_print_protection_enabled ? 1 : 0);
    nvs_set_i32(h, "night", s->safe_overnight_enabled ? 1 : 0);
    nvs_set_i32(h, "pause_en", s->pause_hold_enabled ? 1 : 0);
    nvs_set_i32(h, "pause_st", s->pause_hold_strategy);
    nvs_set_i32(h, "pause_min", s->pause_hold_min);
    nvs_set_i32(h, "pause_low", s->pause_lower_after_min);
    nvs_set_i32(h, "pause_by", s->pause_lower_by_c);
    nvs_set_i32(h, "pause_stop", s->pause_stop_after_min);
    nvs_set_i32(h, "dryout_t", s->dryout_target_temp_c);
    nvs_set_i32(h, "dryout_m", s->dryout_duration_min);
    nvs_set_i32(h, "sched_t", s->scheduled_preheat_target_c);
    nvs_set_i32(h, "sched_h", s->scheduled_preheat_hold_min);
    nvs_set_i32(h, "finish", s->finish_conditioning_mode);
    nvs_set_i32(h, "keep_t", s->keep_warm_temp_c);
    nvs_set_i32(h, "keep_m", s->keep_warm_max_min);
    nvs_set_i32(h, "door_en", s->door_sensor_enabled ? 1 : 0);
    nvs_set_i32(h, "vdoor_en", s->virtual_door_detection_enabled ? 1 : 0);
    nvs_set_i32(h, "vdoor_win", s->virtual_door_window_sec);
    nvs_set_i32(h, "vdoor_drop", s->virtual_door_drop_c);
    nvs_set_i32(h, "vdoor_rate", s->virtual_door_rate_c_per_min);
    nvs_set_i32(h, "vdoor_min", s->virtual_door_min_base_temp_c);
    nvs_set_i32(h, "vdoor_act", s->virtual_door_action);
    nvs_set_i32(h, "health_t", s->health_test_target_c);
    nvs_set_i32(h, "health_s", s->health_test_duration_sec);
    nvs_set_i32(h, "warm_pred", s->warmup_prediction_enabled ? 1 : 0);
    nvs_set_i32(h, "soak_en", s->heat_soak_enabled ? 1 : 0);
    nvs_set_i32(h, "soak_min", s->heat_soak_min);
    nvs_set_i32(h, "soak_band", s->heat_soak_band_c);
    nvs_set_i32(h, "stab_lock", s->chamber_stability_lock_enabled ? 1 : 0);
    nvs_set_i32(h, "filt_en", s->filter_life_counter_enabled ? 1 : 0);
    nvs_set_i32(h, "filt_h", s->filter_life_limit_h);
    nvs_set_i32(h, "wear_en", s->heater_wear_tracking_enabled ? 1 : 0);
    nvs_set_i32(h, "wear_pct", s->heater_wear_warning_pct);
    nvs_set_i32(h, "air_en", s->airflow_detection_enabled ? 1 : 0);
    nvs_set_i32(h, "pla_en", s->pla_protection_enabled ? 1 : 0);
    nvs_set_i32(h, "resume_en", s->smart_resume_enabled ? 1 : 0);
    nvs_set_i32(h, "resume_m", s->resume_recover_min);
    nvs_set_i32(h, "pickup", s->post_print_pickup_mode);
    nvs_set_i32(h, "pickup_m", s->pickup_keep_warm_min);
    nvs_set_i32(h, "risk_en", s->print_risk_enabled ? 1 : 0);
    nvs_set_i32(h, "start_warn", s->start_print_warning_enabled ? 1 : 0);
    nvs_set_i32(h, "recipes", s->local_recipes_enabled ? 1 : 0);
    nvs_set_i32(h, "recipe", s->active_recipe_slot);
    nvs_set_i32(h, "demo", s->demo_mode_enabled ? 1 : 0);
    nvs_set_i32(h, "safety_en", s->safety_score_enabled ? 1 : 0);
    nvs_set_i32(h, "setup_wiz", s->first_setup_wizard_enabled ? 1 : 0);
    nvs_set_i32(h, "setup_step", s->first_setup_step);
    nvs_set_i32(h, "setup_done", s->first_setup_complete ? 1 : 0);
    nvs_set_i32(h, "hist_en", s->temp_history_enabled ? 1 : 0);
    nvs_set_i32(h, "hist_sec", s->history_sample_period_sec);
    nvs_set_i32(h, "inc_en", s->incident_report_enabled ? 1 : 0);
    nvs_set_i32(h, "out_latch", s->output_safety_latch_enabled ? 1 : 0);
    nvs_set_i32(h, "out_arm", s->output_safety_latch_armed ? 1 : 0);
    nvs_set_i32(h, "h_verified", s->heater_output_verified ? 1 : 0);
    nvs_set_i32(h, "f_verified", s->fan_output_verified ? 1 : 0);
    nvs_set_i32(h, "noti_min", s->notification_min_level);
    nvs_set_i32(h, "lang", s->language_code);
    nvs_set_i32(h, "local", s->local_only_mode ? 1 : 0);
    nvs_set_i32(h, "ota_en", s->ota_enabled ? 1 : 0);
    nvs_set_i32(h, "showcase", s->contest_showcase_mode_enabled ? 1 : 0);
    nvs_set_i32(h, "sym_en", s->symbiont_mode_enabled ? 1 : 0);
    nvs_set_i32(h, "sym_vent", s->symbiont_ventilation_allowed ? 1 : 0);
    nvs_set_i32(h, "sym_safe", s->symbiont_safe_control_enabled ? 1 : 0);
    nvs_set_i32(h, "sym_policy", s->symbiont_policy);
    err = nvs_commit(h);
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
    nvs_get_str(h, "wpass", cfg->wifi_password, &len);
    len = sizeof(cfg->moonraker_host);
    nvs_get_str(h, "moon_host", cfg->moonraker_host, &len);
    int32_t port = cfg->moonraker_port;
    if (nvs_get_i32(h, "moon_port", &port) == ESP_OK) cfg->moonraker_port = port;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t shu1_settings_store_save_device_config(const shu1_device_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    nvs_set_str(h, "ssid", cfg->wifi_ssid);
    nvs_set_str(h, "wpass", cfg->wifi_password);
    nvs_set_str(h, "moon_host", cfg->moonraker_host);
    nvs_set_i32(h, "moon_port", cfg->moonraker_port);
    err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) shu1_event_log_add("warn", "device_config_saved", "Wi-Fi/Moonraker config saved; reboot may be required");
    return err;
}

esp_err_t shu1_settings_store_factory_reset(void) {
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) shu1_event_log_add("warn", "factory_reset", "NVS configuration erased");
    return err;
}
