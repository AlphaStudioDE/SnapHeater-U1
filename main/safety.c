/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "safety.h"
#include "safety_rules.h"
#include "app_config.h"
#include "app_state.h"
#include "ntc.h"
#include "heater.h"
#include "heater_pid.h"
#include "safety_latch.h"
#include "control_lease.h"
#include "fan_triac.h"
#include "ble_control.h"
#include "event_log.h"
#include "profiles.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "shu1_safety";
static shu1_heater_fault_t g_last_reported_fault = SHU1_HEATER_OK;
static char g_last_auto_printer_state[24] = "idle";
static bool g_auto_print_context_seen = false;
static bool g_last_printing_for_start_warning = false;
static bool g_last_pause_for_resume = false;
static float g_warm_prev_temp = NAN;
static int64_t g_warm_prev_ms = 0;
static float g_warm_rate_ema = 0.0f;
static int64_t g_airflow_start_ms = 0;
static float g_airflow_start_chamber = NAN;
static float g_airflow_start_ptc = NAN;
static uint64_t g_last_zc_edges = 0;
static int64_t g_last_zc_sample_ms = 0;
static bool g_ptc_foldback_active = false;
static shu1_pid_state_t g_heater_pid;
static volatile bool g_control_task_healthy = false;
static TaskHandle_t g_control_task_handle = NULL;


static bool str_eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static bool printer_state_is_printing(const shu1_printer_state_t *p) {
    return str_eq(p->normalized_state, "printing") || str_eq(p->print_state, "printing");
}

static bool printer_state_is_pause_or_error(const shu1_printer_state_t *p) {
    // These are printer states, not heater/sensor faults. Holding chamber temperature
    // during a printer pause/error can reduce rapid material shrinkage.
    return str_eq(p->normalized_state, "paused") || str_eq(p->normalized_state, "error") ||
           str_eq(p->normalized_state, "timeout") || str_eq(p->print_state, "paused") ||
           str_eq(p->print_state, "error");
}

static bool printer_state_is_complete_or_idle(const shu1_printer_state_t *p) {
    return str_eq(p->normalized_state, "complete") || str_eq(p->normalized_state, "idle") ||
           str_eq(p->print_state, "complete") || str_eq(p->print_state, "idle") ||
           str_eq(p->print_state, "standby");
}

static bool printer_data_fresh(const shu1_printer_state_t *p, int64_t now_ms) {
    if (!p->moonraker_connected) return false;
    if (p->last_update_ms <= 0) return false;
    return (now_ms - p->last_update_ms) <= SHU1_PRINTER_STALE_MS;
}

static int clamp_target(int v) {
    if (v > CONFIG_SHU1_MAX_TARGET_TEMP_C) v = CONFIG_SHU1_MAX_TARGET_TEMP_C;
    if (v > SHU1_VALIDATION_MAX_TARGET_C) v = SHU1_VALIDATION_MAX_TARGET_C;
    if (v < 0) v = 0;
    return v;
}

static int effective_target_for_settings(const shu1_settings_t *s) {
    if (s->tempering_phase == SHU1_TEMPERING_ACTIVE) {
        // During tempering the target may intentionally ramp all the way down to 0,
        // which means "stop requesting heat while the countdown finishes". Do not
        // fall back to the old print target when current target reaches 0.
        return clamp_target(s->tempering_current_target_c);
    }
    if (s->keep_warm_active) {
        return clamp_target(s->keep_warm_temp_c);
    }
    if (s->work_mode == SHU1_MODE_PREHEAT) {
        if (!s->preheat_running) return 0;
        return clamp_target(s->preheat_target_temp_c);
    }
    if (s->work_mode == SHU1_MODE_DRY_OUT) {
        if (!s->dryout_running) return 0;
        return clamp_target(s->dryout_target_temp_c);
    }
    if (s->work_mode == SHU1_MODE_HEALTH_TEST) {
        if (!s->health_test_running) return 0;
        return clamp_target(s->health_test_target_c);
    }
    if (s->work_mode == SHU1_MODE_DRYING) {
        if (!s->drying_running) return 0;
        if (s->drying_mode == SHU1_DRYING_PLA) return 55;
        if (s->drying_mode == SHU1_DRYING_PETG) return 60;
        if (s->drying_mode == SHU1_DRYING_ABS) return 60;
        return clamp_target(s->custom_temp_c);
    }
    return clamp_target(s->target_temp_c);
}

static void start_tempering_if_user_enabled(shu1_settings_t *s, const shu1_runtime_t *rt, int64_t now_ms) {
    // Tempering is intentionally a user/app option, not an automatic hidden rule.
    // The Android app/REST/BLE must set tempering_enabled=true before print completion
    // if the user wants a controlled post-print temperature ramp-down.
    if (!s->tempering_enabled) return;
    if (s->work_mode != SHU1_MODE_AUTO) return;
    if (s->tempering_phase == SHU1_TEMPERING_ACTIVE) return;
    if (!g_auto_print_context_seen) return;

    int start_temp = s->target_temp_c;
    if (isfinite(rt->chamber_temp_c) && rt->chamber_temp_c > (float)start_temp) {
        start_temp = (int)ceilf(rt->chamber_temp_c);
    }
    start_temp = clamp_target(start_temp);
    int end_temp = s->tempering_end_temp_c;
    // Android may set end_temp=0 to mean "ramp to heater-off". This is the
    // default for v0.9.0 because the user chooses duration, not a hidden final hold.
    if (end_temp < 0) end_temp = 0;
    if (end_temp > start_temp) end_temp = start_temp;

    s->tempering_start_temp_c = start_temp;
    s->tempering_current_target_c = start_temp;
    s->tempering_end_temp_c = end_temp;
    s->tempering_start_ms = now_ms;
    int duration_min = s->tempering_duration_min;
    if (duration_min < 0) duration_min = 0;
    if (duration_min > 240) duration_min = 240;
    s->tempering_duration_min = duration_min;
    s->tempering_end_ms = now_ms + ((int64_t)duration_min * 60 * 1000);
    s->tempering_phase = duration_min == 0 ? SHU1_TEMPERING_COMPLETE : SHU1_TEMPERING_ACTIVE;
    s->tempering_complete_pending = duration_min == 0;

    ESP_LOGW(TAG, "auto print finished: tempering ramp start=%d end=%d duration=%d min", start_temp, end_temp, duration_min);
    shu1_event_log_add("info", "tempering_started", "AUTO print finished; user-enabled linear chamber target ramp-down started");
    shu1_ble_notify_status_now();
}

static void update_tempering(shu1_settings_t *s, int64_t now_ms) {
    if (s->tempering_phase != SHU1_TEMPERING_ACTIVE) return;
    if (s->tempering_duration_min <= 0 || s->tempering_end_ms <= s->tempering_start_ms) {
        s->tempering_current_target_c = s->tempering_end_temp_c;
        s->tempering_phase = SHU1_TEMPERING_COMPLETE;
        s->tempering_complete_pending = true;
        s->work_on = false;
        shu1_event_log_add("info", "tempering_complete", "tempering completed; heater disabled");
        shu1_ble_notify_status_now();
        return;
    }

    if (now_ms >= s->tempering_end_ms) {
        s->tempering_current_target_c = s->tempering_end_temp_c;
        s->tempering_phase = SHU1_TEMPERING_COMPLETE;
        s->tempering_complete_pending = true;
        s->work_on = false;
        g_auto_print_context_seen = false;
        shu1_event_log_add("info", "tempering_complete", "linear chamber target ramp-down completed; heater disabled");
        shu1_ble_notify_status_now();
        return;
    }

    float t = (float)(now_ms - s->tempering_start_ms) / (float)(s->tempering_end_ms - s->tempering_start_ms);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float target = (float)s->tempering_start_temp_c + ((float)s->tempering_end_temp_c - (float)s->tempering_start_temp_c) * t;
    // Round upward while cooling so the virtual target never drops faster than requested.
    // If end_temp is 0, this naturally reaches heater-off at the end of the user-selected time.
    s->tempering_current_target_c = (int)ceilf(target);
}

static bool preheat_update(shu1_settings_t *s, const shu1_runtime_t *rt, int64_t now_ms) {
    if (s->work_mode != SHU1_MODE_PREHEAT || !s->preheat_running) {
        if (s->work_mode != SHU1_MODE_PREHEAT && s->preheat_phase != SHU1_PREHEAT_COMPLETE) {
            s->preheat_phase = SHU1_PREHEAT_IDLE;
            s->preheat_hold_start_ms = 0;
            s->preheat_end_ms = 0;
        }
        return false;
    }

    // Preheat is intentionally independent of U1 print state and bed temperature.
    int target = clamp_target(s->preheat_target_temp_c);
    s->preheat_target_temp_c = target;

    if (s->preheat_hold_min < 1) s->preheat_hold_min = 1;
    if (s->preheat_hold_min > 240) s->preheat_hold_min = 240;

    if (s->preheat_phase == SHU1_PREHEAT_IDLE || s->preheat_phase == SHU1_PREHEAT_COMPLETE) {
        s->preheat_phase = SHU1_PREHEAT_HEATING;
        s->preheat_hold_start_ms = 0;
        s->preheat_end_ms = 0;
        s->preheat_complete_pending = false;
    }

    if (s->preheat_phase == SHU1_PREHEAT_HEATING) {
        if (isfinite(rt->chamber_temp_c) && rt->chamber_temp_c >= (float)target - SHU1_PREHEAT_REACHED_BAND_C) {
            s->preheat_phase = SHU1_PREHEAT_HOLDING;
            s->preheat_hold_start_ms = now_ms;
            s->preheat_end_ms = now_ms + ((int64_t)s->preheat_hold_min * 60 * 1000);
            ESP_LOGI(TAG, "preheat target reached: chamber=%.1f target=%d hold=%d min", rt->chamber_temp_c, target, s->preheat_hold_min);
            shu1_event_log_add("info", "preheat_target_reached", "preheat target reached; hold timer started");
            shu1_ble_notify_status_now();
        }
        return false;
    }

    if (s->preheat_phase == SHU1_PREHEAT_HOLDING && s->preheat_end_ms > 0 && now_ms >= s->preheat_end_ms) {
        s->preheat_running = false;
        s->work_on = false;
        s->preheat_phase = SHU1_PREHEAT_COMPLETE;
        s->preheat_complete_pending = true;
        ESP_LOGW(TAG, "preheat hold complete");
        shu1_event_log_add("info", "preheat_complete", "preheat/hold cycle completed");
        shu1_ble_notify_status_now();
        return true;
    }

    return false;
}

