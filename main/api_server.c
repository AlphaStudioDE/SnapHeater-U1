/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "api_server.h"
#include "recorder.h"
#include <math.h>
#include "app_config.h"
#include "app_state.h"
#include "profiles.h"
#include "settings_store.h"
#include "settings_deferred.h"
#include "command_validation.h"
#include "ota_integrity.h"
#include "ota_storage.h"
#include "json_guard.h"
#include "job_commands.h"
#include "event_log.h"
#include "heater.h"
#include "safety_latch.h"
#include "safety.h"
#include "ntc.h"
#include "board_panda_breath.h"
#include "control_lease.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "wifi_sta.h"
#include "moonraker_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "nvs.h"
#include "mbedtls/sha256.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lwip/sockets.h"

// One request per connection: no unbounded SDK drain after an early return.
// The socket timeout bounds a single recv; this deadline bounds slow headers too.
typedef struct { int64_t deadline_us; } api_receive_budget_t;

static int api_bounded_recv(httpd_handle_t server, int fd, char *buf, size_t len, int flags) {
    api_receive_budget_t *budget = httpd_sess_get_transport_ctx(server, fd);
    if (!budget || esp_timer_get_time() >= budget->deadline_us)
        return HTTPD_SOCK_ERR_FAIL;
    int received = recv(fd, buf, len, flags);
    if (received <= 0 || esp_timer_get_time() >= budget->deadline_us)
        return HTTPD_SOCK_ERR_FAIL;
    return received;
}

static esp_err_t api_connection_open(httpd_handle_t server, int fd) {
    api_receive_budget_t *budget = calloc(1, sizeof(*budget));
    if (!budget) return ESP_ERR_NO_MEM;
    budget->deadline_us = esp_timer_get_time() + 2000000;
    httpd_sess_set_transport_ctx(server, fd, budget, free);
    return httpd_sess_set_recv_override(server, fd, api_bounded_recv);
}

static esp_err_t api_guarded_handler(httpd_req_t *req) {
    const httpd_uri_t *route = req->user_ctx;
    api_receive_budget_t *budget = httpd_sess_get_transport_ctx(req->handle, httpd_req_to_sockfd(req));
    httpd_resp_set_hdr(req, "Connection", "close");
    if (!budget || esp_timer_get_time() >= budget->deadline_us) return ESP_FAIL;
    const bool upload = strcmp(route->uri, "/update") == 0 || strcmp(route->uri, "/api/v2/update") == 0;
    const bool bodyless = route->method == HTTP_GET || strcmp(route->uri, "/api/v2/boot-inactive") == 0;
    if (bodyless && req->content_len != 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"unexpected_body\"}");
        return ESP_FAIL;
    }
    budget->deadline_us = esp_timer_get_time() + (upload ? 60000000 : 2000000);
    (void)route->handler(req);
    // ESP_FAIL closes the session without httpd_req_delete draining unread data.
    return ESP_FAIL;
}

static const char *TAG = "shu1_api";
#define SHU1_AUTH_HEADER "X-DragonBreath-Auth"
#define SHU1_OTA_BUFFER_SIZE 1024

static void add_common_headers(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
}

static esp_err_t load_control_token(char out[65]) {
    out[0] = '\0';
    nvs_handle_t h;
    esp_err_t err = nvs_open("app_nvs", NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t size = 65;
    err = nvs_get_str(h, "ctl_token", out, &size);
    out[64] = '\0';
    nvs_close(h);
    if (err != ESP_OK) out[0] = '\0';
    return err;
}

static bool auth_ok(httpd_req_t *req) {
    char configured[65];
    char supplied[65] = {0};
    if (load_control_token(configured) != ESP_OK || !configured[0]) return false;
    size_t len = httpd_req_get_hdr_value_len(req, SHU1_AUTH_HEADER);
    if (len == 0 || len >= sizeof(supplied)) return false;
    if (httpd_req_get_hdr_value_str(req, SHU1_AUTH_HEADER, supplied,
                                    sizeof(supplied)) != ESP_OK) return false;
    return strcmp(supplied, configured) == 0;
}

static bool reject_unauthorized(httpd_req_t *req) {
    if (auth_ok(req)) return false;
    add_common_headers(req);
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_sendstr(req,
        "{\"ok\":false,\"error\":\"auth_failed\","
        "\"message\":\"missing/invalid X-DragonBreath-Auth header\"}");
    return true;
}

static bool accepted_image_identity(const esp_app_desc_t *image) {
    return image && (!strcmp(image->project_name, "SnapHeater_U1") ||
                     !strcmp(image->project_name, "dragonbreath") ||
                     !strcmp(image->project_name, "panda_breath"));
}

typedef struct { bool acquired; bool keep; } maintenance_scope_t;
static maintenance_scope_t maintenance_acquire(void) {
    SHU1_CONTROL_GUARD(guard);
    return (maintenance_scope_t){ .acquired = shu1_control_maintenance_begin() };
}
static void maintenance_release(maintenance_scope_t *scope) {
    if (scope->acquired && !scope->keep) {
        SHU1_CONTROL_GUARD(guard);
        shu1_control_maintenance_end();
    }
}

static void delayed_restart_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1200));
    esp_restart();
}

static bool schedule_restart(void) {
    return xTaskCreate(delayed_restart_task, "shu1_restart", 2048, NULL, 5, NULL) == pdPASS;
}

