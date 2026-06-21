/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "app_state.h"
#include "app_config.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "esp_timer.h"

static shu1_state_t g_state;
static SemaphoreHandle_t g_state_mutex;

void shu1_state_init(void) {
    g_state_mutex = xSemaphoreCreateMutex();
    memset(&g_state, 0, sizeof(g_state));
    g_state.settings.work_on = false;
    g_state.settings.work_mode = SHU1_MODE_AUTO;
    g_state.settings.drying_mode = SHU1_DRYING_PLA;
    g_state.settings.target_temp_c = CONFIG_SHU1_TARGET_TEMP_C;
    g_state.settings.filter_trigger_bed_c = CONFIG_SHU1_FILTER_TRIGGER_BED_C;
    g_state.settings.heater_trigger_bed_c = CONFIG_SHU1_HEATER_TRIGGER_BED_C;
    g_state.settings.custom_temp_c = 50;
    g_state.settings.custom_timer_h = 12;
    g_state.settings.ptc_cutoff_c = CONFIG_SHU1_PTC_CUTOFF_C;
    g_state.settings.preheat_running = false;
    g_state.settings.preheat_target_temp_c = CONFIG_SHU1_TARGET_TEMP_C;
    g_state.settings.preheat_hold_min = 15;
    g_state.settings.preheat_phase = SHU1_PREHEAT_IDLE;
    g_state.settings.preheat_hold_start_ms = 0;
    g_state.settings.preheat_end_ms = 0;
    g_state.settings.preheat_complete_pending = false;
    g_state.settings.material_profile = SHU1_PROFILE_CUSTOM;
    g_state.settings.manual_session_max_min = SHU1_DEFAULT_MAX_SESSION_MIN;
    g_state.settings.session_started_ms = 0;
    g_state.settings.session_timeout_pending = false;
    g_state.settings.fan_postrun_min = SHU1_DEFAULT_FAN_POSTRUN_MIN;
    g_state.settings.tempering_enabled = SHU1_DEFAULT_TEMPERING_ENABLED;
    g_state.settings.tempering_end_temp_c = SHU1_DEFAULT_TEMPERING_END_TEMP_C;
    g_state.settings.tempering_duration_min = SHU1_DEFAULT_TEMPERING_DURATION_MIN;
    g_state.settings.tempering_phase = SHU1_TEMPERING_IDLE;
    g_state.settings.tempering_start_temp_c = 0;
    g_state.settings.tempering_current_target_c = 0;
    g_state.settings.tempering_start_ms = 0;
    g_state.settings.tempering_end_ms = 0;
    g_state.settings.tempering_complete_pending = false;
    // Default is warning-only, not automatic profile switching.
    // This avoids silently changing an ASA chamber profile when U1 reports PLA, etc.
    g_state.settings.auto_material_profile_enabled = false;
    g_state.settings.material_mismatch_warning_enabled = SHU1_DEFAULT_MATERIAL_MISMATCH_WARNING_ENABLED;
    g_state.settings.material_mismatch_pending = false;
    g_state.settings.material_mismatch_user_profile = SHU1_PROFILE_CUSTOM;
    g_state.settings.material_mismatch_printer_profile = SHU1_PROFILE_CUSTOM;
    g_state.settings.material_mismatch_detected_ms = 0;
    g_state.settings.anti_warp_enabled = false;
    g_state.settings.large_print_protection_enabled = false;
    g_state.settings.safe_overnight_enabled = false;
    g_state.settings.pause_hold_enabled = true;
    g_state.settings.pause_hold_strategy = SHU1_PAUSE_HOLD_KEEP;
    g_state.settings.pause_hold_min = 60;
    g_state.settings.pause_lower_after_min = 30;
    g_state.settings.pause_lower_by_c = 5;
    g_state.settings.pause_stop_after_min = 180;
    g_state.settings.pause_seen_start_ms = 0;
    g_state.settings.dryout_running = false;
    g_state.settings.dryout_target_temp_c = SHU1_DRYOUT_DEFAULT_TEMP_C;
    g_state.settings.dryout_duration_min = SHU1_DRYOUT_DEFAULT_MIN;
    g_state.settings.dryout_end_ms = 0;
    g_state.settings.dryout_complete_pending = false;
    g_state.settings.scheduled_preheat_enabled = false;
    g_state.settings.scheduled_preheat_delay_min = 0;
    g_state.settings.scheduled_preheat_target_c = CONFIG_SHU1_TARGET_TEMP_C;
    g_state.settings.scheduled_preheat_hold_min = 15;
    g_state.settings.scheduled_preheat_start_ms = 0;
    g_state.settings.scheduled_preheat_started_pending = false;
    g_state.settings.finish_conditioning_mode = SHU1_FINISH_NORMAL_COOLDOWN;
    g_state.settings.keep_warm_temp_c = SHU1_KEEP_WARM_DEFAULT_TEMP_C;
    g_state.settings.keep_warm_max_min = 60;
    g_state.settings.keep_warm_end_ms = 0;
    g_state.settings.keep_warm_active = false;
    g_state.settings.door_sensor_enabled = false;
    g_state.settings.door_open = false;
    g_state.settings.door_open_pending = false;
    g_state.settings.virtual_door_detection_enabled = SHU1_VDOOR_DEFAULT_ENABLED;
    g_state.settings.virtual_door_window_sec = SHU1_VDOOR_DEFAULT_WINDOW_SEC;
    g_state.settings.virtual_door_drop_c = SHU1_VDOOR_DEFAULT_DROP_C;
    g_state.settings.virtual_door_rate_c_per_min = SHU1_VDOOR_DEFAULT_RATE_C_PER_MIN;
    g_state.settings.virtual_door_min_base_temp_c = SHU1_VDOOR_DEFAULT_MIN_BASE_TEMP_C;
    g_state.settings.virtual_door_action = SHU1_VDOOR_DEFAULT_ACTION;
    g_state.settings.virtual_door_open = false;
    g_state.settings.virtual_door_open_pending = false;
    g_state.settings.virtual_door_detected_ms = 0;
    g_state.settings.virtual_door_window_start_ms = 0;
    g_state.settings.virtual_door_window_start_temp_c = NAN;
    g_state.settings.virtual_door_last_drop_c = 0.0f;
    g_state.settings.virtual_door_last_rate_c_per_min = 0.0f;
    g_state.settings.warmup_prediction_enabled = true;
    g_state.settings.heat_soak_enabled = false;
    g_state.settings.heat_soak_min = SHU1_DEFAULT_HEAT_SOAK_MIN;
    g_state.settings.heat_soak_band_c = SHU1_DEFAULT_HEAT_SOAK_BAND_C;
    g_state.settings.heat_soak_phase = SHU1_HEAT_SOAK_IDLE;
    g_state.settings.heat_soak_start_ms = 0;
    g_state.settings.heat_soak_end_ms = 0;
    g_state.settings.heat_soak_complete_pending = false;
    g_state.settings.chamber_stability_lock_enabled = false;
    g_state.settings.stability_lock_band_c = SHU1_DEFAULT_HEAT_SOAK_BAND_C;
    g_state.settings.stability_lock_min = SHU1_DEFAULT_STABILITY_LOCK_MIN;
    g_state.settings.filter_life_counter_enabled = true;
    g_state.settings.filter_life_limit_h = SHU1_DEFAULT_FILTER_LIFE_H;
    g_state.settings.filter_life_warning_pending = false;
    g_state.settings.heater_wear_tracking_enabled = true;
    g_state.settings.heater_wear_warning_pct = SHU1_DEFAULT_HEATER_WEAR_WARN_PCT;
    g_state.settings.heater_wear_warning_pending = false;
    g_state.settings.airflow_detection_enabled = SHU1_AIRFLOW_DEFAULT_ENABLED;
    g_state.settings.airflow_warning_pending = false;
    g_state.settings.pla_protection_enabled = true;
    g_state.settings.pla_protection_confirmed = false;
    g_state.settings.pla_protection_pending = false;
    g_state.settings.smart_resume_enabled = true;
    g_state.settings.resume_recover_min = 5;
    g_state.settings.resume_recover_active = false;
    g_state.settings.resume_recover_end_ms = 0;
    g_state.settings.post_print_pickup_mode = SHU1_PICKUP_OFF;
    g_state.settings.pickup_keep_warm_min = 60;
    g_state.settings.pickup_active = false;
    g_state.settings.pickup_pending = false;
    g_state.settings.print_risk_enabled = true;
    g_state.settings.print_risk_score = 0;
    g_state.settings.print_risk_warning_pending = false;
    g_state.settings.start_print_warning_enabled = true;
    g_state.settings.start_print_warning_pending = false;
    g_state.settings.local_recipes_enabled = true;
    g_state.settings.active_recipe_slot = 0;
    snprintf(g_state.settings.active_recipe_name, sizeof(g_state.settings.active_recipe_name), "%s", "Default");
    g_state.settings.demo_mode_enabled = false;
    g_state.settings.demo_phase = SHU1_DEMO_IDLE;
    g_state.settings.demo_started_ms = 0;
    g_state.settings.safety_score_enabled = true;
    g_state.settings.safety_score = 0;
    g_state.settings.setup_validation_passed = false;
    g_state.settings.setup_warning_pending = true;
    g_state.settings.first_setup_wizard_enabled = true;
    g_state.settings.first_setup_step = SHU1_SETUP_STEP_BLE_CONNECTED;
    g_state.settings.first_setup_complete = false;
    g_state.settings.temp_history_enabled = true;
    g_state.settings.history_sample_period_sec = SHU1_DEFAULT_HISTORY_SAMPLE_SEC;
    g_state.settings.incident_report_enabled = true;
    g_state.settings.incident_report_pending = false;
    g_state.settings.incident_report_seq = 0;
    strcpy(g_state.settings.incident_last_reason, "none");
    g_state.settings.output_safety_latch_enabled = SHU1_DEFAULT_OUTPUT_LATCH_ENABLED;
    g_state.settings.output_safety_latch_armed = false;
    g_state.settings.heater_output_verified = false;
    g_state.settings.fan_output_verified = false;
    g_state.settings.sensors_verified = false;
    g_state.settings.moonraker_verified = false;
    g_state.settings.notification_min_level = SHU1_NOTIFY_INFO;
    g_state.settings.language_code = SHU1_LANG_EN;
    g_state.settings.local_only_mode = SHU1_DEFAULT_LOCAL_ONLY_MODE;
    g_state.settings.ota_enabled = false;
    g_state.settings.ota_rollback_placeholder_enabled = true;
    g_state.settings.ota_status = SHU1_OTA_IDLE;
    g_state.settings.contest_showcase_mode_enabled = false;
    g_state.settings.symbiont_mode_enabled = false;
    g_state.settings.symbiont_ventilation_allowed = false;
    g_state.settings.symbiont_safe_control_enabled = false;
    g_state.settings.symbiont_policy = SHU1_SYMBIONT_POLICY_READ_ONLY;
    g_state.settings.symbiont_notification_pending = false;

    g_state.settings.health_test_running = false;
    g_state.settings.health_test_phase = SHU1_HEALTH_IDLE;
    g_state.settings.health_test_target_c = SHU1_HEALTH_TEST_MAX_TARGET_C;
    g_state.settings.health_test_duration_sec = SHU1_HEALTH_TEST_DEFAULT_SEC;
    g_state.settings.health_test_result = SHU1_HEALTH_RESULT_NONE;
    g_state.settings.health_test_start_ms = 0;
    g_state.settings.health_test_start_ptc_c = NAN;
    g_state.settings.health_test_start_chamber_c = NAN;
    g_state.settings.health_test_complete_pending = false;
    g_state.runtime.stability_min_c = NAN;
    g_state.runtime.stability_max_c = NAN;
    g_state.runtime.warmup_eta_sec = -1;
    g_state.runtime.warmup_rate_c_per_min = 0.0f;
    g_state.runtime.heat_soak_remaining_sec = 0;
    g_state.runtime.heat_soak_ready = false;
    g_state.runtime.fan_on_accum_ms = 0;
    g_state.runtime.filter_life_pct = 0;
    g_state.runtime.last_health_ptc_rise_c = 0.0f;
    g_state.runtime.last_health_chamber_rise_c = 0.0f;
    g_state.runtime.heater_health_baseline_ptc_rise_c = 0.0f;
    g_state.runtime.heater_wear_pct = 0;
    g_state.runtime.airflow_score_pct = 100;
    g_state.runtime.airflow_warning_pending = false;
    g_state.runtime.print_risk_score = 0;
    g_state.runtime.print_risk_warning_pending = false;
    strcpy(g_state.runtime.print_risk_message, "No print risk score yet");
    g_state.runtime.start_print_warning_pending = false;
    g_state.runtime.safety_score = 0;
    g_state.runtime.setup_validation_passed = false;
    strcpy(g_state.runtime.safety_message, "Setup validation not completed");
    g_state.runtime.notification_level = SHU1_NOTIFY_INFO;
    strcpy(g_state.runtime.notification_code, "boot");
    strcpy(g_state.runtime.notification_message, "SnapHeater U1 booted");
    g_state.runtime.notification_ms = 0;
    g_state.runtime.history_head = 0;
    g_state.runtime.history_count = 0;
    g_state.runtime.history_last_sample_ms = 0;
    g_state.runtime.incident_report_seq = 0;
    strcpy(g_state.runtime.incident_summary, "No incident report captured");
    g_state.runtime.output_safety_latch_ready = false;
    strcpy(g_state.runtime.symbiont_status, "U1 Symbiont Mode disabled; Moonraker read-only observation active");

    strcpy(g_state.runtime.material_advice, "No material advice yet");
    strcpy(g_state.runtime.material_mismatch_message, "No material/profile mismatch");
    g_state.runtime.physical_last_button = SHU1_PHYS_BTN_NONE;
    g_state.runtime.physical_last_button_ms = 0;
    strcpy(g_state.runtime.physical_panel_status, "Physical panel ready; button/LED GPIOs may be disabled until verified");
    g_state.runtime.chamber_sensor_status = SHU1_SENSOR_INVALID;
    g_state.runtime.ptc_sensor_status = SHU1_SENSOR_INVALID;
    g_state.runtime.heater_fault = CONFIG_SHU1_ENABLE_HEATER_OUTPUT ? SHU1_HEATER_OK : SHU1_HEATER_DISABLED_BY_BUILD;
    strcpy(g_state.printer.webhooks_state, "unknown");
    strcpy(g_state.printer.print_state, "standby");
    strcpy(g_state.printer.normalized_state, "idle");
    strcpy(g_state.printer.filename, "");
    strcpy(g_state.printer.active_tool_object, "extruder");
    strcpy(g_state.printer.active_material, "-");
    strcpy(g_state.printer.active_color_rgba, "#FFFFFFFF");
    strcpy(g_state.printer.u1_chamber_object, "temperature_sensor cavity");
    strcpy(g_state.printer.cavity_fan_object, "");
    g_state.printer.active_tool = 0;
    g_state.printer.active_tool_temp = NAN;
    g_state.printer.u1_chamber_temp = NAN;
    g_state.printer.cavity_fan_speed = NAN;
}