static bool drying_timer_expired(shu1_settings_t *s, int64_t now_ms) {
    if (s->work_mode != SHU1_MODE_DRYING || !s->drying_running) return false;
    if (s->drying_end_ms <= 0) return false;
    if (now_ms < s->drying_end_ms) return false;
    s->drying_running = false;
    s->work_on = false;
    s->drying_end_ms = 0;
    shu1_event_log_add("info", "drying_complete", "filament drying timer expired");
    return true;
}

static bool auto_mode_heater_allowed(const shu1_settings_t *s, const shu1_printer_state_t *p, int64_t now_ms) {
    if (s->work_mode != SHU1_MODE_AUTO) return true;
    if (s->tempering_phase == SHU1_TEMPERING_ACTIVE) return true;
    if (!printer_data_fresh(p, now_ms)) return false;

    if (printer_state_is_printing(p)) {
        // AUTO starts only from a real print context. Bed threshold is only an AUTO permission,
        // never a condition for Preheat/Drying/Manual.
        if (p->bed_target >= (float)s->heater_trigger_bed_c) return true;
        if (p->bed_temp >= (float)(s->heater_trigger_bed_c - 5)) return true;
        return false;
    }

#if SHU1_AUTO_PAUSE_ERROR_HOLD_ENABLED
    if (printer_state_is_pause_or_error(p) && g_auto_print_context_seen) {
        // Printer pause/error is not a heater fault. Keep chamber hold to reduce shrinkage.
        return true;
    }
#endif

    return false;
}

static void update_auto_context(shu1_settings_t *s, const shu1_printer_state_t *p, const shu1_runtime_t *rt, int64_t now_ms) {
    if (s->work_mode != SHU1_MODE_AUTO) {
        g_auto_print_context_seen = false;
        snprintf(g_last_auto_printer_state, sizeof(g_last_auto_printer_state), "%s", p->normalized_state);
        return;
    }

    if (printer_data_fresh(p, now_ms) && printer_state_is_printing(p)) {
        g_auto_print_context_seen = true;
    }

    bool last_was_active = str_eq(g_last_auto_printer_state, "printing") || str_eq(g_last_auto_printer_state, "paused") ||
                           str_eq(g_last_auto_printer_state, "error") || str_eq(g_last_auto_printer_state, "timeout");
    bool now_done = printer_data_fresh(p, now_ms) && printer_state_is_complete_or_idle(p);
    if (last_was_active && now_done && g_auto_print_context_seen) {
        if (s->finish_conditioning_mode == SHU1_FINISH_TEMPERING && s->tempering_enabled) {
            start_tempering_if_user_enabled(s, rt, now_ms);
        } else if (s->finish_conditioning_mode == SHU1_FINISH_KEEP_WARM) {
            s->keep_warm_active = true;
            s->keep_warm_end_ms = now_ms + ((int64_t)s->keep_warm_max_min * 60 * 1000);
            s->work_on = true;
            shu1_event_log_add("info", "finish_keep_warm", "AUTO print completed; keep-warm conditioning started by user option");
            shu1_ble_notify_status_now();
        } else {
            // Tempering/conditioning disabled by user/app: AUTO ends cleanly and normal fan post-run/cooldown applies.
            s->work_on = false;
            g_auto_print_context_seen = false;
            shu1_event_log_add("info", "auto_print_complete", "AUTO print completed; heater stopped and fan post-run applies");
        }
    }

    if (s->keep_warm_active && s->keep_warm_end_ms > 0 && now_ms >= s->keep_warm_end_ms) {
        s->keep_warm_active = false;
        s->work_on = false;
        g_auto_print_context_seen = false;
        shu1_event_log_add("info", "keep_warm_complete", "keep-warm conditioning completed");
        shu1_ble_notify_status_now();
    }

    snprintf(g_last_auto_printer_state, sizeof(g_last_auto_printer_state), "%s", p->normalized_state[0] ? p->normalized_state : "idle");
}


static bool virtual_door_context_active(const shu1_settings_t *s, const shu1_printer_state_t *p, int64_t now_ms) {
    // Software-only opening detection is only meaningful after a print or during post-print conditioning.
    // It deliberately does not run during active printing, preheat or drying.
    if (s->tempering_phase == SHU1_TEMPERING_ACTIVE) return true;
    if (s->keep_warm_active) return true;
    if (s->work_mode == SHU1_MODE_AUTO && printer_data_fresh(p, now_ms) && printer_state_is_complete_or_idle(p)) return true;
    return false;
}

static void reset_virtual_door_window(shu1_settings_t *s, const shu1_runtime_t *rt, int64_t now_ms) {
    s->virtual_door_window_start_ms = now_ms;
    s->virtual_door_window_start_temp_c = rt->chamber_temp_c;
    s->virtual_door_last_drop_c = 0.0f;
    s->virtual_door_last_rate_c_per_min = 0.0f;
}

static bool update_virtual_door_detection(shu1_settings_t *s, const shu1_runtime_t *rt, const shu1_printer_state_t *p, int64_t now_ms) {
    if (!s->virtual_door_detection_enabled) {
        s->virtual_door_window_start_ms = 0;
        return false;
    }
    if (!virtual_door_context_active(s, p, now_ms)) {
        s->virtual_door_window_start_ms = 0;
        return false;
    }
    if (rt->chamber_sensor_status != SHU1_SENSOR_OK || !isfinite(rt->chamber_temp_c)) {
        s->virtual_door_window_start_ms = 0;
        return false;
    }

    int window_sec = s->virtual_door_window_sec;
    if (window_sec < 10) window_sec = 10;
    if (window_sec > 300) window_sec = 300;
    s->virtual_door_window_sec = window_sec;

    int drop_c = s->virtual_door_drop_c;
    if (drop_c < 1) drop_c = 1;
    if (drop_c > 30) drop_c = 30;
    s->virtual_door_drop_c = drop_c;

    int rate_cpm = s->virtual_door_rate_c_per_min;
    if (rate_cpm < 1) rate_cpm = 1;
    if (rate_cpm > 60) rate_cpm = 60;
    s->virtual_door_rate_c_per_min = rate_cpm;

    int min_base = s->virtual_door_min_base_temp_c;
    if (min_base < 20) min_base = 20;
    if (min_base > CONFIG_SHU1_MAX_TARGET_TEMP_C) min_base = CONFIG_SHU1_MAX_TARGET_TEMP_C;
    s->virtual_door_min_base_temp_c = min_base;

    if (s->virtual_door_window_start_ms <= 0 || !isfinite(s->virtual_door_window_start_temp_c)) {
        reset_virtual_door_window(s, rt, now_ms);
        return false;
    }

    int64_t elapsed_ms = now_ms - s->virtual_door_window_start_ms;
    if (elapsed_ms < 0 || elapsed_ms > (int64_t)(window_sec * 1000 * 3)) {
        reset_virtual_door_window(s, rt, now_ms);
        return false;
    }
    if (elapsed_ms < (int64_t)window_sec * 1000) return false;

    float start_temp = s->virtual_door_window_start_temp_c;
    float now_temp = rt->chamber_temp_c;
    float drop = start_temp - now_temp;
    float rate = drop * 60000.0f / (float)elapsed_ms;
    if (drop < 0) drop = 0;
    if (rate < 0) rate = 0;
    s->virtual_door_last_drop_c = drop;
    s->virtual_door_last_rate_c_per_min = rate;

    bool enough_temp_to_mean_something = start_temp >= (float)min_base;
    bool probable_open = enough_temp_to_mean_something && drop >= (float)drop_c && rate >= (float)rate_cpm;

    if (!probable_open) {
        // Slide the baseline forward in coarse windows. Real U1 test data will later tune this.
        reset_virtual_door_window(s, rt, now_ms);
        return false;
    }

    s->virtual_door_open = true;
    s->virtual_door_open_pending = true;
    s->virtual_door_detected_ms = now_ms;
    s->door_open = true;
    s->door_open_pending = true;

    if (s->virtual_door_action == SHU1_VDOOR_ACTION_STOP_CONDITIONING || s->virtual_door_action == SHU1_VDOOR_ACTION_STOP_HEATER) {
        s->tempering_phase = SHU1_TEMPERING_IDLE;
        s->tempering_start_ms = 0;
        s->tempering_end_ms = 0;
        s->tempering_current_target_c = 0;
        s->keep_warm_active = false;
        s->work_on = false;
    }

    ESP_LOGW(TAG, "virtual chamber open detected: drop=%.1fC rate=%.1fC/min window=%ds", drop, rate, window_sec);
    shu1_event_log_add("warn", "virtual_chamber_open", "probable chamber/top-cover opening detected from sudden temperature drop");
    shu1_ble_notify_status_now();
    reset_virtual_door_window(s, rt, now_ms);
    return true;
}