static void add_pin_map(cJSON *root, const char *name, bool deprecated_alias) {
    cJSON *pins = cJSON_AddObjectToObject(root, name);
    cJSON_AddStringToObject(pins, "map_name", "panda_breath_accepted");
    cJSON_AddStringToObject(pins, "safety_state", CONFIG_SHU1_ENABLE_HEATER_OUTPUT
        ? "heater_output_build_enabled_runtime_latch_required"
        : "heater_output_build_disabled");
    cJSON_AddBoolToObject(pins, "deprecated_alias", deprecated_alias);
    cJSON_AddNumberToObject(pins, "heater_gpio", CONFIG_SHU1_HEATER_GPIO);
    cJSON_AddNumberToObject(pins, "fan_gpio", CONFIG_SHU1_FAN_GPIO);
    cJSON_AddNumberToObject(pins, "zero_cross_gpio", CONFIG_SHU1_ZERO_CROSS_GPIO);
    cJSON_AddNumberToObject(pins, "button_gpio", CONFIG_SHU1_BUTTON_GPIO);
    cJSON_AddNumberToObject(pins, "button_auto_gpio", CONFIG_SHU1_BUTTON_AUTO_GPIO);
    cJSON_AddNumberToObject(pins, "button_on_gpio", CONFIG_SHU1_BUTTON_ON_GPIO);
    cJSON_AddNumberToObject(pins, "button_power_gpio", CONFIG_SHU1_BUTTON_POWER_GPIO);
    cJSON_AddNumberToObject(pins, "button_dry_gpio", CONFIG_SHU1_BUTTON_DRY_GPIO);
    cJSON_AddNumberToObject(pins, "led_auto_gpio", CONFIG_SHU1_LED_AUTO_GPIO);
    cJSON_AddNumberToObject(pins, "led_on_gpio", CONFIG_SHU1_LED_ON_GPIO);
    cJSON_AddNumberToObject(pins, "led_dry_gpio", CONFIG_SHU1_LED_DRY_GPIO);
    cJSON_AddNumberToObject(pins, "led_power_gpio", CONFIG_SHU1_ENABLE_POWER_LED ? CONFIG_SHU1_LED_POWER_GPIO : -1);
    cJSON_AddNumberToObject(pins, "chamber_adc_channel", CONFIG_SHU1_CHAMBER_ADC_CH);
    cJSON_AddNumberToObject(pins, "ptc_adc_channel", CONFIG_SHU1_PTC_ADC_CH);
    cJSON_AddBoolToObject(pins, "heater_active_high", CONFIG_SHU1_HEATER_ACTIVE_HIGH);
    cJSON_AddBoolToObject(pins, "fan_active_high", CONFIG_SHU1_FAN_ACTIVE_HIGH);
    cJSON_AddBoolToObject(pins, "fan_triac_control", CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL);
    cJSON_AddStringToObject(pins, "fan_drive_mode", "held_gate_zero_cross_on_immediate_off");
    cJSON_AddNumberToObject(pins, "rref_strap_gpio", CONFIG_SHU1_RREF_STRAP_GPIO);
    cJSON_AddNumberToObject(pins, "rref_kohm", shu1_ntc_rref_kohm());
    cJSON_AddStringToObject(pins, "heater_status", SHU1_BOARD_PIN_STATUS_HEATER);
    cJSON_AddStringToObject(pins, "fan_status", SHU1_BOARD_PIN_STATUS_FAN);
    cJSON_AddStringToObject(pins, "zero_cross_status", SHU1_BOARD_PIN_STATUS_ZERO_CROSS);
    cJSON_AddStringToObject(pins, "chamber_adc_status", SHU1_BOARD_PIN_STATUS_CHAMBER_ADC);
    cJSON_AddStringToObject(pins, "ptc_adc_status", SHU1_BOARD_PIN_STATUS_PTC_ADC);
    cJSON_AddStringToObject(pins, "sensor_status", "dragonbreath_inferred_continuity_required");
    cJSON_AddStringToObject(pins, "button_status", SHU1_BOARD_PIN_STATUS_BUTTONS);
    cJSON_AddStringToObject(pins, "led_status", SHU1_BOARD_PIN_STATUS_LEDS);
}

static void add_ota_info(cJSON *parent) {
    cJSON *root = cJSON_AddObjectToObject(parent, "ota");
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    const esp_partition_t *inactive = esp_ota_get_next_update_partition(NULL);
    cJSON_AddNumberToObject(root, "inactive_capacity", inactive ? inactive->size : 0);
    if (running) cJSON_AddStringToObject(root, "running_slot", running->label);
    if (boot) cJSON_AddStringToObject(root, "boot_slot", boot->label);
    esp_ota_img_states_t image_state;
    if (running && esp_ota_get_state_partition(running, &image_state) == ESP_OK)
        cJSON_AddNumberToObject(root, "running_image_state", image_state);
    esp_app_desc_t desc = {0};
    if (inactive && esp_ota_get_partition_description(inactive, &desc) == ESP_OK) {
        cJSON *slot = cJSON_AddObjectToObject(root, "inactive_slot");
        cJSON_AddStringToObject(slot, "label", inactive->label);
        cJSON_AddStringToObject(slot, "project", desc.project_name);
        cJSON_AddStringToObject(slot, "version", desc.version);
        cJSON_AddBoolToObject(slot, "accepted_identity", accepted_image_identity(&desc));
    } else {
        cJSON_AddNullToObject(root, "inactive_slot");
    }
    cJSON_AddBoolToObject(root, "enabled", shu1_state_get_settings().ota_enabled);
}

