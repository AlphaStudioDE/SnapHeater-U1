/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "ble_control.h"
#include "app_config.h"
#include "app_state.h"
#include "profiles.h"
#include "settings_store.h"
#include "command_validation.h"
#include "job_commands.h"
#include "event_log.h"
#include "heater.h"
#include "safety_latch.h"
#include "safety.h"
#include "ntc.h"
#include "control_lease.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "wifi_sta.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#if CONFIG_SHU1_ENABLE_BLE

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "os/os_mbuf.h"

static const char *TAG = "shu1_ble";

// UUIDs shared with Android SnapHeaterBleContract. BLE_UUID128_INIT expects
// little-endian bytes; canonical service UUID is 7b2f1000-4a6f-4c2d-9a1e-2f4f53485531.
static const ble_uuid128_t g_svc_uuid     = BLE_UUID128_INIT(0x31,0x55,0x48,0x53,0x4F,0x2F,0x1E,0x9A,0x2D,0x4C,0x6F,0x4A,0x00,0x10,0x2F,0x7B);
static const ble_uuid128_t g_status_uuid  = BLE_UUID128_INIT(0x31,0x55,0x48,0x53,0x4F,0x2F,0x1E,0x9A,0x2D,0x4C,0x6F,0x4A,0x01,0x10,0x2F,0x7B);
static const ble_uuid128_t g_control_uuid = BLE_UUID128_INIT(0x31,0x55,0x48,0x53,0x4F,0x2F,0x1E,0x9A,0x2D,0x4C,0x6F,0x4A,0x02,0x10,0x2F,0x7B);
static const ble_uuid128_t g_diag_uuid    = BLE_UUID128_INIT(0x31,0x55,0x48,0x53,0x4F,0x2F,0x1E,0x9A,0x2D,0x4C,0x6F,0x4A,0x03,0x10,0x2F,0x7B);

static uint8_t g_own_addr_type;
static uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t g_status_val_handle;
static bool g_ble_unlocked = false;
static bool g_ble_lease_proven = false;
static bool g_notify_requested = false;

static int ble_gap_event(struct ble_gap_event *event, void *arg);
static void ble_advertise(void);

static int clamp_i(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int json_int_clamp(cJSON *root, const char *name, int current, int min, int max) {
    cJSON *item = cJSON_GetObjectItem(root, name);
    if (!cJSON_IsNumber(item)) return current;
    return clamp_i(item->valueint, min, max);
}

static bool json_bool(cJSON *root, const char *name, bool current) {
    cJSON *item = cJSON_GetObjectItem(root, name);
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
    return current;
}

static bool has_control_fields(cJSON *root) {
    const char *names[] = {
        "safe_stop", "emergency_stop", "clear_heater_fault", "tempering_start_now",
        "warehouse_temp_offset", "ptc_temp_offset",
        "work_on", "work_mode", "set_temp", "filtertemp", "hotbedtemp", "ptc_cutoff",
        "filament_drying_mode", "isrunning", "custom_temp", "custom_timer",
        "preheat_running", "preheat_target", "preheat_hold_min", "ack_preheat_complete",
        "material_profile", "profile", "manual_session_max_min", "ack_session_timeout",
        "fan_postrun_min", "cool_release", "tempering_enabled", "tempering_end_temp", "tempering_duration_min", "ack_tempering_complete", "cancel_tempering",
        "auto_material_profile_enabled", "material_mismatch_warning_enabled", "ack_material_mismatch", "clear_material_mismatch",
        "anti_warp_enabled", "large_print_protection_enabled", "safe_overnight_enabled",
        "pause_hold_enabled", "pause_hold_strategy", "pause_hold_min", "pause_lower_after_min", "pause_lower_by_c", "pause_stop_after_min",
        "dryout_running", "dryout_target", "dryout_duration_min", "ack_dryout_complete",
        "scheduled_preheat_enabled", "scheduled_preheat_delay_min", "scheduled_preheat_target", "scheduled_preheat_hold_min", "ack_scheduled_preheat_started",
        "finish_conditioning_mode", "keep_warm_temp", "keep_warm_max_min", "door_sensor_enabled",
        "virtual_door_detection_enabled", "virtual_door_window_sec", "virtual_door_drop_c", "virtual_door_rate_c_per_min",
        "virtual_door_min_base_temp", "virtual_door_action", "ack_virtual_door_open", "clear_virtual_door_open",
        "health_test_running", "health_test_target", "health_test_duration_sec", "ack_health_test_complete",
        "warmup_prediction_enabled", "heat_soak_enabled", "heat_soak_min", "heat_soak_band_c", "ack_heat_soak_complete",
        "chamber_stability_lock_enabled", "stability_lock_band_c", "stability_lock_min",
        "filter_life_counter_enabled", "filter_life_limit_h", "ack_filter_life_warning",
        "heater_wear_tracking_enabled", "heater_wear_warning_pct", "ack_heater_wear_warning",
        "airflow_detection_enabled", "ack_airflow_warning", "pla_protection_enabled", "pla_protection_confirmed", "ack_pla_protection",
        "smart_resume_enabled", "resume_recover_min", "post_print_pickup_mode", "pickup_keep_warm_min",
        "print_risk_enabled", "ack_print_risk_warning", "start_print_warning_enabled", "ack_start_print_warning",
        "local_recipes_enabled", "active_recipe_slot", "active_recipe_name", "safety_score_enabled", "ack_setup_warning",
        "first_setup_wizard_enabled", "first_setup_step", "first_setup_complete", "temp_history_enabled", "history_sample_period_sec",
        "incident_report_enabled", "generate_incident_report", "ack_incident_report", "output_safety_latch_enabled", "heater_output_verified",
        "fan_output_verified", "sensors_verified", "moonraker_verified", "arm_output_safety_latch", "disarm_output_safety_latch",
        "notification_min_level", "language_code", "local_only_mode", "ota_enabled", "contest_showcase_mode_enabled",
        "symbiont_mode_enabled", "symbiont_ventilation_allowed", "symbiont_safe_control_enabled", "symbiont_policy", "ack_symbiont_notification",
        "wifi_ssid", "wifi_password", "moonraker_host", "moonraker_port", "factory_reset", "wifi_setup",
        "heartbeat", "lease_id", "expected_revision", "takeover"
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (cJSON_GetObjectItem(root, names[i])) return true;
    }
    return false;
}

static bool has_energy_control_field(cJSON *root) {
    static const char *const names[] = {
        "work_on", "work_mode", "set_temp", "safe_stop", "emergency_stop",
        "isrunning", "preheat_running", "dryout_running", "health_test_running",
        "scheduled_preheat_enabled", "keep_warm_active", "pickup_active",
        "tempering_enabled", "clear_heater_fault"
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (cJSON_GetObjectItemCaseSensitive(root, names[i])) return true;
    return false;
}

static bool has_session_mutation(cJSON *root) {
    for (cJSON *item = root ? root->child : NULL; item; item = item->next) {
        const char *name = item->string;
        if (!name) continue;
        if (strcmp(name, "lease_id") != 0 &&
            strcmp(name, "expected_revision") != 0 &&
            strcmp(name, "takeover") != 0) return true;
    }
    return false;
}

static uint32_t requested_revision(cJSON *root) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "expected_revision");
    return cJSON_IsNumber(item) && item->valuedouble >= 0.0 &&
           item->valuedouble < (double)UINT32_MAX &&
           item->valuedouble == (double)(uint32_t)item->valuedouble
        ? (uint32_t)item->valuedouble : SHU1_CONTROL_REVISION_ANY;
}