static void update_rise_detector(shu1_runtime_t *rt, bool heat_requested_now, int64_t now_ms) {
    if (!heat_requested_now || rt->heater_fault != SHU1_HEATER_OK) {
        rt->rise_detector.active = false;
        return;
    }

    if (!rt->rise_detector.active) {
        rt->rise_detector.active = true;
        rt->rise_detector.started_ms = now_ms;
        rt->rise_detector.check_due_ms = now_ms + SHU1_RISE_DETECT_WINDOW_MS;
        rt->rise_detector.start_ptc_c = rt->ptc_temp_c;
        rt->rise_detector.start_chamber_c = rt->chamber_temp_c;
        ESP_LOGI(TAG, "heater rise detection started: ptc=%.1f chamber=%.1f", rt->ptc_temp_c, rt->chamber_temp_c);
        return;
    }

    if (now_ms - rt->rise_detector.started_ms < SHU1_RISE_DETECT_DELAY_MS) return;
    if (now_ms < rt->rise_detector.check_due_ms) return;

    float ptc_rise = rt->ptc_temp_c - rt->rise_detector.start_ptc_c;
    float chamber_rise = rt->chamber_temp_c - rt->rise_detector.start_chamber_c;
    if (ptc_rise < SHU1_MIN_PTC_RISE_C && chamber_rise < SHU1_MIN_CHAMBER_RISE_C) {
        ESP_LOGE(TAG, "heater abnormal: PTC rise %.1f C, chamber rise %.1f C", ptc_rise, chamber_rise);
        rt->heater_fault = SHU1_HEATER_NO_RISE;
        rt->rise_detector.active = false;
        return;
    }

    ESP_LOGI(TAG, "heater rise normal: PTC rise %.1f C, chamber rise %.1f C", ptc_rise, chamber_rise);
    rt->rise_detector.started_ms = now_ms;
    rt->rise_detector.check_due_ms = now_ms + SHU1_RISE_DETECT_WINDOW_MS;
    rt->rise_detector.start_ptc_c = rt->ptc_temp_c;
    rt->rise_detector.start_chamber_c = rt->chamber_temp_c;
}

static bool update_session_watchdog(shu1_settings_t *s, int64_t now_ms) {
    if (!s->work_on) {
        s->session_started_ms = 0;
        return false;
    }
    if (s->session_started_ms <= 0) {
        s->session_started_ms = now_ms;
        s->session_timeout_pending = false;
        return false;
    }
    if (s->manual_session_max_min <= 0 || s->manual_session_max_min > 720)
        s->manual_session_max_min = 720;
    int64_t max_ms = (int64_t)s->manual_session_max_min * 60 * 1000;
    if (now_ms - s->session_started_ms < max_ms) return false;

    s->work_on = false;
    s->drying_running = false;
    s->drying_end_ms = 0;
    s->preheat_running = false;
    s->preheat_phase = SHU1_PREHEAT_IDLE;
    s->preheat_end_ms = 0;
    s->tempering_phase = SHU1_TEMPERING_IDLE;
    s->tempering_end_ms = 0;
    s->session_timeout_pending = true;
    shu1_event_log_add("warn", "session_timeout", "maximum heater session time reached");
    shu1_ble_notify_status_now();
    return true;
}

static void report_fault_change(shu1_heater_fault_t fault) {
    if (fault == g_last_reported_fault) return;
    g_last_reported_fault = fault;
    if (fault == SHU1_HEATER_OK) {
        shu1_event_log_add("info", "fault_clear", "heater fault cleared");
    } else {
        shu1_event_log_add("warn", shu1_heater_fault_str(fault), "heater fault state changed");
    }
}



static int profile_from_printer_material(const char *m) {
    if (!m || !*m || strcmp(m, "-") == 0) return SHU1_PROFILE_CUSTOM;
    if (strstr(m, "PLA") || strstr(m, "pla")) return SHU1_PROFILE_PLA;
    if (strstr(m, "PETG") || strstr(m, "petg")) return SHU1_PROFILE_PETG;
    if (strstr(m, "ABS") || strstr(m, "abs")) return SHU1_PROFILE_ABS;
    if (strstr(m, "ASA") || strstr(m, "asa")) return SHU1_PROFILE_ASA;
    if (strstr(m, "TPU") || strstr(m, "tpu")) return SHU1_PROFILE_TPU;
    if (strstr(m, "NYLON") || strstr(m, "nylon") || strstr(m, "PA")) return SHU1_PROFILE_NYLON;
    if (strstr(m, "PC") || strstr(m, "pc")) return SHU1_PROFILE_PC;
    return SHU1_PROFILE_CUSTOM;
}

static void update_material_assistant(shu1_settings_t *s, shu1_runtime_t *rt, const shu1_printer_state_t *p) {
    int detected = profile_from_printer_material(p->active_material);
    const char *detected_name = detected == SHU1_PROFILE_CUSTOM ? p->active_material : shu1_profile_name(detected);
    if (!detected_name || !*detected_name || strcmp(detected_name, "-") == 0) detected_name = "unknown";
    const char *user_name = shu1_profile_name(s->material_profile);

    // Optional auto profile is now explicit. By default we DO NOT change the user's selected profile.
    // If disabled, a mismatch only creates a warning for Android/BLE/REST and heating continues.
    if (s->auto_material_profile_enabled && detected != SHU1_PROFILE_CUSTOM && detected != s->material_profile &&
        s->work_mode == SHU1_MODE_AUTO && printer_state_is_printing(p)) {
        shu1_apply_material_profile(s, detected, true);
        s->material_mismatch_pending = false;
        s->material_mismatch_user_profile = SHU1_PROFILE_CUSTOM;
        s->material_mismatch_printer_profile = SHU1_PROFILE_CUSTOM;
        snprintf(rt->material_mismatch_message, sizeof(rt->material_mismatch_message), "Auto profile applied from U1: %s", detected_name);
        shu1_event_log_add("info", "auto_material_profile", "material profile applied from Snapmaker U1 filament_type");
    } else if (s->material_mismatch_warning_enabled && detected != SHU1_PROFILE_CUSTOM &&
               s->material_profile != SHU1_PROFILE_CUSTOM && detected != s->material_profile &&
               printer_state_is_printing(p)) {
        if (!s->material_mismatch_pending ||
            s->material_mismatch_user_profile != s->material_profile ||
            s->material_mismatch_printer_profile != detected) {
            s->material_mismatch_pending = true;
            s->material_mismatch_user_profile = s->material_profile;
            s->material_mismatch_printer_profile = detected;
            s->material_mismatch_detected_ms = esp_timer_get_time() / 1000;
            snprintf(rt->material_mismatch_message, sizeof(rt->material_mismatch_message),
                     "Profile mismatch: SnapHeater=%s, U1 active material=%s. Heating continues.",
                     user_name, detected_name);
            shu1_event_log_add("warn", "material_profile_mismatch", "U1 material differs from selected SnapHeater profile; heating continues");
            shu1_ble_notify_status_now();
        }
    } else if (detected == s->material_profile || detected == SHU1_PROFILE_CUSTOM || s->material_profile == SHU1_PROFILE_CUSTOM) {
        if (s->material_mismatch_pending && (detected == s->material_profile || detected == SHU1_PROFILE_CUSTOM)) {
            s->material_mismatch_pending = false;
            s->material_mismatch_user_profile = SHU1_PROFILE_CUSTOM;
            s->material_mismatch_printer_profile = SHU1_PROFILE_CUSTOM;
            snprintf(rt->material_mismatch_message, sizeof(rt->material_mismatch_message), "No material/profile mismatch");
            shu1_event_log_add("info", "material_profile_mismatch_clear", "material/profile mismatch cleared");
            shu1_ble_notify_status_now();
        }
    }

    if (s->material_mismatch_pending) {
        snprintf(rt->material_advice, sizeof(rt->material_advice),
                 "Warning: selected profile %s differs from U1 material %s. Heating continues; verify profile in app.",
                 user_name, detected_name);
    } else if (detected == SHU1_PROFILE_PLA) {
        snprintf(rt->material_advice, sizeof(rt->material_advice), "PLA detected: use conservative chamber temperature; avoid overheating soft PLA parts.");
    } else if (detected == SHU1_PROFILE_ABS || detected == SHU1_PROFILE_ASA) {
        snprintf(rt->material_advice, sizeof(rt->material_advice), "%s detected: anti-warp, preheat and optional tempering are recommended.", shu1_profile_name(detected));
    } else if (detected == SHU1_PROFILE_PETG) {
        snprintf(rt->material_advice, sizeof(rt->material_advice), "PETG detected: moderate chamber heat and stable airflow are recommended.");
    } else if (detected == SHU1_PROFILE_NYLON || detected == SHU1_PROFILE_PC) {
        snprintf(rt->material_advice, sizeof(rt->material_advice), "%s detected: use dry filament, preheat and strict temperature limits.", shu1_profile_name(detected));
    } else {
        snprintf(rt->material_advice, sizeof(rt->material_advice), "Material unknown: use conservative chamber settings and monitor first print.");
    }
}

static void update_scheduled_preheat(shu1_settings_t *s, int64_t now_ms) {
    if (!s->scheduled_preheat_enabled) return;
    if (!shu1_control_schedule_allowed()) { shu1_settings_stop(s); return; }
    if (s->scheduled_preheat_start_ms <= 0) {
        if (s->scheduled_preheat_delay_min < 0) s->scheduled_preheat_delay_min = 0;
        if (s->scheduled_preheat_delay_min > 1440) s->scheduled_preheat_delay_min = 1440;
        s->scheduled_preheat_start_ms = now_ms + ((int64_t)s->scheduled_preheat_delay_min * 60 * 1000);
        shu1_event_log_add("info", "preheat_scheduled", "preheat scheduled by Android/BLE/REST");
        return;
    }
    if (now_ms < s->scheduled_preheat_start_ms) return;

    s->scheduled_preheat_enabled = false;
    s->scheduled_preheat_started_pending = true;
    s->preheat_running = true;
    s->preheat_phase = SHU1_PREHEAT_HEATING;
    s->preheat_target_temp_c = clamp_target(s->scheduled_preheat_target_c);
    s->preheat_hold_min = s->scheduled_preheat_hold_min < 1 ? 1 : s->scheduled_preheat_hold_min;
    s->work_on = true;
    s->work_mode = SHU1_MODE_PREHEAT;
    shu1_event_log_add("info", "scheduled_preheat_started", "scheduled preheat started");
    shu1_ble_notify_status_now();
}