SemaphoreHandle_t shu1_state_mutex(void) { return g_state_mutex; }

void shu1_state_get(shu1_state_t *out) {
    if (!out) return;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    *out = g_state;
    xSemaphoreGive(g_state_mutex);
}

shu1_settings_t shu1_state_get_settings(void) {
    shu1_settings_t v;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    v = g_state.settings;
    xSemaphoreGive(g_state_mutex);
    return v;
}

shu1_runtime_t shu1_state_get_runtime(void) {
    shu1_runtime_t v;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    v = g_state.runtime;
    xSemaphoreGive(g_state_mutex);
    return v;
}

shu1_printer_state_t shu1_state_get_printer(void) {
    shu1_printer_state_t v;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    v = g_state.printer;
    xSemaphoreGive(g_state_mutex);
    return v;
}

void shu1_state_update_settings(const shu1_settings_t *settings) {
    if (!settings) return;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state.settings = *settings;
    xSemaphoreGive(g_state_mutex);
}

void shu1_state_update_runtime(const shu1_runtime_t *runtime) {
    if (!runtime) return;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state.runtime = *runtime;
    xSemaphoreGive(g_state_mutex);
}

void shu1_state_update_printer(const shu1_printer_state_t *printer) {
    if (!printer) return;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state.printer = *printer;
    g_state.printer.last_update_ms = esp_timer_get_time() / 1000;
    xSemaphoreGive(g_state_mutex);
}