static cJSON *state_to_json(void) {
    shu1_state_t st;
    shu1_state_get(&st);
    cJSON *root = cJSON_CreateObject();
    if (root) shu1_wifi_status_json(root);
    cJSON_AddStringToObject(root,"fault_clear_block_reason",shu1_safety_latch_clear_block_reason(&st.settings,&st.runtime));
    cJSON_AddBoolToObject(root,"fault_clear_supported",true);
    cJSON_AddBoolToObject(root,"fault_latched",shu1_safety_latch_is_set());
    cJSON_AddBoolToObject(root,"fault_inhibited",shu1_safety_latch_is_inhibited());
    cJSON_AddStringToObject(root,"fault_reason",shu1_heater_fault_str(shu1_safety_latch_fault()));
    bool pending, persist_ok;
    shu1_settings_deferred_status(&pending, &persist_ok);
    cJSON_AddBoolToObject(root, "settings_pending", pending);
    cJSON_AddBoolToObject(root, "settings_persist_ok", persist_ok);
    shu1_recorder_usage(root);
    shu1_moonraker_setup_status(root);
    cJSON_AddStringToObject(root, "fw_name", SHU1_FW_NAME);
    cJSON_AddStringToObject(root, "fw_version", SHU1_FW_VERSION);
    cJSON_AddBoolToObject(root, "heater_output_build_enabled", CONFIG_SHU1_ENABLE_HEATER_OUTPUT);
    cJSON_AddBoolToObject(root, "gpio_probe_build_enabled", CONFIG_SHU1_ENABLE_GPIO_PROBE);
    cJSON_AddNumberToObject(root, "event_count", shu1_event_log_count());
    shu1_control_snapshot_t ctl;
    shu1_control_snapshot(&ctl);
    cJSON *control = cJSON_AddObjectToObject(root, "control");
    cJSON_AddStringToObject(control, "owner", shu1_control_source_str(ctl.owner));
    cJSON_AddNumberToObject(control, "state_revision", ctl.revision);
    cJSON_AddBoolToObject(control, "lease_active", ctl.lease_active);
    cJSON_AddNumberToObject(control, "lease_remaining_ms", ctl.lease_remaining_ms);
    cJSON_AddBoolToObject(control, "takeover_available", ctl.owner != SHU1_CONTROL_NONE);
    add_ota_info(root);

    add_pin_map(root, "hardware_pins", false);
    add_pin_map(root, "inferred_pins", true);

    cJSON *settings = cJSON_AddObjectToObject(root, "settings");
    cJSON_AddBoolToObject(settings, "work_on", st.settings.work_on);
    cJSON_AddBoolToObject(settings, "paused", st.settings.user_paused);
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
    const int64_t job_now_ms=st.settings.user_paused ? st.settings.user_pause_started_ms : now_ms;
    int64_t remaining_s = (st.settings.drying_end_ms > job_now_ms) ? (st.settings.drying_end_ms - job_now_ms) / 1000 : 0;
    cJSON_AddNumberToObject(settings, "remaining_seconds", (double)remaining_s);
    int64_t preheat_remaining_s = (st.settings.preheat_end_ms > job_now_ms) ? (st.settings.preheat_end_ms - job_now_ms) / 1000 : 0;
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
    cJSON_AddNumberToObject(settings, "cool_release", st.settings.cool_release_c);
    cJSON_AddBoolToObject(settings, "tempering_enabled", st.settings.tempering_enabled);
    cJSON_AddNumberToObject(settings, "tempering_end_temp", st.settings.tempering_end_temp_c);
    cJSON_AddNumberToObject(settings, "tempering_duration_min", st.settings.tempering_duration_min);
    cJSON_AddNumberToObject(settings, "tempering_phase", st.settings.tempering_phase);
    cJSON_AddNumberToObject(settings, "tempering_start_temp", st.settings.tempering_start_temp_c);
    cJSON_AddNumberToObject(settings, "tempering_current_target", st.settings.tempering_current_target_c);
    int64_t tempering_remaining_s = (st.settings.tempering_end_ms > job_now_ms) ? (st.settings.tempering_end_ms - job_now_ms) / 1000 : 0;
    int tempering_progress_pct = 0;
    if (st.settings.tempering_phase == SHU1_TEMPERING_ACTIVE && st.settings.tempering_end_ms > st.settings.tempering_start_ms) {
        int64_t elapsed = job_now_ms - st.settings.tempering_start_ms;
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
    cJSON_AddBoolToObject(settings, "pause_hold_enabled", st.settings.pause_hold_enabled);
    cJSON_AddNumberToObject(settings, "pause_hold_strategy", st.settings.pause_hold_strategy);
    cJSON_AddNumberToObject(settings, "pause_hold_min", st.settings.pause_hold_min);
    cJSON_AddNumberToObject(settings, "pause_lower_after_min", st.settings.pause_lower_after_min);
    cJSON_AddNumberToObject(settings, "pause_lower_by_c", st.settings.pause_lower_by_c);
    cJSON_AddNumberToObject(settings, "pause_stop_after_min", st.settings.pause_stop_after_min);
    cJSON_AddBoolToObject(settings, "dryout_running", st.settings.dryout_running);
    cJSON_AddNumberToObject(settings, "dryout_target", st.settings.dryout_target_temp_c);
    cJSON_AddNumberToObject(settings, "dryout_duration_min", st.settings.dryout_duration_min);
    int64_t dryout_remaining_s = (st.settings.dryout_end_ms > job_now_ms) ? (st.settings.dryout_end_ms - job_now_ms) / 1000 : 0;
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
    cJSON_AddNumberToObject(settings, "post_print_pickup_mode", st.settings.post_print_pickup_mode);
    cJSON_AddNumberToObject(settings, "pickup_keep_warm_min", st.settings.pickup_keep_warm_min);
    cJSON_AddBoolToObject(settings, "pickup_active", st.settings.pickup_active);
    cJSON_AddBoolToObject(settings, "pickup_pending", st.settings.pickup_pending);
    cJSON_AddBoolToObject(settings, "print_risk_enabled", st.settings.print_risk_enabled);
    cJSON_AddNumberToObject(settings, "print_risk_score", st.settings.print_risk_score);
    cJSON_AddBoolToObject(settings, "print_risk_warning_pending", st.settings.print_risk_warning_pending);
    cJSON_AddBoolToObject(settings, "start_print_warning_enabled", st.settings.start_print_warning_enabled);
    cJSON_AddBoolToObject(settings, "start_print_warning_pending", st.settings.start_print_warning_pending);
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
    cJSON_AddNumberToObject(settings, "ota_status", st.settings.ota_status);
    cJSON_AddBoolToObject(settings, "symbiont_mode_enabled", st.settings.symbiont_mode_enabled);
    cJSON_AddBoolToObject(settings, "symbiont_ventilation_allowed", st.settings.symbiont_ventilation_allowed);
    cJSON_AddBoolToObject(settings, "symbiont_safe_control_enabled", st.settings.symbiont_safe_control_enabled);
    cJSON_AddNumberToObject(settings, "symbiont_policy", st.settings.symbiont_policy);
    cJSON_AddBoolToObject(settings, "symbiont_notification_pending", st.settings.symbiont_notification_pending);

    cJSON *runtime = cJSON_AddObjectToObject(root, "runtime");
    cJSON_AddNumberToObject(runtime, "warehouse_temper", st.runtime.chamber_temp_c);
    cJSON_AddNumberToObject(runtime, "ptc_temp", st.runtime.ptc_temp_c);
    cJSON_AddNumberToObject(runtime, "warehouse_instant_temp", st.runtime.chamber_instant_temp_c);
    cJSON_AddNumberToObject(runtime, "ptc_instant_temp", st.runtime.ptc_instant_temp_c);
    cJSON_AddNumberToObject(runtime, "warehouse_temp_offset", shu1_ntc_get_offset_c(0));
    cJSON_AddNumberToObject(runtime, "ptc_temp_offset", shu1_ntc_get_offset_c(1));
    cJSON_AddNumberToObject(runtime, "warehouse_raw", st.runtime.chamber_raw);
    cJSON_AddNumberToObject(runtime, "ptc_raw", st.runtime.ptc_raw);
    cJSON_AddStringToObject(runtime, "warehouse_sensor_status", shu1_sensor_status_str(st.runtime.chamber_sensor_status));
    cJSON_AddStringToObject(runtime, "ptc_sensor_status", shu1_sensor_status_str(st.runtime.ptc_sensor_status));
    cJSON_AddBoolToObject(runtime, "heater_requested", st.runtime.heater_requested);
    cJSON_AddBoolToObject(runtime, "heater_output_on", st.runtime.heater_output_on);
    cJSON_AddNumberToObject(runtime, "heater_commanded_duty",
        isfinite(st.runtime.heater_commanded_duty) ? st.runtime.heater_commanded_duty : 0.0f);
    cJSON_AddNumberToObject(runtime, "heater_approach_limit",
        isfinite(st.runtime.heater_approach_limit) ? st.runtime.heater_approach_limit : 0.0f);
    cJSON_AddNumberToObject(runtime, "heater_effective_target_c", st.runtime.heater_effective_target_c);
    cJSON_AddNumberToObject(runtime, "heater_validation_max_target_c", SHU1_VALIDATION_MAX_TARGET_C);
    cJSON_AddStringToObject(runtime, "heater_constraint",
        st.runtime.heater_constraint[0] ? st.runtime.heater_constraint : "off");
    cJSON_AddBoolToObject(runtime, "fan_output_on", st.runtime.fan_output_on);
    cJSON_AddStringToObject(runtime, "ptc_heater_status", shu1_heater_fault_str(st.runtime.heater_fault));
    cJSON_AddNumberToObject(runtime, "sensor_freeze_warning_ms", (double)st.runtime.sensor_freeze_warning_ms);
    cJSON_AddNumberToObject(runtime, "sensor_freeze_remaining_s", st.runtime.sensor_freeze_remaining_s);
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
    cJSON_AddBoolToObject(runtime, "persistent_fault_latched", shu1_safety_latch_is_set());
    cJSON_AddStringToObject(runtime, "persistent_fault",
                            shu1_heater_fault_str(shu1_safety_latch_fault()));
    cJSON_AddNumberToObject(runtime, "zero_cross_edges", (double)st.runtime.zero_cross_edges);
    cJSON_AddNumberToObject(runtime, "zero_cross_rejected_edges", (double)st.runtime.zero_cross_rejected_edges);
    cJSON_AddNumberToObject(runtime, "zero_cross_edges_per_sec", st.runtime.zero_cross_edges_per_sec);
    cJSON_AddNumberToObject(runtime, "zero_cross_last_period_us", st.runtime.zero_cross_last_period_us);
    cJSON_AddNumberToObject(runtime, "zero_cross_min_period_us", st.runtime.zero_cross_min_period_us);
    cJSON_AddNumberToObject(runtime, "zero_cross_max_period_us", st.runtime.zero_cross_max_period_us);
    cJSON_AddNumberToObject(runtime, "zero_cross_last_edge_ms", (double)st.runtime.zero_cross_last_edge_ms);
    cJSON_AddBoolToObject(runtime, "zero_cross_signal_present", st.runtime.zero_cross_signal_present);
    cJSON_AddNumberToObject(runtime, "incident_report_seq", st.runtime.incident_report_seq);
    cJSON_AddStringToObject(runtime, "incident_summary", st.runtime.incident_summary);
    cJSON_AddStringToObject(runtime, "symbiont_status", st.runtime.symbiont_status);

    cJSON *printer = cJSON_AddObjectToObject(root, "printer");
    cJSON_AddBoolToObject(printer, "moonraker_connected", st.printer.moonraker_connected);
    char device_id[13];
    shu1_device_id(device_id, sizeof(device_id));
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddBoolToObject(printer, "data_ready", !shu1_device_config_restart_required() && st.printer.moonraker_connected &&
        st.printer.klippy_ready && st.printer.subscribed && st.printer.last_update_ms > 0 &&
        now_ms >= st.printer.last_update_ms && now_ms - st.printer.last_update_ms <= SHU1_PRINTER_STALE_MS);
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

static bool has_energy_control_field(cJSON *root) {
    static const char *const fields[] = {
        "work_on", "work_mode", "set_temp", "safe_stop", "emergency_stop", "pause_job",
        "isrunning", "preheat_running", "dryout_running", "health_test_running",
        "scheduled_preheat_enabled", "keep_warm_active", "pickup_active",
        "tempering_enabled", "clear_heater_fault"
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        if (cJSON_GetObjectItemCaseSensitive(root, fields[i])) return true;
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

static esp_err_t send_control_rejection(httpd_req_t *req, shu1_control_result_t result) {
    httpd_resp_set_status(req, result == SHU1_CONTROL_STALE ? "409 Conflict" : "423 Locked");
    const char *error = result == SHU1_CONTROL_STALE ? "stale_revision" :
                        result == SHU1_CONTROL_INVALID_LEASE ? "invalid_lease" :
                        result == SHU1_CONTROL_BUSY ? "owned_by_other_channel" : "not_owner";
    char body[128];
    snprintf(body, sizeof(body), "{\"ok\":false,\"error\":\"%s\"}", error);
    return httpd_resp_sendstr(req, body);
}

static void apply_safe_stop(shu1_settings_t *st) {
    st->user_paused = false;
    st->user_pause_started_ms = 0;
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
    st->session_started_ms = 0;
    st->output_safety_latch_armed = false;
    st->work_mode = SHU1_MODE_AUTO;
}

static esp_err_t read_json_body(httpd_req_t *req, cJSON **out) {
    *out = NULL;
    int len = req->content_len;
    if (len <= 0 || len > 2048) return ESP_ERR_INVALID_SIZE;
    char *buf = calloc(1, len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    int received = 0;
    const int64_t deadline=esp_timer_get_time()+2000000;
    while (received < len) {
        if (esp_timer_get_time()>=deadline) {free(buf);return ESP_ERR_TIMEOUT;}
        int chunk = httpd_req_recv(req, buf + received, len - received);
        if (chunk <= 0 || esp_timer_get_time()>=deadline) {
            free(buf);
            return ESP_FAIL;
        }
        received += chunk;
    }
    cJSON *root = memchr(buf,0,(size_t)len) ? NULL : shu1_json_parse(buf);
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
    if (reject_unauthorized(req)) return ESP_FAIL;
    cJSON *root = NULL;
    if (read_json_body(req, &root) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_json\"}");
        return ESP_FAIL; // Close; never let HTTPD drain an unbounded slow body.
    }

    SHU1_CONTROL_GUARD(policy_guard);
    cJSON *factory_reset = cJSON_GetObjectItem(root, "factory_reset");
    if (shu1_stop_requested(root)) {
        shu1_settings_t stopped=shu1_state_get_settings();
        if(cJSON_IsTrue(cJSON_GetObjectItem(root,"emergency_stop")))
            shu1_safety_latch_trip_volatile(SHU1_HEATER_PANIC_OFF);
        apply_safe_stop(&stopped);
        shu1_control_release_any();
        shu1_state_update_settings_command(&stopped);
        shu1_safety_wake();
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        cJSON *reply=state_to_json();
        char *txt=cJSON_PrintUnformatted(reply);
        httpd_resp_sendstr(req,txt ? txt : "{}");
        cJSON_free(txt);cJSON_Delete(reply);
        return ESP_OK;
    }
    cJSON *receipt = cJSON_GetObjectItemCaseSensitive(root, "virtual_door_ack");
    if (receipt && root->child == receipt && !receipt->next && cJSON_IsNumber(receipt) &&
        receipt->valuedouble > 0 && receipt->valuedouble < 9007199254740992.0) {
        shu1_virtual_door_ack((int64_t)receipt->valuedouble);
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        cJSON *reply = state_to_json();
        char *text = cJSON_PrintUnformatted(reply);
        httpd_resp_sendstr(req, text ? text : "{}");
        cJSON_free(text); cJSON_Delete(reply);
        return ESP_OK;
    }
    if (factory_reset) {
        shu1_control_snapshot_t current;
        shu1_control_snapshot(&current);
        if (!shu1_reset_request_valid(root) || !has_valid_revision(root) ||
            requested_revision(root) != current.revision) {
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            httpd_resp_set_status(req, "400 Bad Request"); httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"reset_requires_dedicated_confirmed_request\"}"); return ESP_OK;
        }
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        esp_err_t reset_err = shu1_settings_store_factory_reset();
        httpd_resp_set_status(req, reset_err == ESP_OK ? "200 OK" : "409 Conflict");
        httpd_resp_sendstr(req, reset_err == ESP_OK ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"reset_rejected_or_failed\"}");
        return ESP_OK;
    }

    shu1_settings_t st = shu1_state_get_settings();
    const bool was_work_on = st.work_on;
    const bool stopping = cJSON_IsTrue(cJSON_GetObjectItem(root, "safe_stop")) ||
        cJSON_IsTrue(cJSON_GetObjectItem(root, "disarm_output_safety_latch")) ||
        cJSON_IsTrue(cJSON_GetObjectItem(root, "emergency_stop")) ||
        cJSON_IsFalse(cJSON_GetObjectItem(root, "work_on"));
    if(!stopping && cJSON_GetObjectItemCaseSensitive(root,"printer_setup")) {
        esp_err_t error=shu1_moonraker_setup_request(root);
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        if(error!=ESP_OK) {
            httpd_resp_set_status(req,error==ESP_ERR_INVALID_ARG ? "400 Bad Request":"409 Conflict");
            httpd_resp_sendstr(req,"{\"ok\":false,\"error\":\"printer_setup_rejected_idle_and_fresh_revision_required\"}");
        } else {
            cJSON *reply=state_to_json();char *text=cJSON_PrintUnformatted(reply);
            httpd_resp_sendstr(req,text ? text:"{}");cJSON_free(text);cJSON_Delete(reply);
        }
        return ESP_OK;
    }
    if(!stopping && (cJSON_GetObjectItem(root,"moonraker_host") || cJSON_GetObjectItem(root,"moonraker_port") || cJSON_GetObjectItem(root,"moonraker_api_key"))) {
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);httpd_resp_set_status(req,"400 Bad Request");
        httpd_resp_sendstr(req,"{\"ok\":false,\"error\":\"use_tested_printer_setup\"}");return ESP_OK;
    }
    cJSON *chamber_offset = cJSON_GetObjectItem(root, "warehouse_temp_offset");
    cJSON *ptc_offset = cJSON_GetObjectItem(root, "ptc_temp_offset");
    const bool calibration = chamber_offset || ptc_offset;
    if(!stopping && (cJSON_GetObjectItem(root,"wifi_ssid") || cJSON_GetObjectItem(root,"wifi_password"))) {
        shu1_control_guard_end(&policy_guard);cJSON_Delete(root);
        httpd_resp_set_status(req,"400 Bad Request");
        httpd_resp_sendstr(req,"{\"ok\":false,\"error\":\"use_ble_wifi_setup\"}");return ESP_OK;
    }
    if (!stopping && shu1_control_maintenance_active() &&
        (cJSON_GetObjectItem(root, "wifi_ssid") || cJSON_GetObjectItem(root, "wifi_password") ||
         cJSON_GetObjectItem(root, "moonraker_host") || cJSON_GetObjectItem(root, "moonraker_port"))) {
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"network_setup_busy\"}");
        return ESP_OK;
    }
    if ((!stopping && shu1_control_maintenance_active()) ||
        (!stopping && calibration && (st.work_on || st.scheduled_preheat_enabled ||
            shu1_control_outputs_busy() ||
            has_energy_control_field(root)))) {
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"maintenance_or_calibration_busy\"}");
        return ESP_OK;
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
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"calibration_requires_single_channel_only\"}");
            return ESP_OK;
        }
        shu1_control_snapshot_t current;
        shu1_control_snapshot(&current);
        if (!has_valid_revision(root) || requested_revision(root) != current.revision ||
            current.owner != SHU1_CONTROL_NONE ||
            (chamber_offset && !cJSON_IsNumber(chamber_offset)) ||
            (ptc_offset && !cJSON_IsNumber(ptc_offset))) {
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return send_control_rejection(req, SHU1_CONTROL_STALE);
        }
    }
    char issued_lease[SHU1_LEASE_ID_LEN + 1] = {0};

    if (!stopping && cJSON_IsTrue(cJSON_GetObjectItem(root, "clear_heater_fault"))) {
        shu1_control_snapshot_t ctl;
        shu1_control_snapshot(&ctl);
        if (!has_valid_revision(root) || requested_revision(root) != ctl.revision) {
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return send_control_rejection(req, SHU1_CONTROL_STALE);
        }
        shu1_control_release_any();
        apply_safe_stop(&st);
        shu1_state_update_settings_command(&st);
        shu1_safety_latch_request_clear();
        shu1_safety_wake();
        shu1_event_log_add("warn", "api_fault_clear_requested",
                           "persistent heater fault clear requested; control task will validate safe conditions");
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        cJSON *reply = state_to_json();
        char *txt = cJSON_PrintUnformatted(reply);
        httpd_resp_sendstr(req, txt ? txt : "{}");
        cJSON_free(txt);
        cJSON_Delete(reply);
        return ESP_OK;
    }

    bool emergency_stop = cJSON_IsTrue(cJSON_GetObjectItem(root, "emergency_stop"));
    if (emergency_stop) shu1_safety_latch_trip_volatile(SHU1_HEATER_PANIC_OFF);
    if (cJSON_IsTrue(cJSON_GetObjectItem(root, "safe_stop")) ||
        cJSON_IsTrue(cJSON_GetObjectItem(root, "disarm_output_safety_latch")) ||
        cJSON_IsFalse(cJSON_GetObjectItem(root, "work_on")) || emergency_stop) {
        shu1_control_release_any();
        apply_safe_stop(&st);
        shu1_state_update_settings_command(&st);
        shu1_safety_wake();
        shu1_control_guard_end(&policy_guard);
        // OFF/disarm is volatile and is never restored armed at boot.
        shu1_event_log_add(emergency_stop ? "critical" : "warn",
                           emergency_stop ? "api_emergency_latched" : "api_safe_stop",
                           emergency_stop ? "API emergency stop latched panic-off for this boot" :
                                            "API safe stop forced heater workflows off and disarmed output latch");
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);

        cJSON *reply = state_to_json();
        char *txt = cJSON_PrintUnformatted(reply);
        httpd_resp_sendstr(req, txt ? txt : "{}");
        cJSON_free(txt);
        cJSON_Delete(reply);
        return ESP_OK;
    }

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

    if (shu1_explicit_job_start(root)) shu1_settings_stop(&st);
    st.work_on = json_bool(root, "work_on", st.work_on);
    st.work_mode = json_int_clamp(root, "work_mode", st.work_mode, SHU1_MODE_AUTO, SHU1_MODE_HEALTH_TEST);
    st.target_temp_c = json_int_clamp(root, "set_temp", st.target_temp_c, 0, CONFIG_SHU1_MAX_TARGET_TEMP_C);
    st.filter_trigger_bed_c = json_int_clamp(root, "filtertemp", st.filter_trigger_bed_c, 0, 120);
    st.heater_trigger_bed_c = json_int_clamp(root, "hotbedtemp", st.heater_trigger_bed_c, 30, 120);
    cJSON *ptc_cutoff = cJSON_GetObjectItem(root, "ptc_cutoff");
    if (cJSON_IsNumber(ptc_cutoff)) {
        int value = ptc_cutoff->valueint;
        if (value <= 0) value = 0;
        else if (value < 90) value = 90;
        else if (value > 104) value = 104;
        st.ptc_cutoff_c = value;
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
    st.post_print_pickup_mode = json_int_clamp(root, "post_print_pickup_mode", st.post_print_pickup_mode, SHU1_PICKUP_OFF, SHU1_PICKUP_NOTIFY_ONLY);
    st.pickup_keep_warm_min = json_int_clamp(root, "pickup_keep_warm_min", st.pickup_keep_warm_min, 1, 720);
    st.print_risk_enabled = json_bool(root, "print_risk_enabled", st.print_risk_enabled);
    st.start_print_warning_enabled = json_bool(root, "start_print_warning_enabled", st.start_print_warning_enabled);
    st.safety_score_enabled = json_bool(root, "safety_score_enabled", st.safety_score_enabled);
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

    shu1_finish_job_command(&st, root, esp_timer_get_time() / 1000);
    if ((st.work_on || st.scheduled_preheat_enabled) && !shu1_control_start_allowed()) {
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"fault_latched\"}");
        return ESP_OK;
    }
    const bool energy_command = has_energy_control_field(root);
    const bool session_mutation = has_session_mutation(root);
    shu1_control_snapshot_t ctl;
    shu1_control_snapshot(&ctl);
    if (energy_command && !st.work_on && !st.scheduled_preheat_enabled) {
        // OFF is accepted from every channel and invalidates the old owner.
        shu1_control_release_any();
    } else if (session_mutation && (st.work_on || st.scheduled_preheat_enabled || ctl.owner != SHU1_CONTROL_NONE)) {
        if (!has_valid_revision(root)) {
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return send_control_rejection(req, SHU1_CONTROL_STALE);
        }
        const bool takeover = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "takeover"));
        shu1_control_result_t result;
        if (!was_work_on || ctl.owner == SHU1_CONTROL_NONE) {
            result = shu1_control_claim(SHU1_CONTROL_REST, takeover || !was_work_on,
                                        requested_revision(root), issued_lease);
        } else if (ctl.owner == SHU1_CONTROL_REST) {
            result = shu1_control_authorize(SHU1_CONTROL_REST, requested_lease(root),
                                            requested_revision(root));
        } else if (takeover) {
            // OFF may be written by any task; only the control task can add energy.
            result = shu1_control_claim(SHU1_CONTROL_REST, true,
                                        requested_revision(root), issued_lease);
        } else {
            result = SHU1_CONTROL_BUSY;
        }
        if (result != SHU1_CONTROL_OK) {
            shu1_control_guard_end(&policy_guard);
            cJSON_Delete(root);
            return send_control_rejection(req, result);
        }
    }
    // Calibration is idle-only and all ownership/revision checks have passed.
    esp_err_t calibration_error = ESP_OK;
    if(calibration) {
        if(!shu1_control_network_setup_begin()) {
            shu1_control_guard_end(&policy_guard);cJSON_Delete(root);
            httpd_resp_set_status(req,"409 Conflict");httpd_resp_sendstr(req,"{\"error\":\"maintenance_busy\"}");return ESP_OK;
        }
        shu1_control_guard_end(&policy_guard);
    }
    if (cJSON_IsNumber(chamber_offset))
        calibration_error = shu1_ntc_set_offset_c(0, (float)chamber_offset->valuedouble);
    if (calibration_error == ESP_OK && cJSON_IsNumber(ptc_offset))
        calibration_error = shu1_ntc_set_offset_c(1, (float)ptc_offset->valuedouble);
    if(calibration) {
        policy_guard=shu1_control_guard_begin();
        st=shu1_state_get_settings(); // Do not overwrite an OFF received during the flash write.
        shu1_control_maintenance_end();
    }
    if (calibration_error != ESP_OK) {
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"calibration_persist_failed\"}");
        return ESP_OK;
    }
    if (calibration) shu1_control_release_any(); // Advance revision for the accepted calibration.
    shu1_settings_limit_targets(&st);
    esp_err_t persist_err = calibration ? ESP_OK : shu1_settings_defer(&st);



    if (persist_err != ESP_OK) {
        // The storage mailbox is unavailable: do not admit a new heating job.
        shu1_safety_latch_inhibit();
        shu1_settings_stop(&st);
        shu1_control_release_any();
        shu1_state_update_settings_command(&st);
        shu1_safety_wake();
        shu1_control_guard_end(&policy_guard);
        cJSON_Delete(root);
        httpd_resp_set_status(req, "500 Internal Server Error"); httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"persist_failed_heater_inhibited\"}"); return ESP_OK;
    }
    shu1_state_update_settings_command(&st);
    shu1_safety_wake();
    shu1_control_guard_end(&policy_guard);

    cJSON_Delete(root);

    cJSON *reply = state_to_json();
    if (issued_lease[0]) cJSON_AddStringToObject(reply, "lease_id", issued_lease);
    char *txt = cJSON_PrintUnformatted(reply);
    httpd_resp_sendstr(req, txt ? txt : "{}");
    cJSON_free(txt);
    cJSON_Delete(reply);
    return ESP_OK;
}