static bool dryout_update(shu1_settings_t *s, int64_t now_ms) {
    if (s->work_mode != SHU1_MODE_DRY_OUT || !s->dryout_running) return false;
    if (s->dryout_end_ms <= 0) {
        if (s->dryout_duration_min < 1) s->dryout_duration_min = 1;
        if (s->dryout_duration_min > 240) s->dryout_duration_min = 240;
        s->dryout_end_ms = now_ms + ((int64_t)s->dryout_duration_min * 60 * 1000);
        s->work_on = true;
        shu1_event_log_add("info", "dryout_started", "chamber dry-out cycle started");
    }
    if (now_ms < s->dryout_end_ms) return false;
    s->dryout_running = false;
    s->dryout_end_ms = 0;
    s->work_on = false;
    s->dryout_complete_pending = true;
    shu1_event_log_add("info", "dryout_complete", "chamber dry-out cycle completed");
    shu1_ble_notify_status_now();
    return true;
}

static bool health_test_update(shu1_settings_t *s, shu1_runtime_t *rt, int64_t now_ms) {
    if (s->work_mode != SHU1_MODE_HEALTH_TEST || !s->health_test_running) return false;
    if (s->health_test_phase == SHU1_HEALTH_IDLE) {
        s->health_test_phase = SHU1_HEALTH_PRE_FAN;
        s->health_test_start_ms = now_ms;
        s->health_test_start_ptc_c = rt->ptc_temp_c;
        s->health_test_start_chamber_c = rt->chamber_temp_c;
        s->health_test_result = SHU1_HEALTH_RESULT_NONE;
        shu1_event_log_add("info", "health_test_started", "heater health test started");
    }
    if (rt->chamber_sensor_status != SHU1_SENSOR_OK || rt->ptc_sensor_status != SHU1_SENSOR_OK) {
        s->health_test_phase = SHU1_HEALTH_FAILED;
        s->health_test_running = false;
        s->health_test_result = SHU1_HEALTH_RESULT_SENSOR_FAULT;
        s->health_test_complete_pending = true;
        shu1_event_log_add("warn", "health_test_sensor_fault", "heater health test failed due to sensor fault");
        shu1_ble_notify_status_now();
        return true;
    }
    int64_t elapsed = now_ms - s->health_test_start_ms;
    if (s->health_test_phase == SHU1_HEALTH_PRE_FAN && elapsed > 5000) {
        s->health_test_phase = SHU1_HEALTH_HEATING;
        shu1_event_log_add("info", "health_test_heating", "heater health test heating window started");
    }
    int dur_sec = s->health_test_duration_sec;
    if (dur_sec < 30) dur_sec = 30;
    if (dur_sec > 300) dur_sec = 300;
    s->health_test_duration_sec = dur_sec;
    if (elapsed < ((int64_t)dur_sec * 1000)) return false;

    float ptc_rise = rt->ptc_temp_c - s->health_test_start_ptc_c;
    float chamber_rise = rt->chamber_temp_c - s->health_test_start_chamber_c;
    rt->last_health_ptc_rise_c = ptc_rise;
    rt->last_health_chamber_rise_c = chamber_rise;
    if (rt->heater_health_baseline_ptc_rise_c <= 0.1f && ptc_rise > 0.1f) {
        rt->heater_health_baseline_ptc_rise_c = ptc_rise;
    }
    bool ok = ptc_rise >= SHU1_HEALTH_MIN_PTC_RISE_C || chamber_rise >= SHU1_HEALTH_MIN_CHAMBER_RISE_C;
    s->health_test_phase = ok ? SHU1_HEALTH_COMPLETE : SHU1_HEALTH_FAILED;
    s->health_test_result = ok ? SHU1_HEALTH_RESULT_OK : SHU1_HEALTH_RESULT_WEAK_HEATER;
    s->health_test_running = false;
    s->work_on = false;
    s->health_test_complete_pending = true;
    shu1_event_log_add(ok ? "info" : "warn", ok ? "health_test_ok" : "health_test_weak", ok ? "heater health test passed" : "heater health test did not detect enough temperature rise");
    shu1_ble_notify_status_now();
    return true;
}

static void update_pause_policy(shu1_settings_t *s, const shu1_printer_state_t *p, int64_t now_ms) {
    if (s->work_mode != SHU1_MODE_AUTO || !s->pause_hold_enabled || !g_auto_print_context_seen) {
        s->pause_seen_start_ms = 0;
        return;
    }
    if (!printer_state_is_pause_or_error(p)) {
        s->pause_seen_start_ms = 0;
        return;
    }
    if (s->pause_seen_start_ms <= 0) {
        s->pause_seen_start_ms = now_ms;
        shu1_event_log_add("warn", "printer_pause_hold", "printer pause/error detected; chamber hold policy active");
    }
    int64_t paused_min = (now_ms - s->pause_seen_start_ms) / 60000;
    if (s->pause_hold_strategy == SHU1_PAUSE_HOLD_LOWER && paused_min >= s->pause_lower_after_min && s->pause_lower_by_c > 0) {
        int lowered = s->target_temp_c - s->pause_lower_by_c;
        if (lowered < 0) lowered = 0;
        s->target_temp_c = lowered;
        s->pause_lower_by_c = 0; // one-time lowering for this pause episode
        shu1_event_log_add("warn", "pause_hold_lowered", "pause hold target lowered after user-selected delay");
    } else if (s->pause_hold_strategy == SHU1_PAUSE_HOLD_STOP_AFTER && paused_min >= s->pause_stop_after_min) {
        s->work_on = false;
        shu1_event_log_add("warn", "pause_hold_timeout", "pause hold maximum time reached; heater stopped by user policy");
        shu1_ble_notify_status_now();
    }
}

static void reset_stability_stats(shu1_runtime_t *rt, int target, int band) {
    rt->stability_active = true;
    rt->stability_target_c = target;
    rt->stability_band_c = band;
    rt->stability_samples = 0;
    rt->stability_within_band_samples = 0;
    rt->stability_min_c = NAN;
    rt->stability_max_c = NAN;
    rt->stability_sum_c = 0.0f;
    rt->stability_score_pct = 0;
    rt->stability_report_pending = false;
}

static void update_energy_and_stability(shu1_runtime_t *rt, bool request_heat, bool request_fan, int target, int64_t now_ms) {
    static int64_t last_ms = 0;
    static bool was_printing_context = false;
    if (last_ms <= 0) last_ms = now_ms;
    int64_t dt = now_ms - last_ms;
    if (dt < 0 || dt > 5000) dt = SHU1_CONTROL_PERIOD_MS;
    last_ms = now_ms;

    if (request_heat) {
        rt->heater_on_accum_ms += (uint64_t)dt;
        rt->session_heater_on_ms += (uint64_t)dt;
    }
    if (request_fan) {
        rt->fan_on_accum_ms += (uint64_t)dt;
    }
    rt->estimated_energy_wh = ((float)rt->heater_on_accum_ms / 3600000.0f) * (float)SHU1_ENERGY_HEATER_WATT;
    rt->session_energy_wh = ((float)rt->session_heater_on_ms / 3600000.0f) * (float)SHU1_ENERGY_HEATER_WATT;

    bool active_print = g_auto_print_context_seen;
    if (active_print && !was_printing_context) {
        reset_stability_stats(rt, target, SHU1_DEFAULT_STABILITY_BAND_C);
        rt->session_heater_on_ms = 0;
    }
    if (!active_print && was_printing_context && rt->stability_active) {
        rt->stability_active = false;
        rt->stability_report_pending = true;
        shu1_event_log_add("info", "stability_report_ready", "print chamber stability report ready");
        shu1_ble_notify_status_now();
    }
    was_printing_context = active_print;

    if (rt->stability_active && isfinite(rt->chamber_temp_c) && target > 0) {
        rt->stability_samples++;
        if (!isfinite(rt->stability_min_c) || rt->chamber_temp_c < rt->stability_min_c) rt->stability_min_c = rt->chamber_temp_c;
        if (!isfinite(rt->stability_max_c) || rt->chamber_temp_c > rt->stability_max_c) rt->stability_max_c = rt->chamber_temp_c;
        rt->stability_sum_c += rt->chamber_temp_c;
        if (fabsf(rt->chamber_temp_c - (float)target) <= (float)rt->stability_band_c) rt->stability_within_band_samples++;
        if (rt->stability_samples > 0) rt->stability_score_pct = (int)((rt->stability_within_band_samples * 100) / rt->stability_samples);
    }
}


static void update_heat_soak(shu1_settings_t *st, shu1_runtime_t *rt, int target, int64_t now_ms) {
    if (!st->heat_soak_enabled || target <= 0 || !isfinite(rt->chamber_temp_c)) {
        st->heat_soak_phase = SHU1_HEAT_SOAK_IDLE;
        st->heat_soak_start_ms = 0;
        st->heat_soak_end_ms = 0;
        rt->heat_soak_ready = false;
        rt->heat_soak_remaining_sec = 0;
        return;
    }
    int band = st->heat_soak_band_c;
    if (band < 1) band = 1;
    if (band > 10) band = 10;
    st->heat_soak_band_c = band;
    int soak_min = st->heat_soak_min;
    if (soak_min < 0) soak_min = 0;
    if (soak_min > 240) soak_min = 240;
    st->heat_soak_min = soak_min;

    bool stable_enough = fabsf(rt->chamber_temp_c - (float)target) <= (float)band;
    if (!stable_enough) {
        st->heat_soak_phase = SHU1_HEAT_SOAK_WAITING;
        st->heat_soak_start_ms = 0;
        st->heat_soak_end_ms = 0;
        rt->heat_soak_ready = false;
        rt->heat_soak_remaining_sec = 0;
        return;
    }
    if (st->heat_soak_phase != SHU1_HEAT_SOAK_HOLDING && st->heat_soak_phase != SHU1_HEAT_SOAK_COMPLETE) {
        st->heat_soak_phase = SHU1_HEAT_SOAK_HOLDING;
        st->heat_soak_start_ms = now_ms;
        st->heat_soak_end_ms = now_ms + ((int64_t)soak_min * 60 * 1000);
        st->heat_soak_complete_pending = false;
        shu1_event_log_add("info", "heat_soak_started", "target reached; heat-soak timer started");
    }
    if (st->heat_soak_phase == SHU1_HEAT_SOAK_HOLDING) {
        if (now_ms >= st->heat_soak_end_ms) {
            st->heat_soak_phase = SHU1_HEAT_SOAK_COMPLETE;
            st->heat_soak_complete_pending = true;
            rt->heat_soak_ready = true;
            rt->heat_soak_remaining_sec = 0;
            shu1_event_log_add("info", "heat_soak_complete", "chamber heat-soak completed");
            shu1_ble_notify_status_now();
        } else {
            rt->heat_soak_remaining_sec = (int)((st->heat_soak_end_ms - now_ms) / 1000);
            rt->heat_soak_ready = false;
        }
    } else if (st->heat_soak_phase == SHU1_HEAT_SOAK_COMPLETE) {
        rt->heat_soak_ready = true;
        rt->heat_soak_remaining_sec = 0;
    }
}

