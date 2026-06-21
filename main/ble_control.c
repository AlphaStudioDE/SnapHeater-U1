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
#include "event_log.h"
#include "esp_log.h"
#include "esp_timer.h"
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

// 128-bit UUIDs, vendor/private range. Byte order follows NimBLE BLE_UUID128_INIT convention.
static const ble_uuid128_t g_svc_uuid     = BLE_UUID128_INIT(0x11,0x10,0x00,0x00,0xA1,0x5B,0x4A,0x7C,0x91,0x42,0x53,0x48,0x55,0x31,0x00,0x01);
static const ble_uuid128_t g_status_uuid  = BLE_UUID128_INIT(0x11,0x10,0x00,0x01,0xA1,0x5B,0x4A,0x7C,0x91,0x42,0x53,0x48,0x55,0x31,0x00,0x01);
static const ble_uuid128_t g_control_uuid = BLE_UUID128_INIT(0x11,0x10,0x00,0x02,0xA1,0x5B,0x4A,0x7C,0x91,0x42,0x53,0x48,0x55,0x31,0x00,0x01);
static const ble_uuid128_t g_diag_uuid    = BLE_UUID128_INIT(0x11,0x10,0x00,0x03,0xA1,0x5B,0x4A,0x7C,0x91,0x42,0x53,0x48,0x55,0x31,0x00,0x01);

static uint8_t g_own_addr_type;
static uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t g_status_val_handle;
static bool g_ble_unlocked = false;
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
        "work_on", "work_mode", "set_temp", "filtertemp", "hotbedtemp", "ptc_cutoff",
        "filament_drying_mode", "isrunning", "custom_temp", "custom_timer",
        "preheat_running", "preheat_target", "preheat_hold_min", "ack_preheat_complete",
        "material_profile", "profile", "manual_session_max_min", "ack_session_timeout",
        "fan_postrun_min", "tempering_enabled", "tempering_end_temp", "tempering_duration_min", "ack_tempering_complete", "cancel_tempering",
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
        "local_recipes_enabled", "active_recipe_slot", "active_recipe_name", "demo_mode_enabled", "safety_score_enabled", "ack_setup_warning",
        "first_setup_wizard_enabled", "first_setup_step", "first_setup_complete", "temp_history_enabled", "history_sample_period_sec",
        "incident_report_enabled", "generate_incident_report", "ack_incident_report", "output_safety_latch_enabled", "heater_output_verified",
        "fan_output_verified", "sensors_verified", "moonraker_verified", "arm_output_safety_latch", "disarm_output_safety_latch",
        "notification_min_level", "language_code", "local_only_mode", "ota_enabled", "contest_showcase_mode_enabled",
        "symbiont_mode_enabled", "symbiont_ventilation_allowed", "symbiont_safe_control_enabled", "symbiont_policy", "ack_symbiont_notification",
        "wifi_ssid", "wifi_password", "moonraker_host", "moonraker_port", "factory_reset"
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (cJSON_GetObjectItem(root, names[i])) return true;
    }
    return false;
}