const char *shu1_sensor_status_str(shu1_sensor_status_t status) {
    switch (status) {
    case SHU1_SENSOR_OK: return "ok";
    case SHU1_SENSOR_SHORT: return "short";
    case SHU1_SENSOR_OPEN: return "open";
    case SHU1_SENSOR_INVALID: return "invalid";
    default: return "unknown";
    }
}

const char *shu1_heater_fault_str(shu1_heater_fault_t fault) {
    switch (fault) {
    case SHU1_HEATER_OK: return "ok";
    case SHU1_HEATER_SENSOR_FAULT: return "sensor_fault";
    case SHU1_HEATER_OVERTEMP: return "overtemp";
    case SHU1_HEATER_NO_RISE: return "no_temperature_rise";
    case SHU1_HEATER_DISABLED_BY_BUILD: return "disabled_by_build";
    case SHU1_HEATER_DISABLED_BY_PROBE_LOCK: return "probe_api_disabled";
    case SHU1_HEATER_DRYING_TIMER_EXPIRED: return "drying_timer_expired";
    case SHU1_HEATER_PRINTER_NOT_READY: return "printer_not_ready";
    case SHU1_HEATER_PREHEAT_COMPLETE: return "preheat_complete";
    case SHU1_HEATER_SESSION_TIMEOUT: return "session_timeout";
    case SHU1_HEATER_DRYOUT_COMPLETE: return "dryout_complete";
    case SHU1_HEATER_HEALTH_TEST_COMPLETE: return "health_test_complete";
    case SHU1_HEATER_HEALTH_TEST_FAILED: return "health_test_failed";
    case SHU1_HEATER_DOOR_OPEN: return "door_open";
    default: return "unknown";
    }
}