static void update_warmup_prediction(shu1_settings_t *st, shu1_runtime_t *rt, bool request_heat, int target, int64_t now_ms) {
    if (!st->warmup_prediction_enabled || !request_heat || target <= 0 || !isfinite(rt->chamber_temp_c)) {
        rt->warmup_eta_sec = -1;
        return;
    }
    if (g_warm_prev_ms > 0 && now_ms > g_warm_prev_ms && isfinite(g_warm_prev_temp)) {
        float dt_min = (float)(now_ms - g_warm_prev_ms) / 60000.0f;
        float rate = (rt->chamber_temp_c - g_warm_prev_temp) / dt_min;
        if (rate > 0.05f && rate < 30.0f) {
            g_warm_rate_ema = g_warm_rate_ema <= 0.0f ? rate : (0.75f * g_warm_rate_ema + 0.25f * rate);
        }
    }
    g_warm_prev_ms = now_ms;
    g_warm_prev_temp = rt->chamber_temp_c;
    rt->warmup_rate_c_per_min = g_warm_rate_ema;
    if (g_warm_rate_ema > 0.05f && rt->chamber_temp_c < (float)target) {
        rt->warmup_eta_sec = (int)(((float)target - rt->chamber_temp_c) / g_warm_rate_ema * 60.0f);
    } else {
        rt->warmup_eta_sec = 0;
    }
}

static void update_filter_and_wear(shu1_settings_t *st, shu1_runtime_t *rt) {
    if (st->filter_life_counter_enabled && st->filter_life_limit_h > 0) {
        uint64_t limit_ms = (uint64_t)st->filter_life_limit_h * 3600000ULL;
        rt->filter_life_pct = (int)((rt->fan_on_accum_ms * 100ULL) / limit_ms);
        if (rt->filter_life_pct >= 100 && !st->filter_life_warning_pending) {
            st->filter_life_warning_pending = true;
            shu1_event_log_add("warn", "filter_life_warning", "filter runtime limit reached; replacement recommended");
            shu1_ble_notify_status_now();
        }
    }
    if (st->heater_wear_tracking_enabled && rt->heater_health_baseline_ptc_rise_c > 0.1f && rt->last_health_ptc_rise_c > 0.1f) {
        float ratio = rt->last_health_ptc_rise_c / rt->heater_health_baseline_ptc_rise_c;
        int drop_pct = (int)((1.0f - ratio) * 100.0f);
        if (drop_pct < 0) drop_pct = 0;
        rt->heater_wear_pct = drop_pct;
        if (drop_pct >= st->heater_wear_warning_pct && !st->heater_wear_warning_pending) {
            st->heater_wear_warning_pending = true;
            shu1_event_log_add("warn", "heater_wear_warning", "heater warm-up performance dropped versus baseline");
            shu1_ble_notify_status_now();
        }
    }
}

static void update_airflow_detection(shu1_settings_t *st, shu1_runtime_t *rt, bool request_heat, int64_t now_ms) {
    if (!st->airflow_detection_enabled || !request_heat || !isfinite(rt->ptc_temp_c) || !isfinite(rt->chamber_temp_c)) {
        g_airflow_start_ms = 0;
        return;
    }
    if (g_airflow_start_ms <= 0) {
        g_airflow_start_ms = now_ms;
        g_airflow_start_chamber = rt->chamber_temp_c;
        g_airflow_start_ptc = rt->ptc_temp_c;
        return;
    }
    int64_t elapsed = now_ms - g_airflow_start_ms;
    if (elapsed < SHU1_AIRFLOW_WINDOW_MS) return;
    float ptc_delta = rt->ptc_temp_c - rt->chamber_temp_c;
    float chamber_rise = rt->chamber_temp_c - g_airflow_start_chamber;
    if (ptc_delta >= SHU1_AIRFLOW_PTC_DELTA_C && chamber_rise < SHU1_AIRFLOW_MIN_CHAMBER_RISE_C) {
        st->airflow_warning_pending = true;
        rt->airflow_warning_pending = true;
        rt->airflow_score_pct = 35;
        shu1_event_log_add("warn", "weak_airflow_detected", "PTC is hot but chamber warms slowly; check fan/filter/air path");
        shu1_ble_notify_status_now();
    } else {
        rt->airflow_score_pct = 100;
    }
    g_airflow_start_ms = now_ms;
    g_airflow_start_chamber = rt->chamber_temp_c;
    g_airflow_start_ptc = rt->ptc_temp_c;
}

static void update_print_risk_and_start_warnings(shu1_settings_t *st, shu1_runtime_t *rt, const shu1_printer_state_t *pr, int target, int64_t now_ms) {
    bool printing = printer_state_is_printing(pr);
    int detected = profile_from_printer_material(pr->active_material);
    int risk = 0;
    if (detected == SHU1_PROFILE_ABS || detected == SHU1_PROFILE_ASA) {
        if (target < 50) risk += 40;
        if (!st->heat_soak_enabled || !rt->heat_soak_ready) risk += 20;
        if (!st->anti_warp_enabled) risk += 15;
        snprintf(rt->print_risk_message, sizeof(rt->print_risk_message), "%s: use high chamber temp, heat soak, anti-warp and optional tempering", shu1_profile_name(detected));
    } else if (detected == SHU1_PROFILE_PLA) {
        if (target > SHU1_PLA_PROTECTION_MAX_C) risk += 75;
        snprintf(rt->print_risk_message, sizeof(rt->print_risk_message), "PLA: high chamber temperature can soften PLA; keep chamber conservative");
    } else if (detected == SHU1_PROFILE_PETG) {
        if (target > 50) risk += 35;
        snprintf(rt->print_risk_message, sizeof(rt->print_risk_message), "PETG: moderate chamber heating recommended");
    } else {
        if (target > 55) risk += 25;
        snprintf(rt->print_risk_message, sizeof(rt->print_risk_message), "Material unknown: use conservative chamber settings");
    }
    if (st->large_print_protection_enabled) risk -= 10;
    if (risk < 0) risk = 0;
    if (risk > 100) risk = 100;
    st->print_risk_score = risk;
    rt->print_risk_score = risk;
    if (st->print_risk_enabled && risk >= SHU1_RISK_HIGH && !st->print_risk_warning_pending) {
        st->print_risk_warning_pending = true;
        rt->print_risk_warning_pending = true;
        shu1_event_log_add("warn", "print_risk_high", "high chamber/material risk detected; Android should warn user");
        shu1_ble_notify_status_now();
    }

    if (st->pla_protection_enabled && detected == SHU1_PROFILE_PLA && target > SHU1_PLA_PROTECTION_MAX_C && !st->pla_protection_confirmed) {
        st->pla_protection_pending = true;
        shu1_event_log_add("warn", "pla_protection_warning", "PLA detected with high chamber target; heating continues but Android should warn user");
        shu1_ble_notify_status_now();
    }

    if (st->start_print_warning_enabled && printing && !g_last_printing_for_start_warning) {
        bool not_ready = target > 0 && isfinite(rt->chamber_temp_c) && rt->chamber_temp_c < (float)target - 3.0f;
        if (not_ready || (st->heat_soak_enabled && !rt->heat_soak_ready)) {
            st->start_print_warning_pending = true;
            rt->start_print_warning_pending = true;
            shu1_event_log_add("warn", "print_started_before_ready", "U1 started printing before chamber/heat-soak was ready");
            shu1_ble_notify_status_now();
        }
    }
    g_last_printing_for_start_warning = printing;

    bool paused = printer_state_is_pause_or_error(pr);
    if (st->smart_resume_enabled && g_last_pause_for_resume && printing && !paused) {
        st->resume_recover_active = true;
        st->resume_recover_end_ms = now_ms + ((int64_t)st->resume_recover_min * 60 * 1000);
        shu1_event_log_add("info", "smart_resume_recover", "print resumed; short chamber recovery window active");
    }
    if (st->resume_recover_active && now_ms >= st->resume_recover_end_ms) {
        st->resume_recover_active = false;
        st->resume_recover_end_ms = 0;
        shu1_event_log_add("info", "smart_resume_recover_complete", "resume recovery window completed");
    }
    g_last_pause_for_resume = paused;
}

static void update_safety_score(shu1_settings_t *st, shu1_runtime_t *rt, const shu1_printer_state_t *pr) {
    if (!st->safety_score_enabled) return;
    int score = 0;
    if (rt->chamber_sensor_status == SHU1_SENSOR_OK) score += 20;
    if (rt->ptc_sensor_status == SHU1_SENSOR_OK) score += 20;
    if (pr->moonraker_connected && pr->klippy_ready) score += 15;
    if (CONFIG_SHU1_ENABLE_HEATER_OUTPUT) score += 10; else score += 5; // build is safe but not live-ready
    if (st->filter_life_counter_enabled) score += 10;
    if (st->airflow_detection_enabled) score += 10;
    if (st->virtual_door_detection_enabled || st->door_sensor_enabled) score += 10;
    if (st->safe_overnight_enabled || st->manual_session_max_min > 0) score += 5;
    if (score > 100) score = 100;
    st->safety_score = score;
    rt->safety_score = score;
    st->setup_validation_passed = score >= SHU1_SAFETY_SCORE_MIN_TO_UNLOCK;
    rt->setup_validation_passed = st->setup_validation_passed;
    snprintf(rt->safety_message, sizeof(rt->safety_message), "Safety score %d/100: sensors=%s/%s moonraker=%s heater_build=%s", score,
             shu1_sensor_status_str(rt->chamber_sensor_status), shu1_sensor_status_str(rt->ptc_sensor_status),
             pr->moonraker_connected ? "ok" : "missing", CONFIG_SHU1_ENABLE_HEATER_OUTPUT ? "live" : "locked");
    if (!st->setup_validation_passed && !st->setup_warning_pending) {
        st->setup_warning_pending = true;
        shu1_event_log_add("warn", "setup_validation_incomplete", "safety score below recommended unlock threshold");
    }
}



