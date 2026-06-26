/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "api_server.h"
#include "app_config.h"
#include "app_state.h"
#include "profiles.h"
#include "settings_store.h"
#include "event_log.h"
#include "heater.h"
#include "board_panda_breath.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "shu1_api";

static void add_common_headers(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

static esp_err_t options_handler(httpd_req_t *req) {
    add_common_headers(req);
    httpd_resp_sendstr(req, "{}");
    return ESP_OK;
}

static void add_pin_map(cJSON *root, const char *name, bool deprecated_alias) {
    cJSON *pins = cJSON_AddObjectToObject(root, name);
    cJSON_AddStringToObject(pins, "map_name", "panda_breath_accepted");
    cJSON_AddStringToObject(pins, "safety_state", "heater_output_build_enabled_runtime_latch_required");
    cJSON_AddBoolToObject(pins, "deprecated_alias", deprecated_alias);
    cJSON_AddNumberToObject(pins, "heater_gpio", CONFIG_SHU1_HEATER_GPIO);
    cJSON_AddNumberToObject(pins, "fan_gpio", CONFIG_SHU1_FAN_GPIO);
    cJSON_AddNumberToObject(pins, "zero_cross_gpio", CONFIG_SHU1_ZERO_CROSS_GPIO);
    cJSON_AddNumberToObject(pins, "button_gpio", CONFIG_SHU1_BUTTON_GPIO);
    cJSON_AddNumberToObject(pins, "button_auto_gpio", CONFIG_SHU1_BUTTON_AUTO_GPIO);
    cJSON_AddNumberToObject(pins, "button_on_gpio", CONFIG_SHU1_BUTTON_ON_GPIO);
    cJSON_AddNumberToObject(pins, "button_off_gpio", CONFIG_SHU1_BUTTON_OFF_GPIO);
    cJSON_AddNumberToObject(pins, "button_generic_gpio", CONFIG_SHU1_BUTTON_GENERIC_GPIO);
    cJSON_AddNumberToObject(pins, "led_auto_gpio", CONFIG_SHU1_LED_AUTO_GPIO);
    cJSON_AddNumberToObject(pins, "led_on_gpio", CONFIG_SHU1_LED_ON_GPIO);
    cJSON_AddNumberToObject(pins, "led_off_gpio", CONFIG_SHU1_LED_OFF_GPIO);
    cJSON_AddNumberToObject(pins, "led_error_gpio", CONFIG_SHU1_LED_ERROR_GPIO);
    cJSON_AddNumberToObject(pins, "led_wifi_gpio", CONFIG_SHU1_LED_WIFI_GPIO);
    cJSON_AddNumberToObject(pins, "led_ble_gpio", CONFIG_SHU1_LED_BLE_GPIO);
    cJSON_AddNumberToObject(pins, "chamber_adc_channel", CONFIG_SHU1_CHAMBER_ADC_CH);
    cJSON_AddNumberToObject(pins, "ptc_adc_channel", CONFIG_SHU1_PTC_ADC_CH);
    cJSON_AddBoolToObject(pins, "heater_active_high", CONFIG_SHU1_HEATER_ACTIVE_HIGH);
    cJSON_AddBoolToObject(pins, "fan_active_high", CONFIG_SHU1_FAN_ACTIVE_HIGH);
    cJSON_AddBoolToObject(pins, "fan_triac_control", CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL);
    cJSON_AddNumberToObject(pins, "ac_mains_hz", CONFIG_SHU1_AC_MAINS_HZ);
    cJSON_AddNumberToObject(pins, "fan_triac_run_percent", CONFIG_SHU1_FAN_TRIAC_RUN_PERCENT);
    cJSON_AddNumberToObject(pins, "fan_triac_min_delay_us", CONFIG_SHU1_FAN_TRIAC_MIN_DELAY_US);
    cJSON_AddNumberToObject(pins, "fan_triac_gate_pulse_us", CONFIG_SHU1_FAN_TRIAC_GATE_PULSE_US);
    cJSON_AddStringToObject(pins, "heater_status", SHU1_BOARD_PIN_STATUS_HEATER);
    cJSON_AddStringToObject(pins, "fan_status", SHU1_BOARD_PIN_STATUS_FAN);
    cJSON_AddStringToObject(pins, "zero_cross_status", SHU1_BOARD_PIN_STATUS_ZERO_CROSS);
    cJSON_AddStringToObject(pins, "sensor_status", "accepted_panda_breath_map");
    cJSON_AddStringToObject(pins, "button_status", SHU1_BOARD_PIN_STATUS_BUTTONS);
    cJSON_AddStringToObject(pins, "led_status", SHU1_BOARD_PIN_STATUS_LEDS);
}

static cJSON *state_to_json(void) {
    shu1_state_t st;
    shu1_state_get(&st);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "fw_name", SHU1_FW_NAME);
    cJSON_AddStringToObject(root, "fw_version", SHU1_FW_VERSION);
    cJSON_AddBoolToObject(root, "heater_output_build_enabled", CONFIG_SHU1_ENABLE_HEATER_OUTPUT);
    cJSON_AddBoolToObject(root, "gpio_probe_build_enabled", CONFIG_SHU1_ENABLE_GPIO_PROBE);
    cJSON_AddNumberToObject(root, "event_count", shu1_event_log_count());

    add_pin_map(root, "hardware_pins", false);
    add_pin_map(root, "inferred_pins", true);

    cJSON *settings = cJSON_AddObjectToObject(root, "settings");
    cJSON_AddBoolToObject(settings, "work_on", st.settings.work_on);
    cJSON_AddNumberToObject(settings, "material_profile", st.settings.material_profile);
    cJSON_AddStringToObject(settings, "material_profile_name", shu1_profile_name(st.settings.material_profile));
    cJSON_AddNumberToObject(settings, "work_mode", st.settings.work_mode);
    cJSON_AddNumberToObject(settings, "set_temp", st.settings.target_temp_c);
    cJSON_AddNumberToObject(settings, "filtertemp", st.settings.filter_trigger_bed_c);
    cJSON_AddNumberToObject(settings, "hotbedtemp", st.settings.heater_trigger_bed_c);
    cJSON_AddNumberToObject(settings, "ptc_cutoff", st.settings.ptc_cutoff_c);
    cJSON_AddNumberToObject(settings, "filament_drying_mode", st.settings.drying_mode);
    cJSON_AddBoolToObject(settings, "isrunning", st.settings.drying_running);
    cJSON_AddNumberToObject(settings, "custom_temp", st.settings.custom_temp_c);
    cJSON_AddNumberToObject(settings, "custom_timer", st.settings.custom_timer_h);
    int64_t now_ms = esp_timer_get_time() / 1000;
    int64_t remaining_s = (st.settings.drying_end_ms > now_ms) ? (st.settings.drying_end_ms - now_ms) / 1000 : 0;
    cJSON_AddNumberToObject(settings, "remaining_seconds", (double)remaining_s);
    int64_t preheat_remaining_s = (st.settings.preheat_end_ms > now_ms) ? (st.settings.preheat_end_ms - now_ms) / 1000 : 0;
    cJSON_AddBoolToObject(settings, "preheat_running", st.settings.preheat_running);
    cJSON_AddNumberToObject(settings, "preheat_target", st.settings.preheat_target_temp_c);
    cJSON_AddNumberToObject(settings, "preheat_hold_min", st.settings.preheat_hold_min);
    cJSON_AddNumberToObject(settings, "preheat_phase", st.settings.preheat_phase);
    cJSON_AddNumberToObject(settings, "preheat_remaining_seconds", (double)preheat_remaining_s);
    cJSON_AddBoolToObject(settings, "preheat_complete_pending", st.settings.preheat_complete_pending);
    cJSON_AddNumberToObject(settings, "manual_session_max_min", st.settings.manual_session_max_min);
    cJSON_AddNumberToObject(settings, "session_started_ms", (double)st.settings.session_started_ms);
    cJSON_AddBoolToObject(settings, "session_timeout_pending", st.settings.session_timeout_pending);
    cJSON_AddNumberToObject(settings, "fan_postrun_min", st.settings.fan_postrun_min);
    cJSON_AddBoolToObject(settings, "tempering_enabled", st.settings.tempering_enabled);
    cJSON_AddNumberToObject(settings, "tempering_end_temp", st.settings.tempering_end_temp_c);
    cJSON_AddNumberToObject(settings, "tempering_duration_min", st.settings.tempering_duration_min);
    cJSON_AddNumberToObject(settings, "tempering_phase", st.settings.tempering_phase);
    cJSON_AddNumberToObject(settings, "tempering_start_temp", st.settings.tempering_start_temp_c);
    cJSON_AddNumberToObject(settings, "tempering_current_target", st.settings.tempering_current_target_c);
    int64_t tempering_remaining_s = (st.settings.tempering_end_ms > now_ms) ? (st.settings.tempering_end_ms - now_ms) / 1000 : 0;
    int tempering_progress_pct = 0;
    if (st.settings.tempering_phase == SHU1_TEMPERING_ACTIVE && st.settings.tempering_end_ms > st.settings.tempering_start_ms) {
        int64_t elapsed = now_ms - st.settings.tempering_start_ms;
        int64_t total = st.settings.tempering_end_ms - st.settings.tempering_start_ms;
        if (elapsed < 0) elapsed = 0;
        if (elapsed > total) elapsed = total;
        tempering_progress_pct = (int)((elapsed * 100) / total);
    } else if (st.settings.tempering_phase == SHU1_TEMPERING_COMPLETE) {
        tempering_progress_pct = 100;
    }
    cJSON_AddNumberToObject(settings, "tempering_remaining_seconds", (double)tempering_remaining_s);
    cJSON_AddNumberToObject(settings, "tempering_progress_pct", tempering_progress_pct);
    cJSON_AddBoolToObject(settings, "tempering_ramp_to_off", st.settings.tempering_end_temp_c == 0);
    cJSON_AddBoolToObject(settings, "tempering_complete_pending", st.settings.tempering_complete_pending);
    cJSON_AddBoolToObject(settings, "auto_material_profile_enabled", st.settings.auto_material_profile_enabled);
    cJSON_AddBoolToObject(settings, "material_mismatch_warning_enabled", st.settings.material_mismatch_warning_enabled);
    cJSON_AddBoolToObject(settings, "material_mismatch_pending", st.settings.material_mismatch_pending);
    cJSON_AddNumberToObject(settings, "material_mismatch_user_profile", st.settings.material_mismatch_user_profile);
    cJSON_AddStringToObject(settings, "material_mismatch_user_profile_name", shu1_profile_name(st.settings.material_mismatch_user_profile));
    cJSON_AddNumberToObject(settings, "material_mismatch_printer_profile", st.settings.material_mismatch_printer_profile);
    cJSON_AddStringToObject(settings, "material_mismatch_printer_profile_name", shu1_profile_name(st.settings.material_mismatch_printer_profile));
    cJSON_AddNumberToObject(settings, "material_mismatch_detected_ms", (double)st.settings.material_mismatch_detected_ms);
    cJSON_AddBoolToObject(settings, "anti_warp_enabled", st.settings.anti_warp_enabled);
    cJSON_AddBoolToObject(settings, "large_print_protection_enabled", st.settings.large_print_protection_enabled);
    cJSON_AddBoolToObject(settings, "safe_overnight_enabled", st.settings.safe_overnight_enabled);
    cJSON_AddBoolToObject(settings, "pause_hold_enabled", st.settings.pause_hold_enabled);
    cJSON_AddNumberToObject(settings, "pause_hold_strategy", st.settings.pause_hold_strategy);
    cJSON_AddNumberToObject(settings, "pause_hold_min", st.settings.pause_hold_min);
    cJSON_AddNumberToObject(settings, "pause_lower_after_min", st.settings.pause_lower_after_min);
    cJSON_AddNumberToObject(settings, "pause_lower_by_c", st.settings.pause_lower_by_c);
    cJSON_AddNumberToObject(settings, "pause_stop_after_min", st.settings.pause_stop_after_min);
    cJSON_AddBoolToObject(settings, "dryout_running", st.settings.dryout_running);
    cJSON_AddNumberToObject(settings, "dryout_target", st.settings.dryout_target_temp_c);
    cJSON_AddNumberToObject(settings, "dryout_duration_min", st.settings.dryout_duration_min);
    int64_t dryout_remaining_s = (st.settings.dryout_end_ms > now_ms) ? (st.settings.dryout_end_ms - now_ms) / 1000 : 0;
    cJSON_AddNumberToObject(settings, "dryout_remaining_seconds", (double)dryout_remaining_s);
    cJSON_AddBoolToObject(settings, "dryout_complete_pending", st.settings.dryout_complete_pending);
    cJSON_AddBoolToObject(settings, "scheduled_preheat_enabled", st.settings.scheduled_preheat_enabled);
    cJSON_AddNumberToObject(settings, "scheduled_preheat_delay_min", st.settings.scheduled_preheat_delay_min);
    cJSON_AddNumberToObject(settings, "scheduled_preheat_target", st.settings.scheduled_preheat_target_c);
    cJSON_AddNumberToObject(settings, "scheduled_preheat_hold_min", st.settings.scheduled_preheat_hold_min);
    int64_t sched_remaining_s = (st.settings.scheduled_preheat_start_ms > now_ms) ? (st.settings.scheduled_preheat_start_ms - now_ms) / 1000 : 0;
    cJSON_AddNumberToObject(settings, "scheduled_preheat_remaining_seconds", (double)sched_remaining_s);
    cJSON_AddBoolToObject(settings, "scheduled_preheat_started_pending", st.settings.scheduled_preheat_started_pending);
    cJSON_AddNumberToObject(settings, "finish_conditioning_mode", st.settings.finish_conditioning_mode);
    cJSON_AddBoolToObject(settings, "keep_warm_active", st.settings.keep_warm_active);
    cJSON_AddNumberToObject(settings, "keep_warm_temp", st.settings.keep_warm_temp_c);
    cJSON_AddNumberToObject(settings, "keep_warm_max_min", st.settings.keep_warm_max_min);
    cJSON_AddBoolToObject(settings, "door_sensor_enabled", st.settings.door_sensor_enabled);
    cJSON_AddBoolToObject(settings, "door_open", st.settings.door_open);
    cJSON_AddBoolToObject(settings, "door_open_pending", st.settings.door_open_pending);
    cJSON_AddBoolToObject(settings, "virtual_door_detection_enabled", st.settings.virtual_door_detection_enabled);
    cJSON_AddNumberToObject(settings, "virtual_door_window_sec", st.settings.virtual_door_window_sec);
    cJSON_AddNumberToObject(settings, "virtual_door_drop_c", st.settings.virtual_door_drop_c);
    cJSON_AddNumberToObject(settings, "virtual_door_rate_c_per_min", st.settings.virtual_door_rate_c_per_min);
    cJSON_AddNumberToObject(settings, "virtual_door_min_base_temp", st.settings.virtual_door_min_base_temp_c);
    cJSON_AddNumberToObject(settings, "virtual_door_action", st.settings.virtual_door_action);
    cJSON_AddBoolToObject(settings, "virtual_door_open", st.settings.virtual_door_open);
    cJSON_AddBoolToObject(settings, "virtual_door_open_pending", st.settings.virtual_door_open_pending);
    cJSON_AddNumberToObject(settings, "virtual_door_detected_ms", (double)st.settings.virtual_door_detected_ms);
    cJSON_AddNumberToObject(settings, "virtual_door_last_drop_c", st.settings.virtual_door_last_drop_c);
    cJSON_AddNumberToObject(settings, "virtual_door_last_rate_c_per_min", st.settings.virtual_door_last_rate_c_per_min);
    cJSON_AddBoolToObject(settings, "health_test_running", st.settings.health_test_running);
    cJSON_AddNumberToObject(settings, "health_test_phase", st.settings.health_test_phase);
    cJSON_AddNumberToObject(settings, "health_test_target", st.settings.health_test_target_c);
    cJSON_AddNumberToObject(settings, "health_test_duration_sec", st.settings.health_test_duration_sec);
    cJSON_AddNumberToObject(settings, "health_test_result", st.settings.health_test_result);
    cJSON_AddBoolToObject(settings, "health_test_complete_pending", st.settings.health_test_complete_pending);
    cJSON_AddBoolToObject(settings, "warmup_prediction_enabled", st.settings.warmup_prediction_enabled);
    cJSON_AddBoolToObject(settings, "heat_soak_enabled", st.settings.heat_soak_enabled);
    cJSON_AddNumberToObject(settings, "heat_soak_min", st.settings.heat_soak_min);
    cJSON_AddNumberToObject(settings, "heat_soak_band_c", st.settings.heat_soak_band_c);
    cJSON_AddNumberToObject(settings, "heat_soak_phase", st.settings.heat_soak_phase);
    cJSON_AddBoolToObject(settings, "heat_soak_complete_pending", st.settings.heat_soak_complete_pending);
    cJSON_AddBoolToObject(settings, "chamber_stability_lock_enabled", st.settings.chamber_stability_lock_enabled);
    cJSON_AddBoolToObject(settings, "filter_life_counter_enabled", st.settings.filter_life_counter_enabled);
    cJSON_AddNumberToObject(settings, "filter_life_limit_h", st.settings.filter_life_limit_h);
    cJSON_AddBoolToObject(settings, "filter_life_warning_pending", st.settings.filter_life_warning_pending);
    cJSON_AddBoolToObject(settings, "heater_wear_tracking_enabled", st.settings.heater_wear_tracking_enabled);
    cJSON_AddNumberToObject(settings, "heater_wear_warning_pct", st.settings.heater_wear_warning_pct);
    cJSON_AddBoolToObject(settings, "heater_wear_warning_pending", st.settings.heater_wear_warning_pending);
    cJSON_AddBoolToObject(settings, "airflow_detection_enabled", st.settings.airflow_detection_enabled);
    cJSON_AddBoolToObject(settings, "airflow_warning_pending", st.settings.airflow_warning_pending);
    cJSON_AddBoolToObject(settings, "pla_protection_enabled", st.settings.pla_protection_enabled);
    cJSON_AddBoolToObject(settings, "pla_protection_confirmed", st.settings.pla_protection_confirmed);
    cJSON_AddBoolToObject(settings, "pla_protection_pending", st.settings.pla_protection_pending);
    cJSON_AddBoolToObject(settings, "smart_resume_enabled", st.settings.smart_resume_enabled);
    cJSON_AddNumberToObject(settings, "resume_recover_min", st.settings.resume_recover_min);
    cJSON_AddBoolToObject(settings, "resume_recover_active", st.settings.resume_recover_active);
    cJSON_AddNumberToObject(settings, "post_print_pickup_mode", st.settings.post_print_pickup_mode);
    cJSON_AddNumberToObject(settings, "pickup_keep_warm_min", st.settings.pickup_keep_warm_min);
    cJSON_AddBoolToObject(settings, "pickup_active", st.settings.pickup_active);
    cJSON_AddBoolToObject(settings, "pickup_pending", st.settings.pickup_pending);
    cJSON_AddBoolToObject(settings, "print_risk_enabled", st.settings.print_risk_enabled);
    cJSON_AddNumberToObject(settings, "print_risk_score", st.settings.print_risk_score);
    cJSON_AddBoolToObject(settings, "print_risk_warning_pending", st.settings.print_risk_warning_pending);
    cJSON_AddBoolToObject(settings, "start_print_warning_enabled", st.settings.start_print_warning_enabled);
    cJSON_AddBoolToObject(settings, "start_print_warning_pending", st.settings.start_print_warning_pending);
    cJSON_AddBoolToObject(settings, "local_recipes_enabled", st.settings.local_recipes_enabled);
    cJSON_AddNumberToObject(settings, "active_recipe_slot", st.settings.active_recipe_slot);
    cJSON_AddStringToObject(settings, "active_recipe_name", st.settings.active_recipe_name);
    cJSON_AddBoolToObject(settings, "demo_mode_enabled", st.settings.demo_mode_enabled);
    cJSON_AddNumberToObject(settings, "demo_phase", st.settings.demo_phase);
    cJSON_AddBoolToObject(settings, "safety_score_enabled", st.settings.safety_score_enabled);
    cJSON_AddNumberToObject(settings, "safety_score", st.settings.safety_score);
    cJSON_AddBoolToObject(settings, "setup_validation_passed", st.settings.setup_validation_passed);
    cJSON_AddBoolToObject(settings, "setup_warning_pending", st.settings.setup_warning_pending);
    cJSON_AddBoolToObject(settings, "first_setup_wizard_enabled", st.settings.first_setup_wizard_enabled);
    cJSON_AddNumberToObject(settings, "first_setup_step", st.settings.first_setup_step);
    cJSON_AddBoolToObject(settings, "first_setup_complete", st.settings.first_setup_complete);
    cJSON_AddBoolToObject(settings, "temp_history_enabled", st.settings.temp_history_enabled);
    cJSON_AddNumberToObject(settings, "history_sample_period_sec", st.settings.history_sample_period_sec);
    cJSON_AddBoolToObject(settings, "incident_report_enabled", st.settings.incident_report_enabled);
    cJSON_AddBoolToObject(settings, "incident_report_pending", st.settings.incident_report_pending);
    cJSON_AddNumberToObject(settings, "incident_report_seq", st.settings.incident_report_seq);
    cJSON_AddStringToObject(settings, "incident_last_reason", st.settings.incident_last_reason);
    cJSON_AddBoolToObject(settings, "output_safety_latch_enabled", st.settings.output_safety_latch_enabled);
    cJSON_AddBoolToObject(settings, "output_safety_latch_armed", st.settings.output_safety_latch_armed);
    cJSON_AddBoolToObject(settings, "heater_output_verified", st.settings.heater_output_verified);
    cJSON_AddBoolToObject(settings, "fan_output_verified", st.settings.fan_output_verified);
    cJSON_AddBoolToObject(settings, "sensors_verified", st.settings.sensors_verified);
    cJSON_AddBoolToObject(settings, "moonraker_verified", st.settings.moonraker_verified);
    cJSON_AddNumberToObject(settings, "notification_min_level", st.settings.notification_min_level);
    cJSON_AddNumberToObject(settings, "language_code", st.settings.language_code);
    cJSON_AddBoolToObject(settings, "local_only_mode", st.settings.local_only_mode);
    cJSON_AddBoolToObject(settings, "ota_enabled", st.settings.ota_enabled);
    cJSON_AddBoolToObject(settings, "ota_rollback_placeholder_enabled", st.settings.ota_rollback_placeholder_enabled);
    cJSON_AddNumberToObject(settings, "ota_status", st.settings.ota_status);
    cJSON_AddBoolToObject(settings, "contest_showcase_mode_enabled", st.settings.contest_showcase_mode_enabled);
    cJSON_AddBoolToObject(settings, "symbiont_mode_enabled", st.settings.symbiont_mode_enabled);
    cJSON_AddBoolToObject(settings, "symbiont_ventilation_allowed", st.settings.symbiont_ventilation_allowed);
    cJSON_AddBoolToObject(settings, "symbiont_safe_control_enabled", st.settings.symbiont_safe_control_enabled);
    cJSON_AddNumberToObject(settings, "symbiont_policy", st.settings.symbiont_policy);
    cJSON_AddBoolToObject(settings, "symbiont_notification_pending", st.settings.symbiont_notification_pending);

    cJSON *runtime = cJSON_AddObjectToObject(root, "runtime");
    cJSON_AddNumberToObject(runtime, "warehouse_temper", st.runtime.chamber_temp_c);
    cJSON_AddNumberToObject(runtime, "ptc_temp", st.runtime.ptc_temp_c);
    cJSON_AddNumberToObject(runtime, "warehouse_raw", st.runtime.chamber_raw);
    cJSON_AddNumberToObject(runtime, "ptc_raw", st.runtime.ptc_raw);
    cJSON_AddStringToObject(runtime, "warehouse_sensor_status", shu1_sensor_status_str(st.runtime.chamber_sensor_status));
    cJSON_AddStringToObject(runtime, "ptc_sensor_status", shu1_sensor_status_str(st.runtime.ptc_sensor_status));
    cJSON_AddBoolToObject(runtime, "heater_requested", st.runtime.heater_requested);
    cJSON_AddBoolToObject(runtime, "heater_output_on", st.runtime.heater_output_on);
    cJSON_AddBoolToObject(runtime, "fan_output_on", st.runtime.fan_output_on);
    cJSON_AddStringToObject(runtime, "ptc_heater_status", shu1_heater_fault_str(st.runtime.heater_fault));
    cJSON_AddNumberToObject(runtime, "last_sensor_ms", (double)st.runtime.last_sensor_ms);
    cJSON_AddBoolToObject(runtime, "rise_detect_active", st.runtime.rise_detector.active);
    cJSON_AddNumberToObject(runtime, "rise_detect_start_ptc", st.runtime.rise_detector.start_ptc_c);
    cJSON_AddNumberToObject(runtime, "rise_detect_start_chamber", st.runtime.rise_detector.start_chamber_c);
    cJSON_AddNumberToObject(runtime, "estimated_energy_wh", st.runtime.estimated_energy_wh);
    cJSON_AddNumberToObject(runtime, "session_energy_wh", st.runtime.session_energy_wh);
    cJSON_AddNumberToObject(runtime, "heater_on_accum_ms", (double)st.runtime.heater_on_accum_ms);
    cJSON_AddBoolToObject(runtime, "stability_active", st.runtime.stability_active);
    cJSON_AddNumberToObject(runtime, "stability_score_pct", st.runtime.stability_score_pct);
    cJSON_AddNumberToObject(runtime, "stability_min", st.runtime.stability_min_c);
    cJSON_AddNumberToObject(runtime, "stability_max", st.runtime.stability_max_c);
    cJSON_AddNumberToObject(runtime, "stability_samples", st.runtime.stability_samples);
    cJSON_AddBoolToObject(runtime, "stability_report_pending", st.runtime.stability_report_pending);
    cJSON_AddStringToObject(runtime, "material_advice", st.runtime.material_advice);
    cJSON_AddStringToObject(runtime, "material_mismatch_message", st.runtime.material_mismatch_message);
    cJSON_AddNumberToObject(runtime, "physical_last_button", st.runtime.physical_last_button);
    cJSON_AddStringToObject(runtime, "physical_panel_status", st.runtime.physical_panel_status);
    cJSON_AddNumberToObject(runtime, "warmup_eta_sec", st.runtime.warmup_eta_sec);
    cJSON_AddNumberToObject(runtime, "warmup_rate_c_per_min", st.runtime.warmup_rate_c_per_min);
    cJSON_AddBoolToObject(runtime, "heat_soak_ready", st.runtime.heat_soak_ready);
    cJSON_AddNumberToObject(runtime, "heat_soak_remaining_sec", st.runtime.heat_soak_remaining_sec);
    cJSON_AddNumberToObject(runtime, "fan_on_accum_ms", (double)st.runtime.fan_on_accum_ms);
    cJSON_AddNumberToObject(runtime, "filter_life_pct", st.runtime.filter_life_pct);
    cJSON_AddNumberToObject(runtime, "last_health_ptc_rise_c", st.runtime.last_health_ptc_rise_c);
    cJSON_AddNumberToObject(runtime, "last_health_chamber_rise_c", st.runtime.last_health_chamber_rise_c);
    cJSON_AddNumberToObject(runtime, "heater_health_baseline_ptc_rise_c", st.runtime.heater_health_baseline_ptc_rise_c);
    cJSON_AddNumberToObject(runtime, "heater_wear_pct", st.runtime.heater_wear_pct);
    cJSON_AddNumberToObject(runtime, "airflow_score_pct", st.runtime.airflow_score_pct);
    cJSON_AddBoolToObject(runtime, "airflow_warning_pending", st.runtime.airflow_warning_pending);
    cJSON_AddNumberToObject(runtime, "print_risk_score", st.runtime.print_risk_score);
    cJSON_AddBoolToObject(runtime, "print_risk_warning_pending", st.runtime.print_risk_warning_pending);
    cJSON_AddStringToObject(runtime, "print_risk_message", st.runtime.print_risk_message);
    cJSON_AddBoolToObject(runtime, "start_print_warning_pending", st.runtime.start_print_warning_pending);
    cJSON_AddNumberToObject(runtime, "safety_score", st.runtime.safety_score);
    cJSON_AddBoolToObject(runtime, "setup_validation_passed", st.runtime.setup_validation_passed);
    cJSON_AddStringToObject(runtime, "safety_message", st.runtime.safety_message);
    cJSON_AddNumberToObject(runtime, "notification_level", st.runtime.notification_level);
    cJSON_AddStringToObject(runtime, "notification_code", st.runtime.notification_code);
    cJSON_AddStringToObject(runtime, "notification_message", st.runtime.notification_message);
    cJSON_AddNumberToObject(runtime, "history_count", st.runtime.history_count);
    cJSON_AddNumberToObject(runtime, "history_head", st.runtime.history_head);
    cJSON_AddBoolToObject(runtime, "output_safety_latch_ready", st.runtime.output_safety_latch_ready);
    cJSON_AddNumberToObject(runtime, "incident_report_seq", st.runtime.incident_report_seq);
    cJSON_AddStringToObject(runtime, "incident_summary", st.runtime.incident_summary);
    cJSON_AddStringToObject(runtime, "symbiont_status", st.runtime.symbiont_status);

    cJSON *printer = cJSON_AddObjectToObject(root, "printer");
    cJSON_AddBoolToObject(printer, "moonraker_connected", st.printer.moonraker_connected);
    cJSON_AddStringToObject(printer, "webhooks_state", st.printer.webhooks_state);
    cJSON_AddBoolToObject(printer, "klippy_ready", st.printer.klippy_ready);
    cJSON_AddBoolToObject(printer, "subscribed", st.printer.subscribed);
    cJSON_AddBoolToObject(printer, "autodetect_done", st.printer.autodetect_done);
    cJSON_AddStringToObject(printer, "print_state", st.printer.print_state);
    cJSON_AddStringToObject(printer, "normalized_state", st.printer.normalized_state);
    cJSON_AddStringToObject(printer, "filename", st.printer.filename);
    cJSON_AddNumberToObject(printer, "progress", st.printer.print_progress);
    cJSON_AddNumberToObject(printer, "print_duration_sec", st.printer.print_duration_sec);
    cJSON_AddNumberToObject(printer, "total_duration_sec", st.printer.total_duration_sec);
    cJSON_AddNumberToObject(printer, "bed_temp", st.printer.bed_temp);
    cJSON_AddNumberToObject(printer, "bed_target", st.printer.bed_target);
    cJSON_AddNumberToObject(printer, "extruder_temp", st.printer.extruder_temp);
    cJSON_AddNumberToObject(printer, "extruder_target", st.printer.extruder_target);
    cJSON_AddNumberToObject(printer, "active_tool", st.printer.active_tool);
    cJSON_AddStringToObject(printer, "active_tool_object", st.printer.active_tool_object);
    cJSON_AddNumberToObject(printer, "active_tool_temp", st.printer.active_tool_temp);
    cJSON_AddStringToObject(printer, "active_material", st.printer.active_material);
    cJSON_AddStringToObject(printer, "active_color_rgba", st.printer.active_color_rgba);
    cJSON_AddBoolToObject(printer, "u1_chamber_online", st.printer.chamber_sensor_online);
    cJSON_AddStringToObject(printer, "u1_chamber_object", st.printer.u1_chamber_object);
    cJSON_AddNumberToObject(printer, "u1_chamber_temp", st.printer.u1_chamber_temp);
    cJSON_AddBoolToObject(printer, "cavity_fan_online", st.printer.cavity_fan_online);
    cJSON_AddStringToObject(printer, "cavity_fan_object", st.printer.cavity_fan_object);
    cJSON_AddNumberToObject(printer, "cavity_fan_speed", st.printer.cavity_fan_speed);
    cJSON_AddNumberToObject(printer, "last_update_ms", (double)st.printer.last_update_ms);
    cJSON_AddNumberToObject(printer, "last_ws_message_ms", (double)st.printer.last_ws_message_ms);
    return root;
}

