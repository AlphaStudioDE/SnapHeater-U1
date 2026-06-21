/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHU1_SENSOR_OK = 0,
    SHU1_SENSOR_SHORT = 1,
    SHU1_SENSOR_OPEN = 2,
    SHU1_SENSOR_INVALID = 3,
} shu1_sensor_status_t;

typedef enum {
    SHU1_HEATER_OK = 0,
    SHU1_HEATER_SENSOR_FAULT = 1,
    SHU1_HEATER_OVERTEMP = 2,
    SHU1_HEATER_NO_RISE = 3,
    SHU1_HEATER_DISABLED_BY_BUILD = 4,
    SHU1_HEATER_DISABLED_BY_PROBE_LOCK = 5,
    SHU1_HEATER_DRYING_TIMER_EXPIRED = 6,
    SHU1_HEATER_PRINTER_NOT_READY = 7,
    SHU1_HEATER_PREHEAT_COMPLETE = 8,
    SHU1_HEATER_SESSION_TIMEOUT = 9,
    SHU1_HEATER_DRYOUT_COMPLETE = 10,
    SHU1_HEATER_HEALTH_TEST_COMPLETE = 11,
    SHU1_HEATER_HEALTH_TEST_FAILED = 12,
    SHU1_HEATER_DOOR_OPEN = 13,
} shu1_heater_fault_t;

typedef enum {
    SHU1_OUTPUT_HEATER = 1,
    SHU1_OUTPUT_FAN = 2,
} shu1_output_t;

typedef struct {
    bool moonraker_connected;
    bool klippy_ready;
    bool subscribed;
    bool autodetect_done;
    bool chamber_sensor_online;
    bool cavity_fan_online;

    char webhooks_state[24];
    char print_state[32];
    char normalized_state[24];
    char filename[96];

    float print_progress;
    float print_duration_sec;
    float total_duration_sec;

    int active_tool;
    char active_tool_object[16];
    char active_material[32];
    char active_color_rgba[16];

    float bed_temp;
    float bed_target;
    float extruder_temp;
    float extruder_target;
    float active_tool_temp;

    float u1_chamber_temp;
    char u1_chamber_object[64];
    float cavity_fan_speed;
    char cavity_fan_object[64];

    int64_t last_update_ms;
    int64_t last_ws_message_ms;
    int64_t last_autodetect_ms;
} shu1_printer_state_t;