static esp_err_t heartbeat_post_handler(httpd_req_t *req) {
    add_common_headers(req);
    if (reject_unauthorized(req)) return ESP_FAIL;
    cJSON *root = NULL;
    if (read_json_body(req, &root) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_json\"}");
        return ESP_FAIL;
    }
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "lease_id");
    bool valid = cJSON_IsString(item) &&
                 shu1_control_lease_heartbeat_for(SHU1_CONTROL_REST, item->valuestring);
    cJSON_Delete(root);
    if (!valid) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_lease\"}");
        return ESP_OK;
    }
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t probe_post_handler(httpd_req_t *req) {
    add_common_headers(req);
    if (reject_unauthorized(req)) return ESP_FAIL;
    cJSON *root = NULL;
    if (read_json_body(req, &root) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_json\"}");
        return ESP_FAIL;
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

static esp_err_t token_post_handler(httpd_req_t *req) {
    add_common_headers(req);
    if (reject_unauthorized(req)) return ESP_FAIL;
    cJSON *root = NULL;
    if (read_json_body(req, &root) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_json\"}");
        return ESP_FAIL;
    }
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "token");
    const char *token = cJSON_IsString(item) ? item->valuestring : NULL;
    if (!token || strlen(token) < 16 || strlen(token) > 64) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"token_requires_16_to_64_characters\"}");
        return ESP_OK;
    }
    maintenance_scope_t maintenance __attribute__((cleanup(maintenance_release))) = maintenance_acquire();
    if (!maintenance.acquired) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"outputs_or_heating_busy\"}");
        return ESP_OK;
    }
    esp_err_t err = shu1_settings_store_set_control_token(token);
    cJSON_Delete(root);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"nvs_write_failed\"}");
        return ESP_OK;
    }
    char current[65];
    if (load_control_token(current) != ESP_OK || !current[0]) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"token_readback_failed\"}");
        return ESP_OK;
    }
    httpd_resp_sendstr(req, current[0]
        ? "{\"ok\":true,\"token_set\":true}"
        : "{\"ok\":true,\"token_set\":false}");
    return ESP_OK;
}