static void build_status_json(char *buf, size_t len, bool diagnostics) {
    shu1_state_t st;
    shu1_state_get(&st);
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

    if (!diagnostics) {
        // Compact status for BLE notifications.
        // Android should show local notifications when any *_pending flag is true.
        snprintf(buf, len,
                 "{\"v\":\"%s\",\"on\":%s,\"m\":%d,\"set\":%d,\"tc\":%.1f,\"tp\":%.1f,"
                 "\"h\":%s,\"f\":%s,\"err\":\"%s\",\"dry\":%s,\"rem\":%lld,"
                 "\"prof\":%d,\"prof_name\":\"%s\",\"ph\":%d,\"pr\":%lld,\"pd\":%s,\"sess_to\":%s,"
                 "\"tmph\":%d,\"tmps\":%d,\"tmpt\":%d,\"tmpe\":%d,\"tmpr\":%lld,\"tmpp\":%d,\"tmpd\":%s,"
                 "\"aw\":%s,\"lp\":%s,\"night\":%s,\"dryout\":%s,\"dyd\":%s,\"sched\":%s,\"hst\":%d,\"hres\":%d,\"stab\":%d,\"wh\":%.2f,"
                 "\"eta\":%d,\"soak\":%d,\"soakr\":%d,\"flt\":%d,\"wear\":%d,\"air\":%s,\"pla\":%s,\"risk\":%d,\"riskp\":%s,\"startp\":%s,\"safe\":%d,\"demo\":%s,"
                 "\"vdoor\":%s,\"vdoor_pending\":%s,\"vdoor_drop\":%.1f,\"vdoor_rate\":%.1f,"
                 "\"mm\":%s,\"mmu\":%d,\"mmp\":%d,\"mmmsg\":\"%s\","
                 "\"mr\":%s,\"ps\":\"%s\",\"p\":%.3f,\"bt\":%.1f,\"tool\":%d,\"mat\":\"%s\"}",
                 SHU1_FW_VERSION,
                 st.settings.work_on ? "true" : "false",
                 st.settings.work_mode,
                 st.settings.work_mode == SHU1_MODE_PREHEAT ? st.settings.preheat_target_temp_c : st.settings.target_temp_c,
                 st.runtime.chamber_temp_c,
                 st.runtime.ptc_temp_c,
                 st.runtime.heater_output_on ? "true" : "false",
                 st.runtime.fan_output_on ? "true" : "false",
                 shu1_heater_fault_str(st.runtime.heater_fault),
                 st.settings.drying_running ? "true" : "false",
                 (long long)drying_remaining_s,
                 st.settings.material_profile,
                 shu1_profile_name(st.settings.material_profile),
                 st.settings.preheat_phase,
                 (long long)preheat_remaining_s,
                 st.settings.preheat_complete_pending ? "true" : "false",
                 st.settings.session_timeout_pending ? "true" : "false",
                 st.settings.tempering_phase, st.settings.tempering_start_temp_c, st.settings.tempering_current_target_c,
                 st.settings.tempering_end_temp_c, (long long)tempering_remaining_s, tempering_progress_pct,
                 st.settings.tempering_complete_pending ? "true" : "false",
                 st.settings.anti_warp_enabled ? "true" : "false", st.settings.large_print_protection_enabled ? "true" : "false",
                 st.settings.safe_overnight_enabled ? "true" : "false", st.settings.dryout_running ? "true" : "false",
                 st.settings.dryout_complete_pending ? "true" : "false", st.settings.scheduled_preheat_enabled ? "true" : "false",
                 st.settings.health_test_phase, st.settings.health_test_result, st.runtime.stability_score_pct, st.runtime.estimated_energy_wh,
                 st.runtime.warmup_eta_sec, st.settings.heat_soak_phase, st.runtime.heat_soak_remaining_sec, st.runtime.filter_life_pct,
                 st.runtime.heater_wear_pct, st.settings.airflow_warning_pending ? "true" : "false", st.settings.pla_protection_pending ? "true" : "false",
                 st.runtime.print_risk_score, st.settings.print_risk_warning_pending ? "true" : "false", st.settings.start_print_warning_pending ? "true" : "false",
                 st.runtime.safety_score, st.settings.demo_mode_enabled ? "true" : "false",
                 st.settings.virtual_door_open ? "true" : "false", st.settings.virtual_door_open_pending ? "true" : "false",
                 st.settings.virtual_door_last_drop_c, st.settings.virtual_door_last_rate_c_per_min,
                 st.settings.material_mismatch_pending ? "true" : "false", st.settings.material_mismatch_user_profile,
                 st.settings.material_mismatch_printer_profile, st.runtime.material_mismatch_message,
                 st.printer.moonraker_connected ? "true" : "false",
                 st.printer.normalized_state, st.printer.print_progress, st.printer.bed_temp,
                 st.printer.active_tool, st.printer.active_material);
        return;
    }

    snprintf(buf, len,
             "{\"fw\":\"%s\",\"ble_unlocked\":%s,\"heater_build\":%s,\"probe_build\":%s,"
             "\"pins\":{\"heater\":%d,\"fan\":%d,\"button\":%d,\"adc_chamber\":%d,\"adc_ptc\":%d},"
             "\"settings\":{\"work_on\":%s,\"mode\":%d,\"profile\":%d,\"profile_name\":\"%s\",\"set_temp\":%d,\"filtertemp\":%d,\"hotbedtemp\":%d,\"ptc_cutoff\":%d,"
             "\"preheat_running\":%s,\"preheat_target\":%d,\"preheat_hold_min\":%d,\"preheat_phase\":%d,\"preheat_remaining_seconds\":%lld,\"preheat_complete_pending\":%s,"
             "\"manual_session_max_min\":%d,\"session_timeout_pending\":%s,"
             "\"fan_postrun_min\":%d,\"tempering_enabled\":%s,\"tempering_end_temp\":%d,\"tempering_duration_min\":%d,"
             "\"tempering_phase\":%d,\"tempering_start_temp\":%d,\"tempering_current_target\":%d,"
             "\"tempering_remaining_seconds\":%lld,\"tempering_progress_pct\":%d,\"tempering_ramp_to_off\":%s,\"tempering_complete_pending\":%s,"
             "\"virtual_door_detection_enabled\":%s,\"virtual_door_window_sec\":%d,\"virtual_door_drop_c\":%d,"
             "\"virtual_door_rate_c_per_min\":%d,\"virtual_door_min_base_temp\":%d,\"virtual_door_action\":%d,"
             "\"virtual_door_open\":%s,\"virtual_door_open_pending\":%s,\"virtual_door_last_drop_c\":%.1f,\"virtual_door_last_rate_c_per_min\":%.1f,"
             "\"material_mismatch_warning_enabled\":%s,\"material_mismatch_pending\":%s,\"material_mismatch_user_profile\":%d,\"material_mismatch_printer_profile\":%d,\"material_mismatch_message\":\"%s\"},"
             "\"runtime\":{\"chamber\":%.2f,\"ptc\":%.2f,\"raw_chamber\":%d,\"raw_ptc\":%d,\"chamber_sensor\":\"%s\",\"ptc_sensor\":\"%s\",\"heater\":%s,\"fan\":%s,\"fault\":\"%s\"},"
             "\"printer\":{\"moonraker\":%s,\"klippy\":%s,\"subscribed\":%s,\"webhooks\":\"%s\",\"print_state\":\"%s\",\"normalized\":\"%s\","
             "\"bed\":%.1f,\"bed_target\":%.1f,\"progress\":%.3f,\"active_tool\":%d,\"active_tool_obj\":\"%s\",\"active_tool_temp\":%.1f,"
             "\"material\":\"%s\",\"color\":\"%s\",\"u1_chamber\":%.1f,\"u1_chamber_online\":%s,\"u1_chamber_obj\":\"%s\"},\"event_count\":%u}",
             SHU1_FW_VERSION,
             g_ble_unlocked ? "true" : "false",
             CONFIG_SHU1_ENABLE_HEATER_OUTPUT ? "true" : "false",
             CONFIG_SHU1_ENABLE_GPIO_PROBE ? "true" : "false",
             CONFIG_SHU1_HEATER_GPIO, CONFIG_SHU1_FAN_GPIO, CONFIG_SHU1_BUTTON_GPIO,
             CONFIG_SHU1_CHAMBER_ADC_CH, CONFIG_SHU1_PTC_ADC_CH,
             st.settings.work_on ? "true" : "false", st.settings.work_mode, st.settings.material_profile, shu1_profile_name(st.settings.material_profile), st.settings.target_temp_c,
             st.settings.filter_trigger_bed_c, st.settings.heater_trigger_bed_c, st.settings.ptc_cutoff_c,
             st.settings.preheat_running ? "true" : "false", st.settings.preheat_target_temp_c, st.settings.preheat_hold_min,
             st.settings.preheat_phase, (long long)preheat_remaining_s, st.settings.preheat_complete_pending ? "true" : "false",
             st.settings.manual_session_max_min, st.settings.session_timeout_pending ? "true" : "false",
             st.settings.fan_postrun_min, st.settings.tempering_enabled ? "true" : "false",
             st.settings.tempering_end_temp_c, st.settings.tempering_duration_min, st.settings.tempering_phase,
             st.settings.tempering_start_temp_c, st.settings.tempering_current_target_c,
             (long long)tempering_remaining_s, tempering_progress_pct, st.settings.tempering_end_temp_c == 0 ? "true" : "false",
             st.settings.tempering_complete_pending ? "true" : "false",
             st.settings.virtual_door_detection_enabled ? "true" : "false", st.settings.virtual_door_window_sec,
             st.settings.virtual_door_drop_c, st.settings.virtual_door_rate_c_per_min, st.settings.virtual_door_min_base_temp_c,
             st.settings.virtual_door_action, st.settings.virtual_door_open ? "true" : "false", st.settings.virtual_door_open_pending ? "true" : "false",
             st.settings.virtual_door_last_drop_c, st.settings.virtual_door_last_rate_c_per_min,
             st.settings.material_mismatch_warning_enabled ? "true" : "false", st.settings.material_mismatch_pending ? "true" : "false",
             st.settings.material_mismatch_user_profile, st.settings.material_mismatch_printer_profile, st.runtime.material_mismatch_message,
             st.runtime.chamber_temp_c, st.runtime.ptc_temp_c, st.runtime.chamber_raw, st.runtime.ptc_raw,
             shu1_sensor_status_str(st.runtime.chamber_sensor_status), shu1_sensor_status_str(st.runtime.ptc_sensor_status),
             st.runtime.heater_output_on ? "true" : "false", st.runtime.fan_output_on ? "true" : "false",
             shu1_heater_fault_str(st.runtime.heater_fault),
             st.printer.moonraker_connected ? "true" : "false",
             st.printer.klippy_ready ? "true" : "false", st.printer.subscribed ? "true" : "false",
             st.printer.webhooks_state, st.printer.print_state, st.printer.normalized_state,
             st.printer.bed_temp, st.printer.bed_target, st.printer.print_progress,
             st.printer.active_tool, st.printer.active_tool_object, st.printer.active_tool_temp,
             st.printer.active_material, st.printer.active_color_rgba,
             st.printer.u1_chamber_temp, st.printer.chamber_sensor_online ? "true" : "false", st.printer.u1_chamber_object,
             (unsigned)shu1_event_log_count());
}