static bool has_valid_revision(cJSON *root) {
    return requested_revision(root) != SHU1_CONTROL_REVISION_ANY;
}

static const char *requested_lease(cJSON *root) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "lease_id");
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static void apply_safe_stop(shu1_settings_t *st) {
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

static void build_status_json(char *buf, size_t len, bool diagnostics) {
    shu1_state_t *stp = calloc(1, sizeof(*stp));
    if (!stp) {
        snprintf(buf, len, "{\"v\":\"%s\",\"err\":\"oom\"}", SHU1_FW_VERSION);
        return;
    }
    shu1_state_get(stp);
#define st (*stp)
    int64_t now_ms = esp_timer_get_time() / 1000;
    int64_t drying_remaining_s = (st.settings.drying_end_ms > now_ms) ? (st.settings.drying_end_ms - now_ms) / 1000 : 0;
    int64_t preheat_remaining_s = (st.settings.preheat_end_ms > now_ms) ? (st.settings.preheat_end_ms - now_ms) / 1000 : 0;
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

    cJSON *json = cJSON_CreateObject();
    bool json_ok = json != NULL;
    if (!diagnostics) {
        // Compact schema for GATT reads; notifications fall back to a read hint.
        if (json_ok) json_ok = cJSON_AddStringToObject(json, "v", SHU1_FW_VERSION) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "on", st.settings.work_on) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "m", st.settings.work_mode) != NULL;
        if (json_ok) json_ok = shu1_wifi_status_json(json);
        cJSON *prefs = json_ok ? cJSON_AddObjectToObject(json, "prefs") : NULL;
        json_ok = json_ok && prefs != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "virtual_door_detection_enabled", st.settings.virtual_door_detection_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "heat_soak_enabled", st.settings.heat_soak_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "auto_material_profile_enabled", st.settings.auto_material_profile_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "material_mismatch_warning_enabled", st.settings.material_mismatch_warning_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "pla_protection_enabled", st.settings.pla_protection_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "anti_warp_enabled", st.settings.anti_warp_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "large_print_protection_enabled", st.settings.large_print_protection_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "safe_overnight_enabled", st.settings.safe_overnight_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "pause_hold_enabled", st.settings.pause_hold_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "smart_resume_enabled", st.settings.smart_resume_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "start_print_warning_enabled", st.settings.start_print_warning_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "airflow_detection_enabled", st.settings.airflow_detection_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "temp_history_enabled", st.settings.temp_history_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "incident_report_enabled", st.settings.incident_report_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "local_recipes_enabled", st.settings.local_recipes_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "local_only_mode", st.settings.local_only_mode) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "contest_showcase_mode_enabled", st.settings.contest_showcase_mode_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "symbiont_mode_enabled", st.settings.symbiont_mode_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "symbiont_ventilation_allowed", st.settings.symbiont_ventilation_allowed) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(prefs, "scheduled_preheat_enabled", st.settings.scheduled_preheat_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(prefs, "scheduled_preheat_delay_min", st.settings.scheduled_preheat_delay_min) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(prefs, "scheduled_preheat_target", st.settings.scheduled_preheat_target_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(prefs, "scheduled_preheat_hold_min", st.settings.scheduled_preheat_hold_min) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "phhold", st.settings.preheat_hold_min) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "heater_build", CONFIG_SHU1_ENABLE_HEATER_OUTPUT) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "vdoor_enabled", st.settings.virtual_door_detection_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "vdoor", st.settings.virtual_door_open) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "vdoor_pending", st.settings.virtual_door_open_pending) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "vdoor_ms", (double)st.settings.virtual_door_detected_ms) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "vdoor_drop", st.settings.virtual_door_last_drop_c) != NULL;
        char device_id[13];
        shu1_device_id(device_id, sizeof(device_id));
        if (json_ok) json_ok = cJSON_AddStringToObject(json, "device_id", device_id) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "tmpen", st.settings.tempering_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "finish", st.settings.finish_conditioning_mode) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "tmph", st.settings.tempering_phase) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "tmpmin", st.settings.tempering_duration_min) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "set", st.settings.work_mode == SHU1_MODE_PREHEAT ? st.settings.preheat_target_temp_c : st.settings.target_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "tc", st.runtime.chamber_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "tp", st.runtime.ptc_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "h", st.runtime.heater_output_on) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "f", st.runtime.fan_output_on) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json, "err", shu1_heater_fault_str(st.runtime.heater_fault)) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "safe", st.runtime.safety_score) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "mr", st.printer.moonraker_connected) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "pr_ready", !shu1_device_config_restart_required() && st.printer.moonraker_connected &&
            st.printer.klippy_ready && st.printer.subscribed && st.printer.last_update_ms > 0 &&
            now_ms >= st.printer.last_update_ms && now_ms - st.printer.last_update_ms <= SHU1_PRINTER_STALE_MS) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json, "ps", st.printer.normalized_state) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "p", st.printer.print_progress) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "tool", st.printer.active_tool) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json, "mat", st.printer.active_material) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json, "prof_name", shu1_profile_name(st.settings.material_profile)) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "dry", st.settings.drying_running) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "rem", (long long)drying_remaining_s) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "latch", st.settings.output_safety_latch_armed) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "latch_ready", st.runtime.output_safety_latch_ready) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "setup_ok", st.runtime.setup_validation_passed) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "hv", st.settings.heater_output_verified) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "fv", st.settings.fan_output_verified) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "sv", st.settings.sensors_verified) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "mv", st.settings.moonraker_verified) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "zc", st.runtime.zero_cross_signal_present) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "zcr", (unsigned)st.runtime.zero_cross_edges_per_sec) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "zcp", (unsigned)st.runtime.zero_cross_last_period_us) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "zce", (unsigned long long)st.runtime.zero_cross_edges) != NULL;
    } else {
        if (json_ok) json_ok = cJSON_AddStringToObject(json, "fw", SHU1_FW_VERSION) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "ble_unlocked", g_ble_unlocked) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "heater_build", CONFIG_SHU1_ENABLE_HEATER_OUTPUT) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json, "probe_build", CONFIG_SHU1_ENABLE_GPIO_PROBE) != NULL;
        cJSON *json_child_0 = json_ok ? cJSON_AddObjectToObject(json, "pins") : NULL;
        if (!json_child_0) json_ok = false;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_0, "heater", CONFIG_SHU1_HEATER_GPIO) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_0, "fan", CONFIG_SHU1_FAN_GPIO) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_0, "zero_cross", CONFIG_SHU1_ZERO_CROSS_GPIO) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_0, "fan_triac", CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_0, "fan_mode", "held_gate_zc") != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_0, "button", CONFIG_SHU1_BUTTON_GPIO) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_0, "adc_chamber", CONFIG_SHU1_CHAMBER_ADC_CH) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_0, "adc_ptc", CONFIG_SHU1_PTC_ADC_CH) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_0, "rref_kohm", shu1_ntc_rref_kohm()) != NULL;
        cJSON *json_child_1 = json_ok ? cJSON_AddObjectToObject(json, "settings") : NULL;
        if (!json_child_1) json_ok = false;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "work_on", st.settings.work_on) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "mode", st.settings.work_mode) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "profile", st.settings.material_profile) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_1, "profile_name", shu1_profile_name(st.settings.material_profile)) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "set_temp", st.settings.target_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "filtertemp", st.settings.filter_trigger_bed_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "hotbedtemp", st.settings.heater_trigger_bed_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "ptc_cutoff", st.settings.ptc_cutoff_c) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "preheat_running", st.settings.preheat_running) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "preheat_target", st.settings.preheat_target_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "preheat_hold_min", st.settings.preheat_hold_min) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "preheat_phase", st.settings.preheat_phase) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "preheat_remaining_seconds", (long long)preheat_remaining_s) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "preheat_complete_pending", st.settings.preheat_complete_pending) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "manual_session_max_min", st.settings.manual_session_max_min) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "session_timeout_pending", st.settings.session_timeout_pending) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "fan_postrun_min", st.settings.fan_postrun_min) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "cool_release", st.settings.cool_release_c) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "tempering_enabled", st.settings.tempering_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "finish_conditioning_mode", st.settings.finish_conditioning_mode) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "tempering_end_temp", st.settings.tempering_end_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "tempering_duration_min", st.settings.tempering_duration_min) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "tempering_phase", st.settings.tempering_phase) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "tempering_start_temp", st.settings.tempering_start_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "tempering_current_target", st.settings.tempering_current_target_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "tempering_remaining_seconds", (long long)tempering_remaining_s) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "tempering_progress_pct", tempering_progress_pct) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "tempering_ramp_to_off", st.settings.tempering_end_temp_c == 0) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "tempering_complete_pending", st.settings.tempering_complete_pending) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "virtual_door_detection_enabled", st.settings.virtual_door_detection_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "virtual_door_window_sec", st.settings.virtual_door_window_sec) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "virtual_door_drop_c", st.settings.virtual_door_drop_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "virtual_door_rate_c_per_min", st.settings.virtual_door_rate_c_per_min) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "virtual_door_min_base_temp", st.settings.virtual_door_min_base_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "virtual_door_action", st.settings.virtual_door_action) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "virtual_door_open", st.settings.virtual_door_open) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "virtual_door_open_pending", st.settings.virtual_door_open_pending) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "virtual_door_last_drop_c", st.settings.virtual_door_last_drop_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "virtual_door_last_rate_c_per_min", st.settings.virtual_door_last_rate_c_per_min) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "material_mismatch_warning_enabled", st.settings.material_mismatch_warning_enabled) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_1, "material_mismatch_pending", st.settings.material_mismatch_pending) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "material_mismatch_user_profile", st.settings.material_mismatch_user_profile) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_1, "material_mismatch_printer_profile", st.settings.material_mismatch_printer_profile) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_1, "material_mismatch_message", st.runtime.material_mismatch_message) != NULL;
        cJSON *json_child_2 = json_ok ? cJSON_AddObjectToObject(json, "runtime") : NULL;
        if (!json_child_2) json_ok = false;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_2, "chamber", st.runtime.chamber_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_2, "ptc", st.runtime.ptc_temp_c) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_2, "raw_chamber", st.runtime.chamber_raw) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_2, "raw_ptc", st.runtime.ptc_raw) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_2, "chamber_sensor", shu1_sensor_status_str(st.runtime.chamber_sensor_status)) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_2, "ptc_sensor", shu1_sensor_status_str(st.runtime.ptc_sensor_status)) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_2, "heater", st.runtime.heater_output_on) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_2, "fan", st.runtime.fan_output_on) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_2, "fault", shu1_heater_fault_str(st.runtime.heater_fault)) != NULL;
        cJSON *json_child_3 = json_ok ? cJSON_AddObjectToObject(json, "printer") : NULL;
        if (!json_child_3) json_ok = false;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_3, "moonraker", st.printer.moonraker_connected) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_3, "klippy", st.printer.klippy_ready) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_3, "subscribed", st.printer.subscribed) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_3, "webhooks", st.printer.webhooks_state) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_3, "print_state", st.printer.print_state) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_3, "normalized", st.printer.normalized_state) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_3, "bed", st.printer.bed_temp) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_3, "bed_target", st.printer.bed_target) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_3, "progress", st.printer.print_progress) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_3, "active_tool", st.printer.active_tool) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_3, "active_tool_obj", st.printer.active_tool_object) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_3, "active_tool_temp", st.printer.active_tool_temp) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_3, "material", st.printer.active_material) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_3, "color", st.printer.active_color_rgba) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json_child_3, "u1_chamber", st.printer.u1_chamber_temp) != NULL;
        if (json_ok) json_ok = cJSON_AddBoolToObject(json_child_3, "u1_chamber_online", st.printer.chamber_sensor_online) != NULL;
        if (json_ok) json_ok = cJSON_AddStringToObject(json_child_3, "u1_chamber_obj", st.printer.u1_chamber_object) != NULL;
        if (json_ok) json_ok = cJSON_AddNumberToObject(json, "event_count", (unsigned)shu1_event_log_count()) != NULL;
    }
    shu1_control_snapshot_t ctl;
    char lease[SHU1_LEASE_ID_LEN + 1] = {0};
    shu1_control_snapshot(&ctl);
    if (g_ble_unlocked && g_ble_lease_proven && ctl.owner == SHU1_CONTROL_BLE)
        shu1_control_lease_get_id_for(SHU1_CONTROL_BLE, lease, sizeof(lease));
    cJSON *control = json_ok ? cJSON_AddObjectToObject(json, "control") : NULL;
    if (!control) json_ok = false;
    if (json_ok) json_ok = cJSON_AddStringToObject(control, "owner", shu1_control_source_str(ctl.owner)) != NULL;
    if (json_ok) json_ok = cJSON_AddNumberToObject(control, "state_revision", ctl.revision) != NULL;
    if (json_ok) json_ok = cJSON_AddBoolToObject(control, "lease_active", ctl.lease_active) != NULL;
    if (json_ok) json_ok = cJSON_AddNumberToObject(control, "lease_remaining_ms", ctl.lease_remaining_ms) != NULL;
    if (json_ok) json_ok = cJSON_AddStringToObject(control, "lease_id", lease) != NULL;
    if (json_ok) json_ok = cJSON_AddStringToObject(json, "chamber_sensor_status", shu1_sensor_status_str(st.runtime.chamber_sensor_status)) != NULL;
    if (json_ok) json_ok = cJSON_AddStringToObject(json, "ptc_sensor_status", shu1_sensor_status_str(st.runtime.ptc_sensor_status)) != NULL;
    // cJSON escapes strings and serializes non-finite numbers as null.
    if (!json_ok || !cJSON_PrintPreallocated(json, buf, (int)len, false))
        snprintf(buf, len, "{\"err\":\"status_unavailable\"}");
    cJSON_Delete(json);