static esp_err_t auth_check_get_handler(httpd_req_t *req) {
    add_common_headers(req);
    if (reject_unauthorized(req)) return ESP_FAIL;
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t ota_info_get_handler(httpd_req_t *req) {
    add_common_headers(req);
    cJSON *root = cJSON_CreateObject();
    add_ota_info(root);
    char *txt = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, txt ? txt : "{}");
    cJSON_free(txt);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t ota_update_post_handler(httpd_req_t *req) {
    // Always close this connection after sending the reply. HTTPD otherwise
    // drains unread request bytes after an early rejection, outside our deadline.
    add_common_headers(req);
    if (reject_unauthorized(req)) return ESP_FAIL;
    char expected_hex[65];
    uint8_t expected_digest[32];
    if (httpd_req_get_hdr_value_len(req, SHU1_OTA_SHA256_HEADER) != 64 ||
        httpd_req_get_hdr_value_str(req, SHU1_OTA_SHA256_HEADER, expected_hex, sizeof(expected_hex)) != ESP_OK ||
        !shu1_ota_parse_sha256(expected_hex, expected_digest)) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"expected_sha256_required\"}");
        return ESP_FAIL;
    }
    maintenance_scope_t maintenance __attribute__((cleanup(maintenance_release))) = maintenance_acquire();
    if (!maintenance.acquired) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"outputs_or_heating_busy\"}");
        return ESP_FAIL;
    }
    if (!shu1_state_get_settings().ota_enabled) {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"ota_disabled\"}");
        return ESP_FAIL;
    }
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    if (!partition) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"no_inactive_ota_slot\"}");
        return ESP_FAIL;
    }
    if (req->content_len <= 0 || (size_t)req->content_len > partition->size) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_image_size\"}");
        return ESP_FAIL;
    }

    if (shu1_ota_slot_pending(partition,true)!=ESP_OK) {
        httpd_resp_set_status(req,"500 Internal Server Error");
        httpd_resp_sendstr(req,"{\"ok\":false,\"error\":\"ota_marker_failed\"}");
        return ESP_FAIL;
    }
    esp_ota_handle_t update = 0;
    esp_err_t err = esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &update);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"ota_begin_failed\"}");
        return ESP_FAIL;
    }

    uint8_t *buffer = malloc(SHU1_OTA_BUFFER_SIZE);
    if (!buffer) {
        esp_ota_abort(update);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"out_of_memory\"}");
        return ESP_FAIL;
    }
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    if (mbedtls_sha256_starts(&sha, 0) != 0) {
        free(buffer);
        mbedtls_sha256_free(&sha);
        esp_ota_abort(update);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"sha_init_failed\"}");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    bool write_ok = true;
    const int64_t upload_deadline = esp_timer_get_time() + 60000000;
    while (remaining > 0) {
        if (esp_timer_get_time() >= upload_deadline) { write_ok = false; break; }
        int wanted = remaining < SHU1_OTA_BUFFER_SIZE ? remaining : SHU1_OTA_BUFFER_SIZE;
        int got = httpd_req_recv(req, (char *)buffer, wanted);
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (got <= 0 || esp_ota_write(update, buffer, (size_t)got) != ESP_OK ||
            mbedtls_sha256_update(&sha, buffer, (size_t)got) != 0) {
            write_ok = false;
            break;
        }
        remaining -= got;
    }
    uint8_t digest[32] = {0};
    bool hash_ok = write_ok && mbedtls_sha256_finish(&sha, digest) == 0;
    mbedtls_sha256_free(&sha);
    free(buffer);
    if (!write_ok || !hash_ok) {
        esp_ota_abort(update);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"ota_receive_or_write_failed\"}");
        return ESP_FAIL;
    }

    if (memcmp(digest, expected_digest, sizeof(digest)) != 0) {
        esp_ota_abort(update);
        /* A valid ESP image with a wrong transfer hash must not remain
         * selectable through boot-inactive either. */
        if (esp_partition_erase_range(partition, 0, partition->erase_size) != ESP_OK)
            maintenance.keep = true;
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"sha256_mismatch\"}");
        return ESP_FAIL;
    }

    err = esp_ota_end(update);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_esp_image\"}");
        return ESP_FAIL;
    }
    esp_app_desc_t image = {0};
    err = esp_ota_get_partition_description(partition, &image);
    if (err != ESP_OK || !accepted_image_identity(&image)) {
        esp_err_t erase_err = esp_partition_erase_range(partition, 0, partition->erase_size);
        if (erase_err != ESP_OK)
            ESP_LOGE(TAG, "failed to invalidate rejected OTA image: %s", esp_err_to_name(erase_err));
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req,
            "{\"ok\":false,\"error\":\"untrusted_project_identity\","
            "\"accepted\":[\"SnapHeater_U1\",\"dragonbreath\",\"panda_breath\"]}");
        return ESP_FAIL;
    }
    // Only a completely received, hash-matched, valid image may become bootable
    // through this API. Failed uploads remain rejected across power cycles.
    if (shu1_ota_slot_pending(partition,false)!=ESP_OK || !shu1_ota_slot_boot_allowed(partition)) {
        (void)shu1_ota_slot_pending(partition,true);
        httpd_resp_set_status(req,"500 Internal Server Error");
        httpd_resp_sendstr(req,"{\"ok\":false,\"error\":\"ota_marker_failed\"}");
        return ESP_FAIL;
    }
    err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"set_boot_partition_failed\"}");
        return ESP_FAIL;
    }

    maintenance.keep = true; // Boot changed: inhibit until restart.
    char sha_hex[65];
    for (size_t i = 0; i < sizeof(digest); ++i)
        snprintf(sha_hex + i * 2, 3, "%02x", digest[i]);
    cJSON *reply = cJSON_CreateObject();
    cJSON_AddBoolToObject(reply, "ok", true);
    cJSON_AddBoolToObject(reply, "rebooting", true);
    cJSON_AddNumberToObject(reply, "bytes", req->content_len);
    cJSON_AddStringToObject(reply, "sha256", sha_hex);
    cJSON_AddStringToObject(reply, "slot", partition->label);
    cJSON_AddStringToObject(reply, "project", image.project_name);
    cJSON_AddStringToObject(reply, "version", image.version);
    char *txt = cJSON_PrintUnformatted(reply);
    httpd_resp_sendstr(req, txt ? txt : "{}");
    cJSON_free(txt);
    cJSON_Delete(reply);
    shu1_event_log_add("warn", "ota_accepted", "validated image written to inactive slot; reboot scheduled");
    if (!schedule_restart()) ESP_LOGE(TAG, "OTA accepted but restart task could not be created");
    return ESP_FAIL;
}