static void update_v14_history(shu1_settings_t *st, shu1_runtime_t *rt, bool request_heat, bool request_fan, int target, int64_t now_ms) {
    if (!st->temp_history_enabled) return;
    int period = st->history_sample_period_sec;
    if (period < 5) period = 5;
    if (period > 600) period = 600;
    if (rt->history_last_sample_ms > 0 && now_ms - rt->history_last_sample_ms < (int64_t)period * 1000) return;
    int i = rt->history_head;
    if (i < 0 || i >= SHU1_HISTORY_SLOTS) i = 0;
    rt->history_chamber_c[i] = rt->chamber_temp_c;
    rt->history_ptc_c[i] = rt->ptc_temp_c;
    rt->history_target_c[i] = (float)target;
    rt->history_heater_on[i] = request_heat ? 1 : 0;
    rt->history_fan_on[i] = request_fan ? 1 : 0;
    rt->history_head = (i + 1) % SHU1_HISTORY_SLOTS;
    if (rt->history_count < SHU1_HISTORY_SLOTS) rt->history_count++;
    rt->history_last_sample_ms = now_ms;
}

static void update_zero_cross_runtime(shu1_runtime_t *rt, int64_t now_ms) {
    shu1_zero_cross_stats_t zc = shu1_fan_triac_zero_cross_stats();
    rt->zero_cross_edges = zc.edge_count;
    rt->zero_cross_rejected_edges = zc.rejected_edge_count;
    rt->zero_cross_last_period_us = zc.last_period_us;
    rt->zero_cross_min_period_us = zc.min_period_us;
    rt->zero_cross_max_period_us = zc.max_period_us;
    rt->zero_cross_last_edge_ms = zc.last_edge_ms;
    rt->zero_cross_signal_present = zc.signal_present;

    if (g_last_zc_sample_ms <= 0) {
        g_last_zc_edges = zc.edge_count;
        g_last_zc_sample_ms = now_ms;
        rt->zero_cross_edges_per_sec = 0;
        return;
    }

    int64_t elapsed_ms = now_ms - g_last_zc_sample_ms;
    if (elapsed_ms >= 1000) {
        uint64_t delta = zc.edge_count >= g_last_zc_edges ? zc.edge_count - g_last_zc_edges : 0;
        rt->zero_cross_edges_per_sec = (uint32_t)((delta * 1000ULL) / (uint64_t)elapsed_ms);
        g_last_zc_edges = zc.edge_count;
        g_last_zc_sample_ms = now_ms;
    }
}

static void update_v14_setup_and_latch(shu1_settings_t *st, shu1_runtime_t *rt, const shu1_printer_state_t *pr) {
    bool sensors_ready = rt->chamber_sensor_status == SHU1_SENSOR_OK && rt->ptc_sensor_status == SHU1_SENSOR_OK;
    bool heater_map_ready = CONFIG_SHU1_ENABLE_HEATER_OUTPUT && CONFIG_SHU1_HEATER_GPIO == 18;
    bool fan_map_ready = CONFIG_SHU1_ENABLE_FAN_TRIAC_CONTROL &&
        CONFIG_SHU1_FAN_GPIO == 3 &&
        CONFIG_SHU1_ZERO_CROSS_GPIO == 7 &&
        rt->zero_cross_signal_present;
    bool runtime_gate_ready = sensors_ready && heater_map_ready && fan_map_ready &&
        st->sensors_verified && st->heater_output_verified && st->fan_output_verified;

    // Live plausibility and a matching build never constitute physical verification.
    // These three flags must be recorded explicitly after supervised continuity,
    // sensor and airflow checks; the heater pin remains inferred upstream.
    st->moonraker_verified = (pr->moonraker_connected && pr->klippy_ready) || st->moonraker_verified;
    if (st->first_setup_wizard_enabled && !st->first_setup_complete) {
        int step = SHU1_SETUP_STEP_BLE_CONNECTED;
        if (st->local_only_mode || st->moonraker_verified) step = SHU1_SETUP_STEP_MOONRAKER_READY;
        if (st->sensors_verified) step = SHU1_SETUP_STEP_SENSORS_OK;
        if (st->fan_output_verified) step = SHU1_SETUP_STEP_FAN_VERIFIED;
        if (st->heater_output_verified) step = SHU1_SETUP_STEP_HEATER_VERIFIED;
        if (runtime_gate_ready && (st->local_only_mode || st->moonraker_verified)) {
            step = SHU1_SETUP_STEP_COMPLETE;
            st->first_setup_complete = true;
            st->setup_warning_pending = false;
        }
        st->first_setup_step = step;
    }
    // Readiness never arms the latch automatically. A boot, reconnect or recovered
    // zero-cross signal must still be followed by an explicit user/app arm action.
    rt->output_safety_latch_ready = !st->output_safety_latch_enabled ||
                                    (st->output_safety_latch_armed && runtime_gate_ready);
    if (st->output_safety_latch_enabled && !rt->output_safety_latch_ready && st->work_on) {
        rt->notification_level = SHU1_NOTIFY_ACTION;
        snprintf(rt->notification_code, sizeof(rt->notification_code), "%s", "output_latch_not_ready");
        snprintf(rt->notification_message, sizeof(rt->notification_message), "%s", "Runtime safety gate is not ready; check sensors, AC zero-cross and hardware map");
        st->setup_warning_pending = true;
    }
}

static void update_v14_incident_report(shu1_settings_t *st, shu1_runtime_t *rt, const shu1_printer_state_t *pr, int64_t now_ms) {
    if (!st->incident_report_enabled) return;
    bool critical = (rt->heater_fault == SHU1_HEATER_SENSOR_FAULT || rt->heater_fault == SHU1_HEATER_OVERTEMP || rt->heater_fault == SHU1_HEATER_NO_RISE || rt->heater_fault == SHU1_HEATER_DOOR_OPEN || rt->heater_fault == SHU1_HEATER_ZERO_CROSS_LOST);
    if (critical && !st->incident_report_pending) {
        st->incident_report_pending = true;
        st->incident_report_seq++;
        snprintf(st->incident_last_reason, sizeof(st->incident_last_reason), "%s", shu1_heater_fault_str(rt->heater_fault));
        shu1_event_log_add("warn", "incident_snapshot", "fault snapshot captured for Android export");
    }
    if (st->incident_report_pending) {
        rt->incident_report_seq = st->incident_report_seq;
        snprintf(rt->incident_summary, sizeof(rt->incident_summary),
                 "incident=%d reason=%s mode=%d chamber=%.1f ptc=%.1f heater=%d fan=%d printer=%s moonraker=%d t=%lld",
                 st->incident_report_seq, st->incident_last_reason, st->work_mode, rt->chamber_temp_c, rt->ptc_temp_c,
                 rt->heater_output_on ? 1 : 0, rt->fan_output_on ? 1 : 0, pr->normalized_state, pr->moonraker_connected ? 1 : 0, (long long)now_ms);
    }
}

static void update_v14_symbiont(shu1_settings_t *st, shu1_runtime_t *rt, const shu1_printer_state_t *pr) {
    if (!st->symbiont_mode_enabled) {
        snprintf(rt->symbiont_status, sizeof(rt->symbiont_status), "%s", "U1 Symbiont Mode disabled; read-only Moonraker observation active");
        return;
    }
    if (st->symbiont_policy == SHU1_SYMBIONT_POLICY_READ_ONLY || !st->symbiont_ventilation_allowed) {
        snprintf(rt->symbiont_status, sizeof(rt->symbiont_status), "%s", "U1 Symbiont Mode active in read-only cooperation; no printer writes allowed");
        return;
    }
    snprintf(rt->symbiont_status, sizeof(rt->symbiont_status),
             "U1 Symbiont Mode climate-safe policy armed=%d moonraker=%d; future writes limited to whitelisted ventilation objects only",
             st->symbiont_safe_control_enabled ? 1 : 0, pr->moonraker_connected ? 1 : 0);
    st->symbiont_notification_pending = true;
}

static void update_v14_notifications(shu1_settings_t *st, shu1_runtime_t *rt, int64_t now_ms) {
    if (st->incident_report_pending) {
        rt->notification_level = SHU1_NOTIFY_CRITICAL;
        snprintf(rt->notification_code, sizeof(rt->notification_code), "%s", "incident_report_pending");
        snprintf(rt->notification_message, sizeof(rt->notification_message), "%s", "Incident report is ready for export");
    } else if (st->preheat_complete_pending || st->tempering_complete_pending || st->dryout_complete_pending || st->heat_soak_complete_pending) {
        rt->notification_level = SHU1_NOTIFY_INFO;
        snprintf(rt->notification_code, sizeof(rt->notification_code), "%s", "cycle_complete");
        snprintf(rt->notification_message, sizeof(rt->notification_message), "%s", "A chamber cycle completed");
    } else if (st->material_mismatch_pending || st->pla_protection_pending || st->setup_warning_pending || st->symbiont_notification_pending) {
        rt->notification_level = SHU1_NOTIFY_WARNING;
        snprintf(rt->notification_code, sizeof(rt->notification_code), "%s", "warning_pending");
        snprintf(rt->notification_message, sizeof(rt->notification_message), "%s", "One or more warnings require review in Android app");
    } else {
        rt->notification_level = SHU1_NOTIFY_INFO;
        snprintf(rt->notification_code, sizeof(rt->notification_code), "%s", "ok");
        snprintf(rt->notification_message, sizeof(rt->notification_message), "%s", "No active notifications");
    }
    rt->notification_ms = now_ms;
}