static int append_string(struct os_mbuf *om, const char *s) {
    return os_mbuf_append(om, s, strlen(s)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static void notify_status(void) {
    if (g_conn_handle == BLE_HS_CONN_HANDLE_NONE || g_status_val_handle == 0) return;
    char payload[2048];
    build_status_json(payload, sizeof(payload), false);
    struct os_mbuf *om = ble_hs_mbuf_from_flat(payload, strlen(payload));
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

    cJSON *unlock = cJSON_GetObjectItem(root, "unlock");
    if (cJSON_IsString(unlock) && strcmp(unlock->valuestring, CONFIG_SHU1_BLE_CONTROL_PIN) == 0) {
        g_ble_unlocked = true;
        ESP_LOGW(TAG, "BLE control unlocked for current session");
        cJSON_Delete(root);
        shu1_ble_notify_status_now();
        return 0;
    }

#if CONFIG_SHU1_BLE_REQUIRE_PIN_FOR_CONTROL
    if (!g_ble_unlocked && has_control_fields(root)) {
        ESP_LOGW(TAG, "BLE control rejected: locked");
        cJSON_Delete(root);
        return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
    }
#endif

    if (!has_control_fields(root)) {
        cJSON_Delete(root);
        return 0;
    }

    shu1_settings_t st = shu1_state_get_settings();

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
    st.pickup_keep_warm_min = json_int_clamp(root, "pickup_keep_warm_min", st.pickup_keep_warm_min, 0, 720);
    st.print_risk_enabled = json_bool(root, "print_risk_enabled", st.print_risk_enabled);
    st.start_print_warning_enabled = json_bool(root, "start_print_warning_enabled", st.start_print_warning_enabled);
    st.local_recipes_enabled = json_bool(root, "local_recipes_enabled", st.local_recipes_enabled);
    st.active_recipe_slot = json_int_clamp(root, "active_recipe_slot", st.active_recipe_slot, 0, 8);
    st.demo_mode_enabled = json_bool(root, "demo_mode_enabled", st.demo_mode_enabled);
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
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "arm_output_safety_latch"))) {
        st.output_safety_latch_armed = st.heater_output_verified && st.fan_output_verified && st.sensors_verified && (!st.moonraker_verified || st.setup_validation_passed);
        if (st.output_safety_latch_armed) shu1_event_log_add("info", "output_latch_armed", "runtime output safety latch armed by BLE after verification flags");
    }
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "disarm_output_safety_latch"))) st.output_safety_latch_armed = false;
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
    shu1_ble_notify_status_now();
    return 0;
}

static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle;
    (void)attr_handle;
    uintptr_t chr = (uintptr_t)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        char payload[CONFIG_SHU1_BLE_MAX_READ_BYTES];
        if (chr == 1) {
            build_status_json(payload, sizeof(payload), false);
        } else if (chr == 3) {
            build_status_json(payload, sizeof(payload), true);
        } else {
            return BLE_ATT_ERR_UNLIKELY;
        }
        return append_string(ctxt->om, payload);
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR && chr == 2) {
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
    xTaskCreate(ble_notify_task, "shu1_ble_notify", 4096, NULL, 4, NULL);
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