static esp_err_t boot_inactive_post_handler(httpd_req_t *req) {
    add_common_headers(req);
    if (reject_unauthorized(req)) return ESP_FAIL;
    maintenance_scope_t maintenance __attribute__((cleanup(maintenance_release))) = maintenance_acquire();
    if (!maintenance.acquired) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"outputs_or_heating_busy\"}");
        return ESP_OK;
    }
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    esp_app_desc_t image = {0};
    if (!shu1_ota_slot_boot_allowed(partition)) {
        httpd_resp_set_status(req,"409 Conflict");
        httpd_resp_sendstr(req,"{\"ok\":false,\"error\":\"inactive_upload_not_verified\"}");
        return ESP_OK;
    }
    if (!partition || esp_ota_get_partition_description(partition, &image) != ESP_OK) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"empty_inactive_slot\"}");
        return ESP_OK;
    }
    if (!accepted_image_identity(&image)) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"untrusted_inactive_image\"}");
        return ESP_OK;
    }
    if (esp_ota_set_boot_partition(partition) != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"set_boot_partition_failed\"}");
        return ESP_OK;
    }
    maintenance.keep = true;
    char body[192];
    snprintf(body, sizeof(body),
             "{\"ok\":true,\"rebooting\":true,\"slot\":\"%s\",\"project\":\"%s\",\"version\":\"%s\"}",
             partition->label, image.project_name, image.version);
    httpd_resp_sendstr(req, body);
    shu1_event_log_add("warn", "boot_inactive", "validated inactive application selected for next boot");
    if (!schedule_restart()) ESP_LOGE(TAG, "inactive slot selected but restart task could not be created");
    return ESP_OK;
}