static int json_int_clamp(cJSON *root, const char *name, int current, int min, int max) {
    cJSON *item = cJSON_GetObjectItem(root, name);
    if (!cJSON_IsNumber(item)) return current;
    int v = item->valueint;
    if (v < min) v = min;
    if (v > max) v = max;
    return v;
}

static bool json_bool(cJSON *root, const char *name, bool current) {
    cJSON *item = cJSON_GetObjectItem(root, name);
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
    return current;
}

static esp_err_t read_json_body(httpd_req_t *req, cJSON **out) {
    *out = NULL;
    int len = req->content_len;
    if (len <= 0 || len > 2048) return ESP_ERR_INVALID_SIZE;
    char *buf = calloc(1, len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    int received = httpd_req_recv(req, buf, len);
    if (received <= 0) {
        free(buf);
        return ESP_FAIL;
    }
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return ESP_ERR_INVALID_ARG;
    *out = root;
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req) {
    add_common_headers(req);
    cJSON *root = state_to_json();
    char *txt = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, txt ? txt : "{}");
    cJSON_free(txt);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t settings_post_handler(httpd_req_t *req) {
    add_common_headers(req);
    cJSON *root = NULL;
    if (read_json_body(req, &root) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_json\"}");
        return ESP_OK;
    }

    shu1_settings_t st = shu1_state_get_settings();

    cJSON *profile_name = cJSON_GetObjectItem(root, "profile");
    if (cJSON_IsString(profile_name)) {
        shu1_apply_material_profile(&st, shu1_profile_from_name(profile_name->valuestring), true);
        shu1_event_log_add("info", "profile_applied", profile_name->valuestring);
    }
    cJSON *profile_id = cJSON_GetObjectItem(root, "material_profile");
    if (cJSON_IsNumber(profile_id)) {
        shu1_apply_material_profile(&st, profile_id->valueint, true);
        shu1_event_log_add("info", "profile_applied", shu1_profile_name(st.material_profile));
    }

    st.work_on = json_bool(root, "work_on", st.work_on);
    st.work_mode = json_int_clamp(root, "work_mode", st.work_mode, SHU1_MODE_AUTO, SHU1_MODE_HEALTH_TEST);
    st.target_temp_c = json_int_clamp(root, "set_temp", st.target_temp_c, 0, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.filter_trigger_bed_c = json_int_clamp(root, "filtertemp", st.filter_trigger_bed_c, 0, 120);
    st.heater_trigger_bed_c = json_int_clamp(root, "hotbedtemp", st.heater_trigger_bed_c, 30, 120);
    st.ptc_cutoff_c = json_int_clamp(root, "ptc_cutoff", st.ptc_cutoff_c, 60, SHU1_PTC_HARD_CUTOFF_C);
    st.drying_mode = json_int_clamp(root, "filament_drying_mode", st.drying_mode, SHU1_DRYING_PLA, SHU1_DRYING_CUSTOM);
    st.custom_temp_c = json_int_clamp(root, "custom_temp", st.custom_temp_c, 40, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.custom_timer_h = json_int_clamp(root, "custom_timer", st.custom_timer_h, 1, 99);
    st.preheat_target_temp_c = json_int_clamp(root, "preheat_target", st.preheat_target_temp_c, 0, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.preheat_hold_min = json_int_clamp(root, "preheat_hold_min", st.preheat_hold_min, 1, 240);
    st.manual_session_max_min = json_int_clamp(root, "manual_session_max_min", st.manual_session_max_min, 0, 720);
    st.fan_postrun_min = json_int_clamp(root, "fan_postrun_min", st.fan_postrun_min, 0, 60);
    st.tempering_enabled = json_bool(root, "tempering_enabled", st.tempering_enabled);
    st.tempering_end_temp_c = json_int_clamp(root, "tempering_end_temp", st.tempering_end_temp_c, 0, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.tempering_duration_min = json_int_clamp(root, "tempering_duration_min", st.tempering_duration_min, 0, 240);
    st.auto_material_profile_enabled = json_bool(root, "auto_material_profile_enabled", st.auto_material_profile_enabled);
    st.material_mismatch_warning_enabled = json_bool(root, "material_mismatch_warning_enabled", st.material_mismatch_warning_enabled);
    cJSON *ack_mat_mismatch = cJSON_GetObjectItem(root, "ack_material_mismatch");
    if (cJSON_IsBool(ack_mat_mismatch) && cJSON_IsTrue(ack_mat_mismatch)) st.material_mismatch_pending = false;
    cJSON *clear_mat_mismatch = cJSON_GetObjectItem(root, "clear_material_mismatch");
    if (cJSON_IsBool(clear_mat_mismatch) && cJSON_IsTrue(clear_mat_mismatch)) {
        st.material_mismatch_pending = false;
        st.material_mismatch_user_profile = SHU1_PROFILE_CUSTOM;
        st.material_mismatch_printer_profile = SHU1_PROFILE_CUSTOM;
        st.material_mismatch_detected_ms = 0;
    }
    st.anti_warp_enabled = json_bool(root, "anti_warp_enabled", st.anti_warp_enabled);
    st.large_print_protection_enabled = json_bool(root, "large_print_protection_enabled", st.large_print_protection_enabled);
    st.safe_overnight_enabled = json_bool(root, "safe_overnight_enabled", st.safe_overnight_enabled);
    st.pause_hold_enabled = json_bool(root, "pause_hold_enabled", st.pause_hold_enabled);
    st.pause_hold_strategy = json_int_clamp(root, "pause_hold_strategy", st.pause_hold_strategy, SHU1_PAUSE_HOLD_KEEP, SHU1_PAUSE_HOLD_STOP_AFTER);
    st.pause_hold_min = json_int_clamp(root, "pause_hold_min", st.pause_hold_min, 0, 720);
    st.pause_lower_after_min = json_int_clamp(root, "pause_lower_after_min", st.pause_lower_after_min, 0, 720);
    st.pause_lower_by_c = json_int_clamp(root, "pause_lower_by_c", st.pause_lower_by_c, 0, 30);
    st.pause_stop_after_min = json_int_clamp(root, "pause_stop_after_min", st.pause_stop_after_min, 0, 720);
    st.dryout_target_temp_c = json_int_clamp(root, "dryout_target", st.dryout_target_temp_c, 30, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.dryout_duration_min = json_int_clamp(root, "dryout_duration_min", st.dryout_duration_min, 1, 240);
    st.scheduled_preheat_delay_min = json_int_clamp(root, "scheduled_preheat_delay_min", st.scheduled_preheat_delay_min, 0, 1440);
    st.scheduled_preheat_target_c = json_int_clamp(root, "scheduled_preheat_target", st.scheduled_preheat_target_c, 0, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.scheduled_preheat_hold_min = json_int_clamp(root, "scheduled_preheat_hold_min", st.scheduled_preheat_hold_min, 1, 240);
    st.finish_conditioning_mode = json_int_clamp(root, "finish_conditioning_mode", st.finish_conditioning_mode, SHU1_FINISH_FAST_COOLDOWN, SHU1_FINISH_KEEP_WARM);
    st.keep_warm_temp_c = json_int_clamp(root, "keep_warm_temp", st.keep_warm_temp_c, 0, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.keep_warm_max_min = json_int_clamp(root, "keep_warm_max_min", st.keep_warm_max_min, 1, 720);
    st.door_sensor_enabled = json_bool(root, "door_sensor_enabled", st.door_sensor_enabled);
    st.virtual_door_detection_enabled = json_bool(root, "virtual_door_detection_enabled", st.virtual_door_detection_enabled);
    st.virtual_door_window_sec = json_int_clamp(root, "virtual_door_window_sec", st.virtual_door_window_sec, 10, 300);
    st.virtual_door_drop_c = json_int_clamp(root, "virtual_door_drop_c", st.virtual_door_drop_c, 1, 30);
    st.virtual_door_rate_c_per_min = json_int_clamp(root, "virtual_door_rate_c_per_min", st.virtual_door_rate_c_per_min, 1, 60);
    st.virtual_door_min_base_temp_c = json_int_clamp(root, "virtual_door_min_base_temp", st.virtual_door_min_base_temp_c, 20, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.virtual_door_action = json_int_clamp(root, "virtual_door_action", st.virtual_door_action, SHU1_VDOOR_ACTION_NOTIFY_ONLY, SHU1_VDOOR_ACTION_STOP_HEATER);
    cJSON *ack_vdoor = cJSON_GetObjectItem(root, "ack_virtual_door_open");
    if (cJSON_IsBool(ack_vdoor) && cJSON_IsTrue(ack_vdoor)) {
        st.virtual_door_open_pending = false;
        st.door_open_pending = false;
    }
    cJSON *clear_vdoor = cJSON_GetObjectItem(root, "clear_virtual_door_open");
    if (cJSON_IsBool(clear_vdoor) && cJSON_IsTrue(clear_vdoor)) {
        st.virtual_door_open = false;
        st.virtual_door_open_pending = false;
        st.door_open = false;
        st.door_open_pending = false;
    }
    st.health_test_target_c = json_int_clamp(root, "health_test_target", st.health_test_target_c, 30, SHU1_HEALTH_TEST_MAX_TARGET_C);
    st.health_test_duration_sec = json_int_clamp(root, "health_test_duration_sec", st.health_test_duration_sec, 30, 300);
    st.warmup_prediction_enabled = json_bool(root, "warmup_prediction_enabled", st.warmup_prediction_enabled);
    st.heat_soak_enabled = json_bool(root, "heat_soak_enabled", st.heat_soak_enabled);
    st.heat_soak_min = json_int_clamp(root, "heat_soak_min", st.heat_soak_min, 0, 240);
    st.heat_soak_band_c = json_int_clamp(root, "heat_soak_band_c", st.heat_soak_band_c, 1, 10);
    st.chamber_stability_lock_enabled = json_bool(root, "chamber_stability_lock_enabled", st.chamber_stability_lock_enabled);
    st.stability_lock_band_c = json_int_clamp(root, "stability_lock_band_c", st.stability_lock_band_c, 1, 10);
    st.stability_lock_min = json_int_clamp(root, "stability_lock_min", st.stability_lock_min, 0, 60);
    st.filter_life_counter_enabled = json_bool(root, "filter_life_counter_enabled", st.filter_life_counter_enabled);
    st.filter_life_limit_h = json_int_clamp(root, "filter_life_limit_h", st.filter_life_limit_h, 1, 5000);
    st.heater_wear_tracking_enabled = json_bool(root, "heater_wear_tracking_enabled", st.heater_wear_tracking_enabled);
    st.heater_wear_warning_pct = json_int_clamp(root, "heater_wear_warning_pct", st.heater_wear_warning_pct, 5, 90);
    st.airflow_detection_enabled = json_bool(root, "airflow_detection_enabled", st.airflow_detection_enabled);
    st.pla_protection_enabled = json_bool(root, "pla_protection_enabled", st.pla_protection_enabled);
    st.pla_protection_confirmed = json_bool(root, "pla_protection_confirmed", st.pla_protection_confirmed);
    st.smart_resume_enabled = json_bool(root, "smart_resume_enabled", st.smart_resume_enabled);
    st.resume_recover_min = json_int_clamp(root, "resume_recover_min", st.resume_recover_min, 0, 120);
    st.post_print_pickup_mode = json_int_clamp(root, "post_print_pickup_mode", st.post_print_pickup_mode, SHU1_PICKUP_OFF, SHU1_PICKUP_NOTIFY_ONLY);
    st.pickup_keep_warm_min = json_int_clamp(root, "pickup_keep_warm_min", st.pickup_keep_warm_min, 0, 720);
    st.print_risk_enabled = json_bool(root, "print_risk_enabled", st.print_risk_enabled);
    st.start_print_warning_enabled = json_bool(root, "start_print_warning_enabled", st.start_print_warning_enabled);
    st.local_recipes_enabled = json_bool(root, "local_recipes_enabled", st.local_recipes_enabled);
    st.active_recipe_slot = json_int_clamp(root, "active_recipe_slot", st.active_recipe_slot, 0, 8);
    st.demo_mode_enabled = json_bool(root, "demo_mode_enabled", st.demo_mode_enabled);
    st.safety_score_enabled = json_bool(root, "safety_score_enabled", st.safety_score_enabled);
    cJSON *recipe_name = cJSON_GetObjectItem(root, "active_recipe_name");
    if (cJSON_IsString(recipe_name)) snprintf(st.active_recipe_name, sizeof(st.active_recipe_name), "%s", recipe_name->valuestring);
    cJSON *ack_filter = cJSON_GetObjectItem(root, "ack_filter_life_warning");
    if (cJSON_IsBool(ack_filter) && cJSON_IsTrue(ack_filter)) st.filter_life_warning_pending = false;
    cJSON *ack_wear = cJSON_GetObjectItem(root, "ack_heater_wear_warning");
    if (cJSON_IsBool(ack_wear) && cJSON_IsTrue(ack_wear)) st.heater_wear_warning_pending = false;
    cJSON *ack_air = cJSON_GetObjectItem(root, "ack_airflow_warning");
    if (cJSON_IsBool(ack_air) && cJSON_IsTrue(ack_air)) st.airflow_warning_pending = false;
    cJSON *ack_pla = cJSON_GetObjectItem(root, "ack_pla_protection");
    if (cJSON_IsBool(ack_pla) && cJSON_IsTrue(ack_pla)) st.pla_protection_pending = false;
    cJSON *ack_risk = cJSON_GetObjectItem(root, "ack_print_risk_warning");
    if (cJSON_IsBool(ack_risk) && cJSON_IsTrue(ack_risk)) st.print_risk_warning_pending = false;
    cJSON *ack_start = cJSON_GetObjectItem(root, "ack_start_print_warning");
    if (cJSON_IsBool(ack_start) && cJSON_IsTrue(ack_start)) st.start_print_warning_pending = false;
    cJSON *ack_soak = cJSON_GetObjectItem(root, "ack_heat_soak_complete");
    if (cJSON_IsBool(ack_soak) && cJSON_IsTrue(ack_soak)) st.heat_soak_complete_pending = false;
    cJSON *ack_setup = cJSON_GetObjectItem(root, "ack_setup_warning");
    if (cJSON_IsBool(ack_setup) && cJSON_IsTrue(ack_setup)) st.setup_warning_pending = false;

    st.first_setup_wizard_enabled = json_bool(root, "first_setup_wizard_enabled", st.first_setup_wizard_enabled);
    st.first_setup_step = json_int_clamp(root, "first_setup_step", st.first_setup_step, SHU1_SETUP_STEP_BLE_CONNECTED, SHU1_SETUP_STEP_COMPLETE);
    st.first_setup_complete = json_bool(root, "first_setup_complete", st.first_setup_complete);
    st.temp_history_enabled = json_bool(root, "temp_history_enabled", st.temp_history_enabled);
    st.history_sample_period_sec = json_int_clamp(root, "history_sample_period_sec", st.history_sample_period_sec, 5, 600);
    st.incident_report_enabled = json_bool(root, "incident_report_enabled", st.incident_report_enabled);
    st.output_safety_latch_enabled = json_bool(root, "output_safety_latch_enabled", st.output_safety_latch_enabled);
    st.heater_output_verified = json_bool(root, "heater_output_verified", st.heater_output_verified);
    st.fan_output_verified = json_bool(root, "fan_output_verified", st.fan_output_verified);
    st.sensors_verified = json_bool(root, "sensors_verified", st.sensors_verified);
    st.moonraker_verified = json_bool(root, "moonraker_verified", st.moonraker_verified);
    st.notification_min_level = json_int_clamp(root, "notification_min_level", st.notification_min_level, SHU1_NOTIFY_INFO, SHU1_NOTIFY_ACTION);
    st.language_code = json_int_clamp(root, "language_code", st.language_code, SHU1_LANG_EN, SHU1_LANG_PL);
    st.local_only_mode = json_bool(root, "local_only_mode", st.local_only_mode);
    st.ota_enabled = json_bool(root, "ota_enabled", st.ota_enabled);
    st.ota_rollback_placeholder_enabled = json_bool(root, "ota_rollback_placeholder_enabled", st.ota_rollback_placeholder_enabled);
    st.contest_showcase_mode_enabled = json_bool(root, "contest_showcase_mode_enabled", st.contest_showcase_mode_enabled);
    st.symbiont_mode_enabled = json_bool(root, "symbiont_mode_enabled", st.symbiont_mode_enabled);
    st.symbiont_ventilation_allowed = json_bool(root, "symbiont_ventilation_allowed", st.symbiont_ventilation_allowed);
    st.symbiont_safe_control_enabled = json_bool(root, "symbiont_safe_control_enabled", st.symbiont_safe_control_enabled);
    st.symbiont_policy = json_int_clamp(root, "symbiont_policy", st.symbiont_policy, SHU1_SYMBIONT_POLICY_READ_ONLY, SHU1_SYMBIONT_POLICY_CLIMATE_SAFE);
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "arm_output_safety_latch"))) {
        st.output_safety_latch_armed = st.heater_output_verified && st.fan_output_verified && st.sensors_verified && (!st.moonraker_verified || st.setup_validation_passed);
        if (st.output_safety_latch_armed) shu1_event_log_add("info", "output_latch_armed", "runtime output safety latch armed by user/app after verification flags");
    }
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "disarm_output_safety_latch"))) st.output_safety_latch_armed = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_incident_report"))) st.incident_report_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_symbiont_notification"))) st.symbiont_notification_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "generate_incident_report"))) {
        st.incident_report_pending = true;
        st.incident_report_seq++;
        snprintf(st.incident_last_reason, sizeof(st.incident_last_reason), "%s", "manual_request");
        shu1_event_log_add("warn", "incident_report_requested", "manual incident report snapshot requested by app/API");
    }

    cJSON *ack_tempering = cJSON_GetObjectItem(root, "ack_tempering_complete");
    if (cJSON_IsBool(ack_tempering) && cJSON_IsTrue(ack_tempering)) {
        st.tempering_complete_pending = false;
        if (st.tempering_phase == SHU1_TEMPERING_COMPLETE) st.tempering_phase = SHU1_TEMPERING_IDLE;
    }

    cJSON *cancel_tempering = cJSON_GetObjectItem(root, "cancel_tempering");
    if (cJSON_IsBool(cancel_tempering) && cJSON_IsTrue(cancel_tempering)) {
        st.tempering_phase = SHU1_TEMPERING_IDLE;
        st.tempering_start_ms = 0;
        st.tempering_end_ms = 0;
        st.tempering_current_target_c = 0;
        st.tempering_complete_pending = false;
    }

    cJSON *ack_session = cJSON_GetObjectItem(root, "ack_session_timeout");
    if (cJSON_IsBool(ack_session) && cJSON_IsTrue(ack_session)) {
        st.session_timeout_pending = false;
    }

    cJSON *ack_preheat = cJSON_GetObjectItem(root, "ack_preheat_complete");
    if (cJSON_IsBool(ack_preheat) && cJSON_IsTrue(ack_preheat)) {
        st.preheat_complete_pending = false;
        if (!st.preheat_running && st.preheat_phase == SHU1_PREHEAT_COMPLETE) st.preheat_phase = SHU1_PREHEAT_IDLE;
    }

    cJSON *preheat = cJSON_GetObjectItem(root, "preheat_running");
    if (cJSON_IsBool(preheat)) {
        bool run = cJSON_IsTrue(preheat);
        if (run && !st.preheat_running) {
            st.preheat_running = true;
            st.preheat_phase = SHU1_PREHEAT_HEATING;
            st.preheat_hold_start_ms = 0;
            st.preheat_end_ms = 0;
            st.preheat_complete_pending = false;
            st.work_on = true;
            st.work_mode = SHU1_MODE_PREHEAT;
        } else if (!run) {
            st.preheat_running = false;
            st.preheat_phase = SHU1_PREHEAT_IDLE;
            st.preheat_hold_start_ms = 0;
            st.preheat_end_ms = 0;
            st.work_on = false;
            if (st.work_mode == SHU1_MODE_PREHEAT) st.work_mode = SHU1_MODE_POWER_ON;
        }
    }



    cJSON *dryout = cJSON_GetObjectItem(root, "dryout_running");
    if (cJSON_IsBool(dryout)) {
        bool run = cJSON_IsTrue(dryout);
        if (run && !st.dryout_running) {
            st.dryout_running = true;
            st.dryout_end_ms = 0;
            st.work_on = true;
            st.work_mode = SHU1_MODE_DRY_OUT;
            st.preheat_running = false;
            st.drying_running = false;
            st.health_test_running = false;
        } else if (!run) {
            st.dryout_running = false;
            st.dryout_end_ms = 0;
            st.work_on = false;
        }
    }

    cJSON *schedule = cJSON_GetObjectItem(root, "scheduled_preheat_enabled");
    if (cJSON_IsBool(schedule)) {
        st.scheduled_preheat_enabled = cJSON_IsTrue(schedule);
        st.scheduled_preheat_start_ms = 0;
        st.scheduled_preheat_started_pending = false;
    }

    cJSON *ack_sched = cJSON_GetObjectItem(root, "ack_scheduled_preheat_started");
    if (cJSON_IsBool(ack_sched) && cJSON_IsTrue(ack_sched)) st.scheduled_preheat_started_pending = false;

    cJSON *health = cJSON_GetObjectItem(root, "health_test_running");
    if (cJSON_IsBool(health)) {
        bool run = cJSON_IsTrue(health);
        if (run && !st.health_test_running) {
            st.health_test_running = true;
            st.health_test_phase = SHU1_HEALTH_IDLE;
            st.health_test_result = SHU1_HEALTH_RESULT_NONE;
            st.health_test_complete_pending = false;
            st.work_on = true;
            st.work_mode = SHU1_MODE_HEALTH_TEST;
            st.preheat_running = false;
            st.drying_running = false;
            st.dryout_running = false;
        } else if (!run) {
            st.health_test_running = false;
            st.health_test_phase = SHU1_HEALTH_IDLE;
            st.health_test_result = SHU1_HEALTH_RESULT_ABORTED;
            st.work_on = false;
        }
    }

    cJSON *ack_health = cJSON_GetObjectItem(root, "ack_health_test_complete");
    if (cJSON_IsBool(ack_health) && cJSON_IsTrue(ack_health)) st.health_test_complete_pending = false;
    cJSON *ack_dryout = cJSON_GetObjectItem(root, "ack_dryout_complete");
    if (cJSON_IsBool(ack_dryout) && cJSON_IsTrue(ack_dryout)) st.dryout_complete_pending = false;

    bool start_drying = json_bool(root, "isrunning", st.drying_running);
    if (start_drying && !st.drying_running) {
        int hours = st.custom_timer_h;
        if (st.drying_mode == SHU1_DRYING_PLA || st.drying_mode == SHU1_DRYING_PETG || st.drying_mode == SHU1_DRYING_ABS) hours = 12;
        st.drying_running = true;
        st.drying_end_ms = (esp_timer_get_time() / 1000) + ((int64_t)hours * 3600 * 1000);
        st.preheat_running = false;
        st.preheat_phase = SHU1_PREHEAT_IDLE;
        st.preheat_end_ms = 0;
        st.work_on = true;
        st.work_mode = SHU1_MODE_DRYING;
    } else if (!start_drying) {
        st.drying_running = false;
        st.drying_end_ms = 0;
    }

    shu1_state_update_settings(&st);
    shu1_settings_store_save_settings(&st);

    cJSON *ssid = cJSON_GetObjectItem(root, "wifi_ssid");
    cJSON *password = cJSON_GetObjectItem(root, "wifi_password");
    cJSON *mh = cJSON_GetObjectItem(root, "moonraker_host");
    cJSON *mp = cJSON_GetObjectItem(root, "moonraker_port");
    if (cJSON_IsString(ssid) || cJSON_IsString(password) || cJSON_IsString(mh) || cJSON_IsNumber(mp)) {
        shu1_device_config_t cfg;
        shu1_device_config_defaults(&cfg);
        shu1_settings_store_load_device_config(&cfg);
        if (cJSON_IsString(ssid)) snprintf(cfg.wifi_ssid, sizeof(cfg.wifi_ssid), "%s", ssid->valuestring);
        if (cJSON_IsString(password)) snprintf(cfg.wifi_password, sizeof(cfg.wifi_password), "%s", password->valuestring);
        if (cJSON_IsString(mh)) snprintf(cfg.moonraker_host, sizeof(cfg.moonraker_host), "%s", mh->valuestring);
        if (cJSON_IsNumber(mp)) cfg.moonraker_port = mp->valueint;
        shu1_settings_store_save_device_config(&cfg);
    }

    cJSON *factory_reset = cJSON_GetObjectItem(root, "factory_reset");
    if (cJSON_IsBool(factory_reset) && cJSON_IsTrue(factory_reset)) {
        shu1_settings_store_factory_reset();
    }

    cJSON_Delete(root);

    cJSON *reply = state_to_json();
    char *txt = cJSON_PrintUnformatted(reply);
    httpd_resp_sendstr(req, txt ? txt : "{}");
    cJSON_free(txt);
    cJSON_Delete(reply);
    return ESP_OK;
}