typedef struct {
    bool work_on;
    int work_mode;
    int drying_mode;
    bool drying_running;
    int target_temp_c;
    int filter_trigger_bed_c;
    int heater_trigger_bed_c;
    int custom_temp_c;
    int custom_timer_h;
    int ptc_cutoff_c;
    int64_t drying_end_ms;

    // SnapHeater-only preheat/hold scenario.
    // User selects target temperature and hold time; countdown starts after target is reached.
    bool preheat_running;
    int preheat_target_temp_c;
    int preheat_hold_min;
    int preheat_phase;
    int64_t preheat_hold_start_ms;
    int64_t preheat_end_ms;
    bool preheat_complete_pending;

    // User/material presets and general safety watchdog.
    int material_profile;
    int manual_session_max_min;
    int64_t session_started_ms;
    bool session_timeout_pending;

    // Fan/cooldown policy. Fan remains on after heater turns off for this many minutes.
    int fan_postrun_min;

    // Print-finish tempering: optional user/app scenario after AUTO print completion.
    // Android enables it and chooses a duration. Firmware ramps chamber target
    // linearly from the final print/chamber temperature to tempering_end_temp_c
    // (default 0 = heater-off target), then disables heating completely.
    bool tempering_enabled;
    int tempering_end_temp_c;
    int tempering_duration_min;
    int tempering_phase;
    int tempering_start_temp_c;
    int tempering_current_target_c;
    int64_t tempering_start_ms;
    int64_t tempering_end_ms;
    bool tempering_complete_pending;

    // v1.0 feature pack: app-selectable smart chamber behavior.
    bool auto_material_profile_enabled;       // Optional: apply profile from U1 filament_type when explicitly enabled.
    bool material_mismatch_warning_enabled;   // Warn if selected profile differs from U1 filament_type; never stops heating.
    bool material_mismatch_pending;           // Android should show a notification.
    int material_mismatch_user_profile;       // Profile chosen in app/SnapHeater.
    int material_mismatch_printer_profile;    // Profile detected from Snapmaker U1 active tool/material.
    int64_t material_mismatch_detected_ms;
    bool anti_warp_enabled;                   // Smooth chamber control for ABS/ASA/PETG large prints.
    bool large_print_protection_enabled;      // Prefer stability, less aggressive target changes and longer post-run.
    bool safe_overnight_enabled;              // Conservative limits and mandatory printer freshness for unattended prints.

    // Pause behavior is intentionally user configurable. Pause/error from U1 is not a local heater fault.
    bool pause_hold_enabled;
    int pause_hold_strategy;                  // SHU1_PAUSE_HOLD_*
    int pause_hold_min;
    int pause_lower_after_min;
    int pause_lower_by_c;
    int pause_stop_after_min;
    int64_t pause_seen_start_ms;

    // Chamber dry-out: low/medium heat + fan before a print to dry the chamber interior.
    bool dryout_running;
    int dryout_target_temp_c;
    int dryout_duration_min;
    int64_t dryout_end_ms;
    bool dryout_complete_pending;

    // Preheat scheduler: Android may schedule a relative local start; phone should still notify user.
    bool scheduled_preheat_enabled;
    int scheduled_preheat_delay_min;
    int scheduled_preheat_target_c;
    int scheduled_preheat_hold_min;
    int64_t scheduled_preheat_start_ms;
    bool scheduled_preheat_started_pending;

    // Print finish conditioning. Tempering remains one selectable conditioning option.
    int finish_conditioning_mode;             // SHU1_FINISH_*
    int keep_warm_temp_c;
    int keep_warm_max_min;
    int64_t keep_warm_end_ms;
    bool keep_warm_active;

    // Optional future physical door/lid sensor. Disabled unless user confirms hardware.
    bool door_sensor_enabled;
    bool door_open;
    bool door_open_pending;

    // v1.1 software-only Virtual Door/Open Lid Detection.
    // Infers probable opening from sudden chamber temperature drop after print finish,
    // during tempering, keep-warm or cooldown. Thresholds are user-calibrated later.
    bool virtual_door_detection_enabled;
    int virtual_door_window_sec;
    int virtual_door_drop_c;
    int virtual_door_rate_c_per_min;
    int virtual_door_min_base_temp_c;
    int virtual_door_action;                  // SHU1_VDOOR_ACTION_*
    bool virtual_door_open;
    bool virtual_door_open_pending;
    int64_t virtual_door_detected_ms;
    int64_t virtual_door_window_start_ms;
    float virtual_door_window_start_temp_c;
    float virtual_door_last_drop_c;
    float virtual_door_last_rate_c_per_min;

    // v1.3 extended intelligent chamber functions.
    bool warmup_prediction_enabled;
    bool heat_soak_enabled;
    int heat_soak_min;
    int heat_soak_band_c;
    int heat_soak_phase;
    int64_t heat_soak_start_ms;
    int64_t heat_soak_end_ms;
    bool heat_soak_complete_pending;
    bool chamber_stability_lock_enabled;
    int stability_lock_band_c;
    int stability_lock_min;
    bool filter_life_counter_enabled;
    int filter_life_limit_h;
    bool filter_life_warning_pending;
    bool heater_wear_tracking_enabled;
    int heater_wear_warning_pct;
    bool heater_wear_warning_pending;
    bool airflow_detection_enabled;
    bool airflow_warning_pending;
    bool pla_protection_enabled;
    bool pla_protection_confirmed;
    bool pla_protection_pending;
    bool smart_resume_enabled;
    int resume_recover_min;
    bool resume_recover_active;
    int64_t resume_recover_end_ms;
    int post_print_pickup_mode;
    int pickup_keep_warm_min;
    bool pickup_active;
    bool pickup_pending;
    bool print_risk_enabled;
    int print_risk_score;
    bool print_risk_warning_pending;
    bool start_print_warning_enabled;
    bool start_print_warning_pending;
    bool local_recipes_enabled;
    int active_recipe_slot;
    char active_recipe_name[32];
    bool demo_mode_enabled;
    int demo_phase;
    int64_t demo_started_ms;
    bool safety_score_enabled;
    int safety_score;
    bool setup_validation_passed;
    bool setup_warning_pending;


    // v1.4 productization/safety pack.
    bool first_setup_wizard_enabled;
    int first_setup_step;
    bool first_setup_complete;
    bool temp_history_enabled;
    int history_sample_period_sec;
    bool incident_report_enabled;
    bool incident_report_pending;
    int incident_report_seq;
    char incident_last_reason[64];
    bool output_safety_latch_enabled;
    bool output_safety_latch_armed;
    bool heater_output_verified;
    bool fan_output_verified;
    bool sensors_verified;
    bool moonraker_verified;
    int notification_min_level;
    int language_code;
    bool local_only_mode;
    bool ota_enabled;
    bool ota_rollback_placeholder_enabled;
    int ota_status;
    bool contest_showcase_mode_enabled;

    // U1 Symbiont Mode: friendly, explicit cooperation with Snapmaker U1.
    // Default policy remains read-only; climate/ventilation cooperation may be
    // enabled later only through a safe whitelist and explicit user consent.
    bool symbiont_mode_enabled;
    bool symbiont_ventilation_allowed;
    bool symbiont_safe_control_enabled;
    int symbiont_policy;
    bool symbiont_notification_pending;

    // Heater health test: short diagnostic cycle based on temperature rise.
    bool health_test_running;
    int health_test_phase;
    int health_test_target_c;
    int health_test_duration_sec;
    int health_test_result;
    int64_t health_test_start_ms;
    float health_test_start_ptc_c;
    float health_test_start_chamber_c;
    bool health_test_complete_pending;
} shu1_settings_t;