static esp_err_t events_get_handler(httpd_req_t *req) {
    add_common_headers(req);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "count", shu1_event_log_count());
    cJSON_AddItemToObject(root, "events", shu1_event_log_to_json());
    cJSON_AddItemToObject(root, "notifications", shu1_event_notifications());
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

static esp_err_t history_get_handler(httpd_req_t *req) {
    add_common_headers(req);
    char query[48], value[16];
    uint32_t after=0;
    if(httpd_req_get_url_query_len(req)>0) {
        if(httpd_req_get_url_query_str(req,query,sizeof(query))!=ESP_OK ||
            httpd_query_key_value(query,"after",value,sizeof(value))!=ESP_OK || !value[0])
            goto bad_query;
        uint64_t parsed=0;
        for(const char *p=value;*p;p++) {
            if(*p<'0' || *p>'9') goto bad_query;
            parsed=parsed*10+(unsigned)(*p-'0');
            if(parsed>UINT32_MAX) goto bad_query;
        }
        after=(uint32_t)parsed;
    }
    cJSON *root=shu1_recorder_page(after);
    char *txt=root?cJSON_PrintUnformatted(root):NULL;
    if(!txt) {
        httpd_resp_set_status(req,"503 Service Unavailable");
        httpd_resp_sendstr(req,"{\"error\":\"history_unavailable\"}");
    } else httpd_resp_sendstr(req,txt);
    cJSON_free(txt); cJSON_Delete(root);
    return ESP_OK;
bad_query:
    httpd_resp_set_status(req,"400 Bad Request");
    httpd_resp_sendstr(req,"{\"error\":\"invalid_history_cursor\"}");
    return ESP_OK;
}