#undef st
    free(stp);
}

static int append_string(struct os_mbuf *om, const char *s) {
    return os_mbuf_append(om, s, strlen(s)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static void notify_status(void) {
    if (g_conn_handle == BLE_HS_CONN_HANDLE_NONE || g_status_val_handle == 0) return;
    char *payload = calloc(1, CONFIG_SHU1_BLE_MAX_READ_BYTES);
    if (!payload) return;
    build_status_json(payload, CONFIG_SHU1_BLE_MAX_READ_BYTES, false);
    // ATT notifications cannot carry an arbitrarily long JSON value.
    // Clients receive a change hint and obtain the complete value through a GATT read.
    uint16_t mtu = ble_att_mtu(g_conn_handle);
    if (mtu <= 3 || strlen(payload) > (size_t)(mtu - 3))
        snprintf(payload, CONFIG_SHU1_BLE_MAX_READ_BYTES, "{\"read\":true}");
    struct os_mbuf *om = ble_hs_mbuf_from_flat(payload, strlen(payload));
    free(payload);
    if (!om) return;
    int rc = ble_gatts_notify_custom(g_conn_handle, g_status_val_handle, om);
    if (rc != 0) ESP_LOGD(TAG, "status notify skipped/failed rc=%d", rc);
}

static void apply_drying_start_stop(shu1_settings_t *st, bool requested_running) {
    if (requested_running && !st->drying_running) {
        int hours = st->custom_timer_h;
        if (st->drying_mode == SHU1_DRYING_PLA || st->drying_mode == SHU1_DRYING_PETG || st->drying_mode == SHU1_DRYING_ABS) hours = 12;
        st->drying_running = true;
        st->drying_end_ms = (esp_timer_get_time() / 1000) + ((int64_t)hours * 3600 * 1000);
        st->preheat_running = false;
        st->preheat_phase = SHU1_PREHEAT_IDLE;
        st->preheat_end_ms = 0;
        st->work_on = true;
        st->work_mode = SHU1_MODE_DRYING;
    } else if (!requested_running) {
        st->drying_running = false;
        st->drying_end_ms = 0;
    }
}

static int handle_control_write(struct ble_gatt_access_ctxt *ctxt) {
    int len = OS_MBUF_PKTLEN(ctxt->om);
    if (len <= 0 || len > CONFIG_SHU1_BLE_MAX_WRITE_BYTES) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    char *buf = calloc(1, len + 1);
    if (!buf) return BLE_ATT_ERR_INSUFFICIENT_RES;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, len, NULL);
    if (rc != 0) {
        free(buf);
        return BLE_ATT_ERR_UNLIKELY;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    SHU1_CONTROL_GUARD(policy_guard);
    cJSON *unlock = cJSON_GetObjectItem(root, "unlock");
    if (cJSON_IsString(unlock) && strcmp(unlock->valuestring, CONFIG_SHU1_BLE_CONTROL_PIN) == 0) {
        g_ble_unlocked = true;
        g_ble_lease_proven = false;
        ESP_LOGW(TAG, "BLE control unlocked for current session");
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        shu1_ble_notify_status_now();
        return 0;
    }

#if CONFIG_SHU1_BLE_REQUIRE_PIN_FOR_CONTROL
    if (!g_ble_unlocked && has_control_fields(root)) {
        ESP_LOGW(TAG, "BLE control rejected: locked");
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
    }
#endif

    // Initial REST provisioning is local BLE, never an unauthenticated HTTP fallback.
    cJSON *wifi_setup = cJSON_GetObjectItemCaseSensitive(root, "wifi_setup");
    bool wifi_stop = cJSON_IsTrue(cJSON_GetObjectItem(root, "safe_stop")) ||
        cJSON_IsTrue(cJSON_GetObjectItem(root, "emergency_stop")) ||
        cJSON_IsTrue(cJSON_GetObjectItem(root, "disarm_output_safety_latch")) ||
        cJSON_IsFalse(cJSON_GetObjectItem(root, "work_on"));
    if (wifi_setup && !wifi_stop) {
        bool dedicated = true;
        for (cJSON *item = root->child; item; item = item->next) {
            if (strcmp(item->string, "wifi_setup") && strcmp(item->string, "expected_revision") &&
                strcmp(item->string, "lease_id")) dedicated = false;
        }
        shu1_control_snapshot_t control;
        shu1_control_snapshot(&control);
        cJSON *revision = cJSON_GetObjectItemCaseSensitive(root, "expected_revision");
        bool fresh = cJSON_IsNumber(revision) && revision->valuedouble == (double)control.revision;
        esp_err_t err = g_ble_unlocked && dedicated && fresh
            ? shu1_wifi_setup_request(wifi_setup) : ESP_ERR_INVALID_STATE;
        if (err == ESP_OK) shu1_control_release_any(); // invalidate pre-setup commands
        cJSON_Delete(root);
        return err == ESP_OK ? 0 : BLE_ATT_ERR_UNLIKELY;
    }

    cJSON *rest_token = cJSON_GetObjectItemCaseSensitive(root, "rest_token");
    cJSON *receipt = cJSON_GetObjectItemCaseSensitive(root, "virtual_door_ack");
    if (receipt && root->child == receipt && !receipt->next && g_ble_unlocked &&
        cJSON_IsNumber(receipt) && receipt->valuedouble > 0 && receipt->valuedouble < 9007199254740992.0) {
        shu1_virtual_door_ack((int64_t)receipt->valuedouble);
        cJSON_Delete(root);
        return 0;
    }
    if (!wifi_stop && shu1_control_maintenance_active() &&
        (cJSON_GetObjectItem(root, "wifi_ssid") || cJSON_GetObjectItem(root, "wifi_password") ||
         cJSON_GetObjectItem(root, "moonraker_host") || cJSON_GetObjectItem(root, "moonraker_port"))) {
        cJSON_Delete(root);
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (rest_token) {
        bool dedicated = cJSON_IsObject(root) && root->child == rest_token && !rest_token->next;
        if (!g_ble_unlocked || !dedicated || !cJSON_IsString(rest_token) ||
            strlen(rest_token->valuestring) < 16 || strlen(rest_token->valuestring) > 64 ||
            !shu1_control_network_setup_begin()) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
        }
        esp_err_t err = shu1_settings_store_set_control_token(rest_token->valuestring);
        shu1_control_maintenance_end();
        cJSON_Delete(root);
        return err == ESP_OK ? 0 : BLE_ATT_ERR_UNLIKELY;
    }

    cJSON *heartbeat = cJSON_GetObjectItemCaseSensitive(root, "heartbeat");
    if (cJSON_IsString(heartbeat)) {
        bool valid = shu1_control_lease_heartbeat_for(SHU1_CONTROL_BLE,
                                                       heartbeat->valuestring);
        g_ble_lease_proven = valid;
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        if (!valid) return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
        shu1_ble_notify_status_now();
        return 0;
    }

    if (!has_control_fields(root)) {
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        return 0;
    }

    cJSON *factory_reset = cJSON_GetObjectItem(root, "factory_reset");
    if (factory_reset) {
        shu1_control_snapshot_t current;
        shu1_control_snapshot(&current);
        if (!shu1_reset_request_valid(root) || !has_valid_revision(root) ||
            requested_revision(root) != current.revision) {
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        esp_err_t reset_err = shu1_settings_store_factory_reset();
        return reset_err == ESP_OK ? 0 : BLE_ATT_ERR_UNLIKELY;
    }

    shu1_settings_t st = shu1_state_get_settings();
    const bool was_work_on = st.work_on;
    const bool stopping = cJSON_IsTrue(cJSON_GetObjectItem(root, "safe_stop")) ||
        cJSON_IsTrue(cJSON_GetObjectItem(root, "disarm_output_safety_latch")) ||
        cJSON_IsTrue(cJSON_GetObjectItem(root, "emergency_stop")) ||
        cJSON_IsFalse(cJSON_GetObjectItem(root, "work_on"));
    cJSON *chamber_offset = cJSON_GetObjectItem(root, "warehouse_temp_offset");
    cJSON *ptc_offset = cJSON_GetObjectItem(root, "ptc_temp_offset");
    const bool calibration = chamber_offset || ptc_offset;
    if ((!stopping && shu1_control_maintenance_active()) ||
        (!stopping && calibration && (st.work_on || st.scheduled_preheat_enabled ||
            shu1_control_outputs_busy() ||
            has_energy_control_field(root)))) {
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (calibration && !stopping) {
        // Dedicated one-channel transaction: no mixed start/reset/config side effects.
        bool invalid_calibration_request = chamber_offset && ptc_offset;
        for (cJSON *item = root->child; item; item = item->next) {
            if (!item->string ||
                (strcmp(item->string, "warehouse_temp_offset") != 0 &&
                 strcmp(item->string, "ptc_temp_offset") != 0 &&
                 strcmp(item->string, "expected_revision") != 0))
                invalid_calibration_request = true;
        }
        if (invalid_calibration_request) {
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        shu1_control_snapshot_t current;
        shu1_control_snapshot(&current);
        if (!has_valid_revision(root) || requested_revision(root) != current.revision ||
            current.owner != SHU1_CONTROL_NONE ||
            (chamber_offset && !cJSON_IsNumber(chamber_offset)) ||
            (ptc_offset && !cJSON_IsNumber(ptc_offset))) {
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    if (!stopping && cJSON_IsTrue(cJSON_GetObjectItem(root, "clear_heater_fault"))) {
        shu1_control_snapshot_t ctl;
        shu1_control_snapshot(&ctl);
        if (!has_valid_revision(root) || requested_revision(root) != ctl.revision) {
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return BLE_ATT_ERR_UNLIKELY;
        }
        shu1_control_release_any();
        g_ble_lease_proven = false;
        apply_safe_stop(&st);
        shu1_state_update_settings_command(&st);
        shu1_safety_latch_request_clear();
        shu1_safety_wake();
        shu1_event_log_add("warn", "ble_fault_clear_requested",
                           "persistent heater fault clear requested; control task will validate safe conditions");
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        shu1_ble_notify_status_now();
        return 0;
    }

    bool emergency_stop = cJSON_IsTrue(cJSON_GetObjectItem(root, "emergency_stop"));
    if (emergency_stop) shu1_safety_latch_trip_volatile(SHU1_HEATER_PANIC_OFF);
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "safe_stop")) ||
        cJSON_IsTrue(cJSON_GetObjectItem(root, "disarm_output_safety_latch")) ||
        cJSON_IsFalse(cJSON_GetObjectItem(root, "work_on")) || emergency_stop) {
        shu1_control_release_any();
        g_ble_lease_proven = false;
        apply_safe_stop(&st);
        shu1_state_update_settings_command(&st);
        shu1_safety_wake();
        shu1_control_guard_end(&policy_guard);
        // OFF/disarm is volatile and is never restored armed at boot.
        shu1_event_log_add(emergency_stop ? "critical" : "warn",
                           emergency_stop ? "ble_emergency_latched" : "ble_safe_stop",
                           emergency_stop ? "BLE emergency stop latched panic-off for this boot" :
                                            "BLE safe stop forced heater workflows off and disarmed output latch");
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        shu1_ble_notify_status_now();
        return 0;
    }

    cJSON *profile_name = cJSON_GetObjectItem(root, "profile");
    if (cJSON_IsString(profile_name)) {
        shu1_apply_material_profile(&st, shu1_profile_from_name(profile_name->valuestring), true);
        shu1_event_log_add("info", "ble_profile_applied", profile_name->valuestring);
    }
    cJSON *profile_id = cJSON_GetObjectItem(root, "material_profile");
    if (cJSON_IsNumber(profile_id)) {
        shu1_apply_material_profile(&st, profile_id->valueint, true);
        shu1_event_log_add("info", "ble_profile_applied", shu1_profile_name(st.material_profile));
    }

    if (shu1_explicit_job_start(root)) shu1_settings_stop(&st);
    st.work_on = json_bool(root, "work_on", st.work_on);
    st.work_mode = json_int_clamp(root, "work_mode", st.work_mode, SHU1_MODE_AUTO, SHU1_MODE_HEALTH_TEST);
    st.target_temp_c = json_int_clamp(root, "set_temp", st.target_temp_c, 0, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.filter_trigger_bed_c = json_int_clamp(root, "filtertemp", st.filter_trigger_bed_c, 0, 120);
    st.heater_trigger_bed_c = json_int_clamp(root, "hotbedtemp", st.heater_trigger_bed_c, 30, 120);
    cJSON *ptc_cutoff = cJSON_GetObjectItem(root, "ptc_cutoff");
    if (cJSON_IsNumber(ptc_cutoff)) {
        st.ptc_cutoff_c = ptc_cutoff->valueint <= 0 ? 0 : clamp_i(ptc_cutoff->valueint, 90, 104);
    }
    st.drying_mode = json_int_clamp(root, "filament_drying_mode", st.drying_mode, SHU1_DRYING_PLA, SHU1_DRYING_CUSTOM);
    st.custom_temp_c = json_int_clamp(root, "custom_temp", st.custom_temp_c, 40, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.custom_timer_h = json_int_clamp(root, "custom_timer", st.custom_timer_h, 1, 12);
    st.preheat_target_temp_c = json_int_clamp(root, "preheat_target", st.preheat_target_temp_c, 0, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.preheat_hold_min = json_int_clamp(root, "preheat_hold_min", st.preheat_hold_min, 1, 240);
    st.manual_session_max_min = json_int_clamp(root, "manual_session_max_min", st.manual_session_max_min, 1, 720);
    st.fan_postrun_min = json_int_clamp(root, "fan_postrun_min", st.fan_postrun_min, 0, 60);
    st.cool_release_c = json_int_clamp(root, "cool_release", st.cool_release_c, 30, 65);
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
    st.pause_hold_min = json_int_clamp(root, "pause_hold_min", st.pause_hold_min, 1, 720);
    st.pause_lower_after_min = json_int_clamp(root, "pause_lower_after_min", st.pause_lower_after_min, 1, 720);
    st.pause_lower_by_c = json_int_clamp(root, "pause_lower_by_c", st.pause_lower_by_c, 0, 30);
    st.pause_stop_after_min = json_int_clamp(root, "pause_stop_after_min", st.pause_stop_after_min, 1, 720);
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
    st.virtual_door_action = SHU1_VDOOR_ACTION_NOTIFY_ONLY;
    cJSON *ack_vdoor = cJSON_GetObjectItem(root, "ack_virtual_door_open");
    if (cJSON_IsBool(ack_vdoor) && cJSON_IsTrue(ack_vdoor)) { st.virtual_door_open_pending = false; st.door_open_pending = false; }
    cJSON *clear_vdoor = cJSON_GetObjectItem(root, "clear_virtual_door_open");
    if (cJSON_IsBool(clear_vdoor) && cJSON_IsTrue(clear_vdoor)) { st.virtual_door_open = false; st.virtual_door_open_pending = false; st.door_open = false; st.door_open_pending = false; }
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
    st.pickup_keep_warm_min = json_int_clamp(root, "pickup_keep_warm_min", st.pickup_keep_warm_min, 1, 720);
    st.print_risk_enabled = json_bool(root, "print_risk_enabled", st.print_risk_enabled);
    st.start_print_warning_enabled = json_bool(root, "start_print_warning_enabled", st.start_print_warning_enabled);
    st.local_recipes_enabled = json_bool(root, "local_recipes_enabled", st.local_recipes_enabled);
    st.active_recipe_slot = json_int_clamp(root, "active_recipe_slot", st.active_recipe_slot, 0, 8);
    st.safety_score_enabled = json_bool(root, "safety_score_enabled", st.safety_score_enabled);
    cJSON *recipe_name = cJSON_GetObjectItem(root, "active_recipe_name");
    if (cJSON_IsString(recipe_name)) snprintf(st.active_recipe_name, sizeof(st.active_recipe_name), "%s", recipe_name->valuestring);
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_filter_life_warning"))) st.filter_life_warning_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_heater_wear_warning"))) st.heater_wear_warning_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_airflow_warning"))) st.airflow_warning_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_pla_protection"))) st.pla_protection_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_print_risk_warning"))) st.print_risk_warning_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_start_print_warning"))) st.start_print_warning_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_heat_soak_complete"))) st.heat_soak_complete_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_setup_warning"))) st.setup_warning_pending = false;

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
    st.contest_showcase_mode_enabled = json_bool(root, "contest_showcase_mode_enabled", st.contest_showcase_mode_enabled);
    st.symbiont_mode_enabled = json_bool(root, "symbiont_mode_enabled", st.symbiont_mode_enabled);
    st.symbiont_ventilation_allowed = json_bool(root, "symbiont_ventilation_allowed", st.symbiont_ventilation_allowed);
    st.symbiont_safe_control_enabled = json_bool(root, "symbiont_safe_control_enabled", st.symbiont_safe_control_enabled);
    st.symbiont_policy = json_int_clamp(root, "symbiont_policy", st.symbiont_policy, SHU1_SYMBIONT_POLICY_READ_ONLY, SHU1_SYMBIONT_POLICY_CLIMATE_SAFE);
    // Legacy arm field is inert. Normal work requests admit a session through
    // the measured safety gate; legacy disarm is handled as unconditional OFF.
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_incident_report"))) st.incident_report_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "ack_symbiont_notification"))) st.symbiont_notification_pending = false;
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "generate_incident_report"))) {
        st.incident_report_pending = true;
        st.incident_report_seq++;
        snprintf(st.incident_last_reason, sizeof(st.incident_last_reason), "%s", "manual_request");
        shu1_event_log_add("warn", "incident_report_requested", "manual incident report snapshot requested by BLE");
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
            st.drying_running = false;
            st.drying_end_ms = 0;
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
            st.preheat_running = false; st.drying_running = false; st.health_test_running = false;
        } else if (!run) {
            st.dryout_running = false; st.dryout_end_ms = 0; st.work_on = false;
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
            st.health_test_running = true; st.health_test_phase = SHU1_HEALTH_IDLE;
            st.health_test_result = SHU1_HEALTH_RESULT_NONE; st.health_test_complete_pending = false;
            st.work_on = true; st.work_mode = SHU1_MODE_HEALTH_TEST;
            st.preheat_running = false; st.drying_running = false; st.dryout_running = false;
        } else if (!run) {
            st.health_test_running = false; st.health_test_phase = SHU1_HEALTH_IDLE;
            st.health_test_result = SHU1_HEALTH_RESULT_ABORTED; st.work_on = false;
        }
    }
    cJSON *ack_health = cJSON_GetObjectItem(root, "ack_health_test_complete");
    if (cJSON_IsBool(ack_health) && cJSON_IsTrue(ack_health)) st.health_test_complete_pending = false;
    cJSON *ack_dryout = cJSON_GetObjectItem(root, "ack_dryout_complete");
    if (cJSON_IsBool(ack_dryout) && cJSON_IsTrue(ack_dryout)) st.dryout_complete_pending = false;

    cJSON *dry = cJSON_GetObjectItem(root, "isrunning");
    if (cJSON_IsBool(dry)) apply_drying_start_stop(&st, cJSON_IsTrue(dry));

    shu1_finish_job_command(&st, root, esp_timer_get_time() / 1000);
    if ((st.work_on || st.scheduled_preheat_enabled) && !shu1_control_start_allowed()) {
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        return BLE_ATT_ERR_UNLIKELY;
    }
    const bool energy_command = has_energy_control_field(root);
    const bool session_mutation = has_session_mutation(root);
    shu1_control_snapshot_t ctl;
    shu1_control_snapshot(&ctl);
    if (energy_command && !st.work_on && !st.scheduled_preheat_enabled) {
        shu1_control_release_any();
        g_ble_lease_proven = false;
    } else if (session_mutation && (st.work_on || st.scheduled_preheat_enabled || ctl.owner != SHU1_CONTROL_NONE)) {
        if (!has_valid_revision(root)) {
            g_ble_lease_proven = false;
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return BLE_ATT_ERR_UNLIKELY;
        }
        bool takeover = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "takeover"));
        char issued_lease[SHU1_LEASE_ID_LEN + 1];
        shu1_control_result_t result;
        if (!was_work_on || ctl.owner == SHU1_CONTROL_NONE) {
            result = shu1_control_claim(SHU1_CONTROL_BLE, takeover || !was_work_on,
                                        requested_revision(root), issued_lease);
        } else if (ctl.owner == SHU1_CONTROL_BLE) {
            result = shu1_control_authorize(SHU1_CONTROL_BLE, requested_lease(root),
                                            requested_revision(root));
        } else if (takeover) {
            result = shu1_control_claim(SHU1_CONTROL_BLE, true,
                                        requested_revision(root), issued_lease);
        } else {
            result = SHU1_CONTROL_BUSY;
        }
        if (result != SHU1_CONTROL_OK) {
            g_ble_lease_proven = false;
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return result == SHU1_CONTROL_STALE ? BLE_ATT_ERR_UNLIKELY
                                                : BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
        }
        g_ble_lease_proven = true;
    }

    // Calibration is idle-only and all ownership/revision checks have passed.
    esp_err_t calibration_error = ESP_OK;
    if (cJSON_IsNumber(chamber_offset))
        calibration_error = shu1_ntc_set_offset_c(0, (float)chamber_offset->valuedouble);
    if (calibration_error == ESP_OK && cJSON_IsNumber(ptc_offset))
        calibration_error = shu1_ntc_set_offset_c(1, (float)ptc_offset->valuedouble);
    if (calibration_error != ESP_OK) {
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (calibration) shu1_control_release_any(); // Advance revision for the accepted calibration.
    shu1_settings_limit_targets(&st);
    esp_err_t persist_err = calibration ? ESP_OK : shu1_settings_store_save_settings(&st);

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
        if (persist_err == ESP_OK) persist_err = shu1_settings_store_save_device_config(&cfg);
        if (persist_err == ESP_OK) shu1_device_config_require_restart();
    }


    if (persist_err != ESP_OK) {
        // Writes may be partial: do not admit heat or pretend the command succeeded.
        shu1_safety_latch_inhibit();
        shu1_settings_stop(&st);
        shu1_control_release_any();
        shu1_state_update_settings_command(&st);
        shu1_safety_wake();
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        return BLE_ATT_ERR_UNLIKELY;
    }
    shu1_state_update_settings_command(&st);
    shu1_safety_wake();
    shu1_control_guard_end(&policy_guard);

    cJSON_Delete(root);
    shu1_ble_notify_status_now();
    return 0;
}