typedef struct {
    bool active;
    int64_t started_ms;
    int64_t check_due_ms;
    float start_ptc_c;
    float start_chamber_c;
} shu1_rise_detector_t;

typedef struct {
    float chamber_temp_c;
    float ptc_temp_c;
    int chamber_raw;
    int ptc_raw;
    shu1_sensor_status_t chamber_sensor_status;
    shu1_sensor_status_t ptc_sensor_status;
    bool heater_requested;
    bool heater_output_on;
    bool fan_output_on;
    shu1_heater_fault_t heater_fault;
    int64_t last_sensor_ms;
    int64_t last_heat_off_ms;
    shu1_rise_detector_t rise_detector;

    // Energy/stability statistics for Android summaries.
    uint64_t heater_on_accum_ms;
    uint64_t session_heater_on_ms;
    float estimated_energy_wh;
    float session_energy_wh;
    bool stability_active;
    int stability_target_c;
    int stability_band_c;
    uint32_t stability_samples;
    uint32_t stability_within_band_samples;
    float stability_min_c;
    float stability_max_c;
    float stability_sum_c;
    int stability_score_pct;
    bool stability_report_pending;

    int warmup_eta_sec;
    float warmup_rate_c_per_min;
    int heat_soak_remaining_sec;
    bool heat_soak_ready;
    uint64_t fan_on_accum_ms;
    int filter_life_pct;
    float last_health_ptc_rise_c;
    float last_health_chamber_rise_c;
    float heater_health_baseline_ptc_rise_c;
    int heater_wear_pct;
    int airflow_score_pct;
    bool airflow_warning_pending;
    int print_risk_score;
    bool print_risk_warning_pending;
    char print_risk_message[192];
    bool start_print_warning_pending;
    int safety_score;
    bool setup_validation_passed;
    char safety_message[192];


    int notification_level;
    char notification_code[48];
    char notification_message[192];
    int64_t notification_ms;

    int history_head;
    int history_count;
    int64_t history_last_sample_ms;
    float history_chamber_c[SHU1_HISTORY_SLOTS];
    float history_ptc_c[SHU1_HISTORY_SLOTS];
    float history_target_c[SHU1_HISTORY_SLOTS];
    uint8_t history_heater_on[SHU1_HISTORY_SLOTS];
    uint8_t history_fan_on[SHU1_HISTORY_SLOTS];

    int incident_report_seq;
    char incident_summary[256];
    bool output_safety_latch_ready;
    char symbiont_status[192];

    char material_advice[160];
    char material_mismatch_message[192];

    int physical_last_button;
    int64_t physical_last_button_ms;
    char physical_panel_status[160];
} shu1_runtime_t;

typedef struct {
    shu1_settings_t settings;
    shu1_runtime_t runtime;
    shu1_printer_state_t printer;
} shu1_state_t;

void shu1_state_init(void);
SemaphoreHandle_t shu1_state_mutex(void);
void shu1_state_get(shu1_state_t *out);
void shu1_state_update_settings(const shu1_settings_t *settings);
void shu1_state_update_runtime(const shu1_runtime_t *runtime);
void shu1_state_update_printer(const shu1_printer_state_t *printer);
shu1_settings_t shu1_state_get_settings(void);
shu1_runtime_t shu1_state_get_runtime(void);
shu1_printer_state_t shu1_state_get_printer(void);

const char *shu1_sensor_status_str(shu1_sensor_status_t status);
const char *shu1_heater_fault_str(shu1_heater_fault_t fault);

#ifdef __cplusplus
}
#endif