static void update_v14_productization(shu1_settings_t *st, shu1_runtime_t *rt, shu1_printer_state_t *pr, bool request_heat, bool request_fan, int target, int64_t now_ms) {
    update_v14_history(st, rt, request_heat, request_fan, target, now_ms);
    update_v14_setup_and_latch(st, rt, pr);
    update_v14_incident_report(st, rt, pr, now_ms);
    update_v14_symbiont(st, rt, pr);
    update_v14_notifications(st, rt, now_ms);
}

static void update_v13_extended_features(shu1_settings_t *st, shu1_runtime_t *rt, shu1_printer_state_t *pr, bool request_heat, bool request_fan, int target, int64_t now_ms) {
    update_warmup_prediction(st, rt, request_heat, target, now_ms);
    update_heat_soak(st, rt, target, now_ms);
    update_filter_and_wear(st, rt);
    update_airflow_detection(st, rt, request_heat, now_ms);
    update_print_risk_and_start_warnings(st, rt, pr, target, now_ms);
    update_safety_score(st, rt, pr);
}

static void control_task(void *arg) {
    (void)arg;
    // DragonBreath uses a per-boot inhibit here: WDT setup failure must keep the
    // heater off, but it is not written as a reboot-surviving hardware fault.
    const bool wdt_armed = esp_task_wdt_add(NULL) == ESP_OK;
    if (!wdt_armed) {
        ESP_LOGE(TAG, "task watchdog unavailable; heater inhibited for this boot");
        shu1_heater_force_off();
        shu1_safety_latch_inhibit();
    }
    // A reboot may follow heating: require positive evidence of a cold device.
    bool heated_this_session = true;
    bool zc_seen_while_armed = false;
    uint64_t zc_gap_count = shu1_fan_triac_zero_cross_stats().signal_gap_count;
    bool thermal_purge = true;
    while (true) {
        SHU1_CONTROL_GUARD(policy_guard);
        int64_t now_ms = esp_timer_get_time() / 1000;
        shu1_sensor_sample_t sample = {0};
        shu1_runtime_t rt = shu1_state_get_runtime();
        shu1_settings_t st = shu1_state_get_settings();
        const uint32_t command_epoch = shu1_state_command_epoch();
        shu1_printer_state_t pr = shu1_state_get_printer();
        shu1_control_snapshot_t ctl_snapshot;
        shu1_control_snapshot(&ctl_snapshot);
        const bool remote_lease_expired = (st.work_on || st.scheduled_preheat_enabled) && ctl_snapshot.lease_active &&
                                          shu1_control_lease_expired();
        if (remote_lease_expired) {
            shu1_settings_stop(&st);
            shu1_control_release_any();
        }
        (void)shu1_safety_latch_retry_persist();

        if (shu1_ntc_read(&sample) == ESP_OK) {
            rt.chamber_temp_c = sample.chamber_c;
            rt.ptc_temp_c = sample.ptc_c;
            rt.chamber_instant_temp_c = sample.chamber_instant_c;
            rt.ptc_instant_temp_c = sample.ptc_instant_c;
            rt.chamber_raw = sample.chamber_raw;
            rt.ptc_raw = sample.ptc_raw;
            rt.chamber_sensor_status = sample.chamber_status;
            rt.ptc_sensor_status = sample.ptc_status;
            rt.last_sensor_ms = now_ms;
        } else {
            rt.chamber_instant_temp_c = NAN;
            rt.ptc_instant_temp_c = NAN;
            rt.chamber_sensor_status = SHU1_SENSOR_INVALID;
            rt.ptc_sensor_status = SHU1_SENSOR_INVALID;
        }

        // Mode priority: Preheat, Drying, Dry-Out, Health Test and Manual are user-driven and never require U1 printing.
        update_material_assistant(&st, &rt, &pr);
        update_scheduled_preheat(&st, now_ms);
        update_pause_policy(&st, &pr, now_ms);
        bool preheat_completed_now = preheat_update(&st, &rt, now_ms);
        bool dryout_completed_now = dryout_update(&st, now_ms);
        bool health_done_now = health_test_update(&st, &rt, now_ms);
        update_auto_context(&st, &pr, &rt, now_ms);
        update_tempering(&st, now_ms);
        bool virtual_door_detected_now = update_virtual_door_detection(&st, &rt, &pr, now_ms);

        if (virtual_door_detected_now) {
            rt.heater_fault = SHU1_HEATER_DOOR_OPEN;
        } else if (dryout_completed_now) {
            rt.heater_fault = SHU1_HEATER_DRYOUT_COMPLETE;
        } else if (health_done_now) {
            rt.heater_fault = st.health_test_result == SHU1_HEALTH_RESULT_OK ? SHU1_HEATER_HEALTH_TEST_COMPLETE : SHU1_HEATER_HEALTH_TEST_FAILED;
        } else if (drying_timer_expired(&st, now_ms)) {
            rt.heater_fault = SHU1_HEATER_DRYING_TIMER_EXPIRED;
        } else if (preheat_completed_now) {
            rt.heater_fault = SHU1_HEATER_PREHEAT_COMPLETE;
        } else if (update_session_watchdog(&st, now_ms)) {
            rt.heater_fault = SHU1_HEATER_SESSION_TIMEOUT;
        } else {
            rt.heater_fault = SHU1_HEATER_OK;
        }

        int target = effective_target_for_settings(&st);
        if (target > SHU1_VALIDATION_MAX_TARGET_C) target = SHU1_VALIDATION_MAX_TARGET_C;
        rt.heater_effective_target_c = target;
        rt.heater_commanded_duty = 0.0f;
        rt.heater_approach_limit = 0.0f;
        snprintf(rt.heater_constraint, sizeof(rt.heater_constraint), "off");
        bool sensor_ok = rt.chamber_sensor_status == SHU1_SENSOR_OK && rt.ptc_sensor_status == SHU1_SENSOR_OK &&
                         isfinite(rt.chamber_instant_temp_c) && isfinite(rt.ptc_instant_temp_c);
        bool chamber_overtemp = sample.chamber_status == SHU1_SENSOR_OK &&
                                isfinite(sample.chamber_instant_c) &&
                                sample.chamber_instant_c >= SHU1_CHAMBER_HARD_CUTOFF_C;
        bool ptc_hard_overtemp = sample.ptc_status == SHU1_SENSOR_OK &&
                                 isfinite(sample.ptc_instant_c) &&
                                 sample.ptc_instant_c >= SHU1_PTC_HARD_CUTOFF_C;
        float board_foldback_cut_c = shu1_ntc_rref_kohm() == 33
            ? SHU1_PTC_FOLDBACK_33K_CUT_C : SHU1_PTC_FOLDBACK_82K_CUT_C;
        float board_foldback_resume_c = shu1_ntc_rref_kohm() == 33
            ? SHU1_PTC_FOLDBACK_33K_RESUME_C : SHU1_PTC_FOLDBACK_82K_RESUME_C;
        float configured_foldback_cut_c = (float)st.ptc_cutoff_c;
        if (configured_foldback_cut_c > 0.0f && configured_foldback_cut_c < 90.0f)
            configured_foldback_cut_c = 90.0f;
        if (configured_foldback_cut_c > 104.0f) configured_foldback_cut_c = 104.0f;
        float foldback_cut_c = configured_foldback_cut_c > 0.0f
            ? configured_foldback_cut_c : board_foldback_cut_c;
        float foldback_resume_c = configured_foldback_cut_c > 0.0f
            ? configured_foldback_cut_c - 3.0f : board_foldback_resume_c;
        if (sample.ptc_status == SHU1_SENSOR_OK && isfinite(sample.ptc_instant_c)) {
            bool previous_foldback = g_ptc_foldback_active;
            if (sample.ptc_instant_c >= foldback_cut_c) g_ptc_foldback_active = true;
            else if (sample.ptc_instant_c < foldback_resume_c) g_ptc_foldback_active = false;
            if (g_ptc_foldback_active != previous_foldback) {
                shu1_event_log_add("info", g_ptc_foldback_active ? "ptc_foldback_on" : "ptc_foldback_off",
                                   g_ptc_foldback_active ? "PTC SSR foldback cut active" : "PTC SSR foldback released");
            }
        }

        bool request_heat = false;
        bool request_fan = false;
        bool pid_active = false;

        if (!CONFIG_SHU1_ENABLE_HEATER_OUTPUT && rt.heater_fault == SHU1_HEATER_OK) {
            rt.heater_fault = SHU1_HEATER_DISABLED_BY_BUILD;
        }
        if (chamber_overtemp || ptc_hard_overtemp) {
            rt.heater_fault = SHU1_HEATER_OVERTEMP;
        } else if (!sensor_ok) {
            rt.heater_fault = SHU1_HEATER_SENSOR_FAULT;
        } else if (st.work_on && target > 0) {
            if (st.safe_overnight_enabled && target > 55) target = 55;
            bool mode_allowed = true;
            if (st.work_mode == SHU1_MODE_AUTO) {
                mode_allowed = auto_mode_heater_allowed(&st, &pr, now_ms);
                if (!mode_allowed) rt.heater_fault = SHU1_HEATER_PRINTER_NOT_READY;
            }
            if (mode_allowed) {
                request_fan = true; // Airflow throughout heat mode, including SSR-off/foldback windows.
                float duty = 0.0f;
                const bool inhibited = g_ptc_foldback_active ||
                    (st.output_safety_latch_enabled && !rt.output_safety_latch_ready) ||
                    shu1_safety_latch_is_set() || shu1_safety_latch_is_inhibited() ||
                    shu1_control_maintenance_active() || !wdt_armed ||
                    !CONFIG_SHU1_ENABLE_HEATER_OUTPUT || !shu1_fan_triac_is_running();
                pid_active = shu1_pid_step_timed(&g_heater_pid, (float)target,
                    rt.chamber_instant_temp_c, !inhibited, esp_timer_get_time(), &duty);
                rt.heater_approach_limit = shu1_pid_approach_cap((float)target - rt.chamber_instant_temp_c);
                snprintf(rt.heater_constraint, sizeof(rt.heater_constraint), "%s",
                    shu1_pid_constraint(pid_active, g_ptc_foldback_active, (float)target,
                                        rt.chamber_instant_temp_c, duty));
                rt.heater_commanded_duty = pid_active && !inhibited ? duty : 0.0f;
                request_heat = pid_active && shu1_pid_window_on(
                    &g_heater_pid, rt.heater_commanded_duty, esp_timer_get_time());
                if (request_heat) request_fan = true;
            }
        }

        if (!pid_active) shu1_pid_reset(&g_heater_pid);

        if (st.work_mode == SHU1_MODE_DRY_OUT && st.dryout_running) {
            request_fan = true;
        }
        if (st.work_mode == SHU1_MODE_HEALTH_TEST && st.health_test_running) {
            request_fan = true;
            if (st.health_test_phase == SHU1_HEALTH_PRE_FAN) request_heat = false;
        }

        // Per-board PTC foldback is non-latching and may only remove SSR demand.
        // Fan demand remains unchanged so the element can cool through the hysteresis band.
        if (g_ptc_foldback_active) request_heat = false;

        bool active_or_armed_heat =
            st.output_safety_latch_armed ||
            st.work_on ||
            st.preheat_running ||
            st.drying_running ||
            st.dryout_running ||
            st.health_test_running ||
            st.keep_warm_active ||
            st.pickup_active ||
            rt.heater_output_on;

        // Remember ZC across SSR off-windows, but not across stopped sessions.
        // Waiting for the first edge at startup is not a recovered heating session.
        const shu1_zero_cross_stats_t zc_stats = shu1_fan_triac_zero_cross_stats();
        const bool zc_present = zc_stats.signal_present;
        const bool zc_session = st.output_safety_latch_armed ||
            (st.work_on && target > 0);
        if (!zc_session) zc_seen_while_armed = false;
        const bool zc_lost = zc_session && zc_seen_while_armed &&
            (!zc_present || zc_stats.signal_gap_count != zc_gap_count);
        zc_gap_count = zc_stats.signal_gap_count;
        if (zc_session && zc_present) zc_seen_while_armed = true;
        if (zc_lost && rt.heater_fault != SHU1_HEATER_OVERTEMP &&
            rt.heater_fault != SHU1_HEATER_SENSOR_FAULT &&
            rt.heater_fault != SHU1_HEATER_NO_RISE)
            rt.heater_fault = SHU1_HEATER_ZERO_CROSS_LOST;

        if (rt.heater_fault != SHU1_HEATER_OK) {
            request_heat = false;
            if (rt.heater_fault == SHU1_HEATER_OVERTEMP) {
                request_fan = true; // fail-safe cooldown path
            } else if (rt.heater_fault == SHU1_HEATER_SENSOR_FAULT && active_or_armed_heat) {
                request_fan = true;
            }
            if (rt.heater_fault == SHU1_HEATER_DISABLED_BY_BUILD && st.work_on && target > 0) {
                // Dry-run builds still show the requested logical fan behavior for UI/tests.
                request_fan = true;
            }
        }

        // Warm-up watchdog: SSR off-phases must not reset it; normal holding near
        // target is not expected to keep increasing temperature.
        update_rise_detector(&rt, pid_active && rt.heater_commanded_duty > 0.0f &&
            (float)target - rt.chamber_instant_temp_c > 2.0f, now_ms);
        if (remote_lease_expired) rt.heater_fault = SHU1_HEATER_LINK_LOST;
        if (rt.heater_fault != SHU1_HEATER_OK) request_heat = false;

        const bool persistent_hazard =
            rt.heater_fault == SHU1_HEATER_NO_RISE ||
            rt.heater_fault == SHU1_HEATER_OVERTEMP ||
            rt.heater_fault == SHU1_HEATER_LINK_LOST ||
            zc_lost ||
            (rt.heater_fault == SHU1_HEATER_SENSOR_FAULT && active_or_armed_heat);
        if (persistent_hazard && !shu1_safety_latch_is_set()) {
            (void)shu1_safety_latch_trip(rt.heater_fault);
            rt.heater_output_on = false;
            shu1_event_log_add("critical", "heater_fault_latched",
                               "hazardous heater fault persisted; explicit safe clear required");
        }
        if (!shu1_safety_latch_is_inhibited() && shu1_safety_latch_clear_requested() && !st.work_on && sensor_ok &&
            (shu1_safety_latch_fault() != SHU1_HEATER_ZERO_CROSS_LOST || zc_present) &&
            !chamber_overtemp && !ptc_hard_overtemp) {
            if (shu1_safety_latch_clear() == ESP_OK) {
                rt.heater_fault = SHU1_HEATER_OK;
                shu1_event_log_add("info", "heater_fault_cleared",
                                   "persistent heater fault cleared after safe-state validation");
            }
        }
        if (shu1_safety_latch_is_set()) {
            rt.heater_fault = shu1_safety_latch_fault();
            shu1_settings_stop(&st);
            request_heat = false;
            request_fan = true; // Fault airflow is independent of purge history.
            shu1_control_snapshot(&ctl_snapshot);
            if (ctl_snapshot.owner != SHU1_CONTROL_NONE) shu1_control_release_any();
        }

        // DragonBreath residual-heat purge. "Heat mode" is the armed PID state,
        // not the instantaneous SSR window pulse. After heat has run, start the
        // purge unless BOTH sensors confirm < release+3 C; once started, release
        // only when BOTH are known below release. Unknown is deliberately hot.
        const bool heat_mode = pid_active && !shu1_safety_latch_is_set();
        if (heat_mode) {
            heated_this_session = true;
            thermal_purge = false;
        } else if (heated_this_session) {
            int release_c = st.cool_release_c;
            if (release_c < 30) release_c = 30;
            if (release_c > 65) release_c = 65;
            const bool chamber_known = rt.chamber_sensor_status == SHU1_SENSOR_OK && isfinite(rt.chamber_temp_c);
            const bool ptc_known = rt.ptc_sensor_status == SHU1_SENSOR_OK && isfinite(rt.ptc_temp_c);
            const bool confirmed_cool = chamber_known && rt.chamber_temp_c < (float)(release_c + 3) &&
                                        ptc_known && rt.ptc_temp_c < (float)(release_c + 3);
            const bool both_cool = chamber_known && rt.chamber_temp_c < (float)release_c &&
                                   ptc_known && rt.ptc_temp_c < (float)release_c;
            thermal_purge = thermal_purge ? !both_cool : !confirmed_cool;
            if (!thermal_purge) heated_this_session = false;
        }
        if (thermal_purge) request_fan = true;

        update_energy_and_stability(&rt, request_heat, request_fan, target, now_ms);
        update_zero_cross_runtime(&rt, now_ms);
        update_v13_extended_features(&st, &rt, &pr, request_heat, request_fan, target, now_ms);
        update_v14_productization(&st, &rt, &pr, request_heat, request_fan, target, now_ms);
        if (st.output_safety_latch_enabled && !rt.output_safety_latch_ready) {
            request_heat = false;
        }
        const bool faulted = shu1_safety_latch_is_set() || shu1_safety_latch_is_inhibited();
        request_heat = shu1_safety_heat_allowed(request_heat,
            shu1_control_maintenance_active(), wdt_armed, faulted);
        request_fan = shu1_safety_airflow(request_fan, heat_mode, faulted);
        // Final governors also govern the reported admitted command. Do not
        // confuse an ordinary OFF portion of the SSR window with an inhibit.
        if (faulted || shu1_control_maintenance_active() || !wdt_armed ||
            !CONFIG_SHU1_ENABLE_HEATER_OUTPUT || rt.heater_fault != SHU1_HEATER_OK ||
            (st.work_mode == SHU1_MODE_HEALTH_TEST && st.health_test_running &&
             st.health_test_phase == SHU1_HEALTH_PRE_FAN) ||
            (st.output_safety_latch_enabled && !rt.output_safety_latch_ready) ||
            !shu1_fan_triac_is_running()) {
            rt.heater_commanded_duty = 0.0f;
            snprintf(rt.heater_constraint, sizeof(rt.heater_constraint), "safety_inhibit");
        }
        if (!st.work_on && !st.scheduled_preheat_enabled) {
            shu1_control_snapshot(&ctl_snapshot);
            if (ctl_snapshot.owner != SHU1_CONTROL_NONE) shu1_control_release_any();
        }
        // The same policy guard covers snapshot, command admission and actuator apply.
        // No external writer can commit OFF between these operations.
        rt.heater_requested = request_heat;
        const bool committed = shu1_state_commit_control_if_epoch(&st, &rt, command_epoch);
        if (!committed) {
            shu1_heater_cut_power();
            shu1_safety_latch_inhibit();
            request_heat = false;
            request_fan = true;
        }
        shu1_heater_set(request_heat, request_fan);
        shu1_control_guard_end(&policy_guard);
        report_fault_change(rt.heater_fault);

        // Set only after one complete fail-closed sensor/safety/output pass.
        if (wdt_armed) {
            g_control_task_healthy = true;
            ESP_ERROR_CHECK(esp_task_wdt_reset());
        }

        // Commands from HTTP/BLE/buttons wake this task so an OFF/latch request
        // converges here without another task racing the SSR GPIO writer.
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SHU1_CONTROL_PERIOD_MS));
    }
}

esp_err_t shu1_safety_start(void) {
    g_control_task_healthy = false;
    g_control_task_handle = NULL;
    BaseType_t ok = xTaskCreate(control_task, "shu1_safety", CONFIG_SHU1_SAFETY_TASK_STACK_BYTES,
                                NULL, 6, &g_control_task_handle);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void shu1_safety_wake(void) {
    TaskHandle_t task = g_control_task_handle;
    if (task) xTaskNotifyGive(task);
}

esp_err_t shu1_safety_wait_healthy(uint32_t timeout_ms) {
    const TickType_t poll_ticks = pdMS_TO_TICKS(20);
    uint32_t waited_ms = 0;
    while (!g_control_task_healthy && waited_ms < timeout_ms) {
        vTaskDelay(poll_ticks > 0 ? poll_ticks : 1);
        waited_ms += 20;
    }
    return g_control_task_healthy ? ESP_OK : ESP_ERR_TIMEOUT;
}