static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle;
    (void)attr_handle;
    uintptr_t chr = (uintptr_t)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        char *payload = calloc(1, CONFIG_SHU1_BLE_MAX_READ_BYTES);
        if (!payload) return BLE_ATT_ERR_INSUFFICIENT_RES;
        if (chr == 1) {
            build_status_json(payload, CONFIG_SHU1_BLE_MAX_READ_BYTES, false);
        } else if (chr == 3) {
            build_status_json(payload, CONFIG_SHU1_BLE_MAX_READ_BYTES, true);
        } else {
            free(payload);
            return BLE_ATT_ERR_UNLIKELY;
        }
        ESP_LOGD(TAG, "BLE read chr=%u len=%u", (unsigned)chr, (unsigned)strlen(payload));
        int rc = append_string(ctxt->om, payload);
        free(payload);
        return rc;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR && chr == 2) {
        ESP_LOGD(TAG, "BLE write control len=%u", (unsigned)OS_MBUF_PKTLEN(ctxt->om));
        return handle_control_write(ctxt);
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_chr_def g_ble_chars[] = {
    {
        .uuid = &g_status_uuid.u,
        .access_cb = gatt_access_cb,
        .arg = (void *)1,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &g_status_val_handle,
    },
    {
        .uuid = &g_control_uuid.u,
        .access_cb = gatt_access_cb,
        .arg = (void *)2,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {
        .uuid = &g_diag_uuid.u,
        .access_cb = gatt_access_cb,
        .arg = (void *)3,
        .flags = BLE_GATT_CHR_F_READ,
    },
    {0}
};

static const struct ble_gatt_svc_def g_ble_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &g_svc_uuid.u,
        .characteristics = g_ble_chars,
    },
    {0}
};