esp_err_t shu1_api_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = SHU1_API_PORT;
    config.ctrl_port = 32768;
    config.max_uri_handlers = 20;
    config.stack_size = 8192;
    config.recv_wait_timeout = 1;
    config.send_wait_timeout = 1;
    config.lru_purge_enable = true;
    config.open_fn = api_connection_open;
    httpd_handle_t server = NULL;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG, "http server start failed");

    static const httpd_uri_t health = {.uri = "/api/health", .method = HTTP_GET, .handler = health_get_handler};
    static const httpd_uri_t history = {.uri = "/api/history", .method = HTTP_GET, .handler = history_get_handler};
    static const httpd_uri_t status = {.uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler};
    static const httpd_uri_t settings_post = {.uri = "/api/settings", .method = HTTP_POST, .handler = settings_post_handler};
    static const httpd_uri_t probe_post = {.uri = "/api/probe", .method = HTTP_POST, .handler = probe_post_handler};
    static const httpd_uri_t token_post = {.uri = "/api/token", .method = HTTP_POST, .handler = token_post_handler};
    static const httpd_uri_t token_v2_post = {.uri = "/api/v2/token", .method = HTTP_POST, .handler = token_post_handler};
    static const httpd_uri_t heartbeat_post = {.uri = "/api/heartbeat", .method = HTTP_POST, .handler = heartbeat_post_handler};
    static const httpd_uri_t heartbeat_v2_post = {.uri = "/api/v2/heartbeat", .method = HTTP_POST, .handler = heartbeat_post_handler};
    static const httpd_uri_t events = {.uri = "/api/events", .method = HTTP_GET, .handler = events_get_handler};
    static const httpd_uri_t ota_info = {.uri = "/api/v2/ota", .method = HTTP_GET, .handler = ota_info_get_handler};
    static const httpd_uri_t auth_check = {.uri = "/api/v2/auth", .method = HTTP_GET, .handler = auth_check_get_handler};
    static const httpd_uri_t ota_update = {.uri = "/update", .method = HTTP_POST, .handler = ota_update_post_handler};
    static const httpd_uri_t ota_update_v2 = {.uri = "/api/v2/update", .method = HTTP_POST, .handler = ota_update_post_handler};
    static const httpd_uri_t boot_inactive = {.uri = "/api/v2/boot-inactive", .method = HTTP_POST, .handler = boot_inactive_post_handler};

    const httpd_uri_t *routes[] = {
        &health, &history, &status, &settings_post, &probe_post, &token_post,
        &token_v2_post, &heartbeat_post, &heartbeat_v2_post, &events,
        &auth_check, &ota_info, &ota_update, &ota_update_v2, &boot_inactive,
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        httpd_uri_t guarded = *routes[i];
        guarded.handler = api_guarded_handler;
        guarded.user_ctx = (void *)routes[i];
        esp_err_t err = httpd_register_uri_handler(server, &guarded);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "route registration failed for %s: %s",
                     routes[i]->uri, esp_err_to_name(err));
            httpd_stop(server);
            return err;
        }
    }
    ESP_LOGI(TAG, "REST API ready on port %d", SHU1_API_PORT);
    return ESP_OK;
}