static esp_err_t probe_post_handler(httpd_req_t *req) {
    add_common_headers(req);
    cJSON *root = NULL;
    if (read_json_body(req, &root) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_json\"}");
        return ESP_OK;
    }

    const char *output = NULL;
    cJSON *out = cJSON_GetObjectItem(root, "output");
    if (cJSON_IsString(out)) output = out->valuestring;
    int duration = json_int_clamp(root, "duration_ms", 200, 50, 10000);

    shu1_output_t which = 0;
    if (output && strcmp(output, "heater") == 0) which = SHU1_OUTPUT_HEATER;
    else if (output && strcmp(output, "fan") == 0) which = SHU1_OUTPUT_FAN;

    esp_err_t err = which ? shu1_heater_probe_pulse(which, duration) : ESP_ERR_INVALID_ARG;
    cJSON_Delete(root);

    if (err == ESP_OK) {
        httpd_resp_sendstr(req, "{\"ok\":true}");
    } else if (err == ESP_ERR_INVALID_STATE) {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"gpio_probe_disabled_in_build\"}");
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"probe_failed_or_invalid_output\"}");
    }
    return ESP_OK;
}

static esp_err_t events_get_handler(httpd_req_t *req) {
    add_common_headers(req);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "count", shu1_event_log_count());
    cJSON_AddItemToObject(root, "events", shu1_event_log_to_json());
    char *txt = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, txt ? txt : "{}");
    cJSON_free(txt);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t health_get_handler(httpd_req_t *req) {
    add_common_headers(req);
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"ok\":true,\"name\":\"SnapHeater U1\",\"version\":\"%s\"}", SHU1_FW_VERSION);
    httpd_resp_sendstr(req, payload);
    return ESP_OK;
}