static void ble_on_reset(int reason) {
    ESP_LOGE(TAG, "BLE reset reason=%d", reason);
}

static void ble_on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &g_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed rc=%d", rc);
        return;
    }
    ble_advertise();
}

static void ble_advertise(void) {
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)CONFIG_SHU1_BLE_DEVICE_NAME;
    fields.name_len = strlen(CONFIG_SHU1_BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv set fields failed rc=%d", rc);
        return;
    }

    struct ble_hs_adv_fields rsp_fields;
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    rsp_fields.uuids128 = &g_svc_uuid;
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "scan response set fields failed rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(g_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) ESP_LOGE(TAG, "adv start failed rc=%d", rc);
    else ESP_LOGI(TAG, "BLE advertising as %s", CONFIG_SHU1_BLE_DEVICE_NAME);
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            g_conn_handle = event->connect.conn_handle;
            g_ble_unlocked = !CONFIG_SHU1_BLE_REQUIRE_PIN_FOR_CONTROL;
            g_ble_lease_proven = false;
            ESP_LOGI(TAG, "BLE connected handle=%d", g_conn_handle);
            shu1_ble_notify_status_now();
        } else {
            ESP_LOGW(TAG, "BLE connect failed status=%d", event->connect.status);
            ble_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected reason=%d", event->disconnect.reason);
        g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        g_ble_unlocked = false;
        g_ble_lease_proven = false;
        ble_advertise();
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "BLE subscribe attr=%d notify=%d indicate=%d", event->subscribe.attr_handle,
                 event->subscribe.cur_notify, event->subscribe.cur_indicate);
        if (event->subscribe.cur_notify) shu1_ble_notify_status_now();
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "BLE MTU updated to %d", event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