esp_err_t shu1_api_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = SHU1_API_PORT;
    config.ctrl_port = 32768;
    httpd_handle_t server = NULL;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG, "http server start failed");

    httpd_uri_t health = {.uri = "/api/health", .method = HTTP_GET, .handler = health_get_handler};
    httpd_uri_t status = {.uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler};
    httpd_uri_t settings_post = {.uri = "/api/settings", .method = HTTP_POST, .handler = settings_post_handler};
    httpd_uri_t settings_opt = {.uri = "/api/settings", .method = HTTP_OPTIONS, .handler = options_handler};
    httpd_uri_t probe_post = {.uri = "/api/probe", .method = HTTP_POST, .handler = probe_post_handler};
    httpd_uri_t probe_opt = {.uri = "/api/probe", .method = HTTP_OPTIONS, .handler = options_handler};
    httpd_uri_t events = {.uri = "/api/events", .method = HTTP_GET, .handler = events_get_handler};

    httpd_register_uri_handler(server, &health);
    httpd_register_uri_handler(server, &status);
    httpd_register_uri_handler(server, &settings_post);
    httpd_register_uri_handler(server, &settings_opt);
    httpd_register_uri_handler(server, &probe_post);
    httpd_register_uri_handler(server, &probe_opt);
    httpd_register_uri_handler(server, &events);
    ESP_LOGI(TAG, "REST API ready on port %d", SHU1_API_PORT);
    return ESP_OK;
}