static void ble_host_task(void *param) {
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_notify_task(void *param) {
    (void)param;
    while (1) {
        if (g_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            if (g_notify_requested) {
                g_notify_requested = false;
                notify_status();
            }
            vTaskDelay(pdMS_TO_TICKS(CONFIG_SHU1_BLE_NOTIFY_PERIOD_MS));
            notify_status();
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

esp_err_t shu1_ble_start(void) {
    ESP_LOGI(TAG, "starting BLE control service");

    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "nimble_port_init failed rc=%d", rc);
        return ESP_FAIL;
    }

    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.sm_our_key_dist = 0;
    ble_hs_cfg.sm_their_key_dist = 0;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(CONFIG_SHU1_BLE_DEVICE_NAME);

    rc = ble_gatts_count_cfg(g_ble_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed rc=%d", rc);
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(g_ble_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed rc=%d", rc);
        return ESP_FAIL;
    }

    nimble_port_freertos_init(ble_host_task);
    xTaskCreate(ble_notify_task, "shu1_ble_notify", 8192, NULL, 4, NULL);
    return ESP_OK;
}

void shu1_ble_notify_status_now(void) {
    g_notify_requested = true;
}

#else

static const char *TAG = "shu1_ble";

esp_err_t shu1_ble_start(void) {
    ESP_LOGW(TAG, "BLE disabled in build");
    return ESP_OK;
}

void shu1_ble_notify_status_now(void) {}

#endif
