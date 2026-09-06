/*
 * Persistent hazardous-fault behavior adapted from DragonBreath commit
 * 5e2f668 (MIT): fail-safe load, persist on trip, and persist-first clear.
 */
#include "safety_latch.h"
#include "heater.h"
#include "event_log.h"
#include "ntc.h"
#include "thermal_limits.h"
#include <math.h>

#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "shu1_fault_latch";
static bool g_latched;
static bool g_inhibited;
static bool g_clear_requested;
static bool g_persist_pending;
static shu1_heater_fault_t g_fault = SHU1_HEATER_OK;
static portMUX_TYPE g_latch_mux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t g_nvs_mutex;
static int64_t g_last_persist_attempt_us;
static uint64_t g_generation;
static bool g_deferred_clear;
static uint64_t g_clear_generation;

// Caller must hold g_nvs_mutex. Keeping the NVS transaction separate lets the
// retry path re-read RAM only after it owns the same lock as clear().
static esp_err_t persist_fault_locked(bool latched, shu1_heater_fault_t fault) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("app_nvs", NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_u8(h, "fault_code", (uint8_t)fault);
        if (err == ESP_OK) err = nvs_set_u8(h, "fault_latch", latched ? 1 : 0);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    return err;
}

esp_err_t shu1_safety_latch_init(void) {
    if (!g_nvs_mutex) g_nvs_mutex = xSemaphoreCreateMutex();
    g_latched = false;
    g_clear_requested = false;
    g_persist_pending = false;
    g_fault = SHU1_HEATER_OK;
    g_last_persist_attempt_us = 0;
    ++g_generation;g_deferred_clear=false;
    if (!g_nvs_mutex) {
        g_latched = true;
        g_fault = SHU1_HEATER_NVS_UNREADABLE;
        return ESP_ERR_NO_MEM;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open("app_nvs", NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) {
        g_latched = true;
        g_fault = SHU1_HEATER_NVS_UNREADABLE;
        return err;
    }
    uint8_t latched = 0;
    uint8_t reason = SHU1_HEATER_OK;
    esp_err_t latch_err = nvs_get_u8(h, "fault_latch", &latched);
    esp_err_t reason_err = nvs_get_u8(h, "fault_code", &reason);
    nvs_close(h);
    if (latch_err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (latch_err != ESP_OK || (latched && reason_err != ESP_OK)) {
        g_latched = true;
        g_fault = SHU1_HEATER_NVS_UNREADABLE;
        return latch_err != ESP_OK ? latch_err : reason_err;
    }
    if (latched) {
        if (reason <= SHU1_HEATER_OK || reason >= SHU1_HEATER_FAULT_COUNT)
            reason = SHU1_HEATER_PERSISTED_FAULT;
        g_latched = true;
        g_fault = (shu1_heater_fault_t)reason;
        ESP_LOGW(TAG, "restored persistent heater fault: %u", (unsigned)reason);
    }
    return ESP_OK;
}

void shu1_safety_latch_inhibit(void) {
    portENTER_CRITICAL(&g_latch_mux);
    g_inhibited = true;
    g_latched = true;
    g_fault = SHU1_HEATER_PERSISTED_FAULT;
    g_clear_requested = false;
    ++g_generation;
    g_deferred_clear=false;
    portEXIT_CRITICAL(&g_latch_mux);
}
bool shu1_safety_latch_is_inhibited(void) {
    portENTER_CRITICAL(&g_latch_mux);
    bool value = g_inhibited;
    portEXIT_CRITICAL(&g_latch_mux);
    return value;
}
bool shu1_safety_latch_is_set(void) {
    portENTER_CRITICAL(&g_latch_mux);
    bool value = g_latched;
    portEXIT_CRITICAL(&g_latch_mux);
    return value;
}

shu1_heater_fault_t shu1_safety_latch_fault(void) {
    portENTER_CRITICAL(&g_latch_mux);
    shu1_heater_fault_t value = g_fault;
    portEXIT_CRITICAL(&g_latch_mux);
    return value;
}

esp_err_t shu1_safety_latch_trip(shu1_heater_fault_t fault) {
    shu1_heater_cut_power(); // Control-task only; never wait on NVS with SSR energized.
    if (fault <= SHU1_HEATER_OK || fault >= SHU1_HEATER_FAULT_COUNT)
        fault = SHU1_HEATER_PERSISTED_FAULT;
    if (!g_nvs_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    portENTER_CRITICAL(&g_latch_mux);
    g_latched = true;
    g_fault = fault;
    g_clear_requested = false; // A request predating this fault cannot clear it.
    g_persist_pending = true;
    g_last_persist_attempt_us = esp_timer_get_time();
    portEXIT_CRITICAL(&g_latch_mux);
    esp_err_t err = persist_fault_locked(true, fault);
    xSemaphoreGive(g_nvs_mutex);
    if (err == ESP_OK) {
        portENTER_CRITICAL(&g_latch_mux);
        if (g_latched && g_fault == fault) g_persist_pending = false;
        portEXIT_CRITICAL(&g_latch_mux);
    }
    if (err != ESP_OK) ESP_LOGE(TAG, "heater fault could not be persisted: %s", esp_err_to_name(err));
    return err;
}

void shu1_safety_latch_trip_volatile(shu1_heater_fault_t fault) {
    if (fault <= SHU1_HEATER_OK || fault >= SHU1_HEATER_FAULT_COUNT)
        fault = SHU1_HEATER_PERSISTED_FAULT;
    portENTER_CRITICAL(&g_latch_mux);
    /* Never replace a persistent/earlier fault with the less informative
     * operator panic reason. This also preserves a pending NVS transaction. */
    if (!g_latched) {
        g_latched = true;
        g_fault = fault;
        g_clear_requested = false;
    }
    portEXIT_CRITICAL(&g_latch_mux);
}

esp_err_t shu1_safety_latch_retry_persist(void) {
    portENTER_CRITICAL(&g_latch_mux);
    const bool pending = g_persist_pending;
    const int64_t last_attempt=g_last_persist_attempt_us;
    portEXIT_CRITICAL(&g_latch_mux);
    if (!pending) return ESP_OK;

    const int64_t now = esp_timer_get_time();
    if (last_attempt != 0 && now - last_attempt < 2000000)
        return ESP_ERR_INVALID_STATE;
    if (!g_nvs_mutex || xSemaphoreTake(g_nvs_mutex, 0) != pdTRUE)
        return ESP_ERR_TIMEOUT;

    // Match DragonBreath: re-read only while holding the persistence lock. A
    // concurrent successful clear can therefore never be overwritten by a stale
    // retry that captured the old latched state before acquiring the mutex.
    portENTER_CRITICAL(&g_latch_mux);
    const bool still_latched = g_latched;
    const shu1_heater_fault_t fault = g_fault;
    const uint64_t generation=g_generation;
    g_last_persist_attempt_us = now;
    portEXIT_CRITICAL(&g_latch_mux);
    esp_err_t err = still_latched
        ? persist_fault_locked(true, fault)
        : ESP_OK;
    if (err == ESP_OK) {
        portENTER_CRITICAL(&g_latch_mux);
        if(g_generation==generation) g_persist_pending = false;
        portEXIT_CRITICAL(&g_latch_mux);
        ESP_LOGW(TAG, "previously failed heater-fault persistence recovered");
    } else {
        ESP_LOGE(TAG, "heater-fault persistence retry failed: %s", esp_err_to_name(err));
    }
    xSemaphoreGive(g_nvs_mutex);
    return err;
}

void shu1_safety_latch_request_clear(void) {
    portENTER_CRITICAL(&g_latch_mux);
    g_clear_requested = true;
    portEXIT_CRITICAL(&g_latch_mux);
}

bool shu1_safety_latch_take_clear_request(void) {
    portENTER_CRITICAL(&g_latch_mux);
    bool value = g_clear_requested;
    g_clear_requested = false;
    portEXIT_CRITICAL(&g_latch_mux);
    return value;
}

esp_err_t shu1_safety_latch_clear(void) {
    if (shu1_safety_latch_is_inhibited()) return ESP_ERR_INVALID_STATE;
    if (!g_nvs_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    esp_err_t err = persist_fault_locked(false, SHU1_HEATER_OK);
    if (err == ESP_OK) {
        portENTER_CRITICAL(&g_latch_mux);
        g_latched = false;
        g_clear_requested = false;
        g_persist_pending = false;
        g_fault = SHU1_HEATER_OK;
        portEXIT_CRITICAL(&g_latch_mux);
    }
    xSemaphoreGive(g_nvs_mutex);
    return err;
}

void shu1_safety_latch_defer_trip(shu1_heater_fault_t fault) {
    shu1_heater_cut_power();
    if(fault<=SHU1_HEATER_OK || fault>=SHU1_HEATER_FAULT_COUNT) fault=SHU1_HEATER_PERSISTED_FAULT;
    portENTER_CRITICAL(&g_latch_mux);
    ++g_generation;g_latched=true;g_fault=fault;g_persist_pending=true;
    g_clear_requested=false;g_deferred_clear=false;g_last_persist_attempt_us=0;
    portEXIT_CRITICAL(&g_latch_mux);
}

void shu1_safety_latch_defer_clear(void) {
    portENTER_CRITICAL(&g_latch_mux);
    if(!g_inhibited && g_latched) {
        g_deferred_clear=true;g_clear_generation=g_generation;
    }
    portEXIT_CRITICAL(&g_latch_mux);
}

const char *shu1_safety_latch_clear_block_reason(const shu1_settings_t *settings,const shu1_runtime_t *runtime) {
    // Worker caller holds policy guard; recheck AFTER storage, not just at request time.
#define st (*settings)
#define rt (*runtime)
    int64_t age=esp_timer_get_time()/1000-rt.last_sensor_ms;
    if(shu1_safety_latch_is_inhibited()) return "inhibited";
    if(st.work_on || st.scheduled_preheat_enabled || rt.heater_output_on) return "busy";
    if(rt.last_sensor_ms<=0 || age<0 || age>1500 ||
       rt.chamber_sensor_status!=SHU1_SENSOR_OK || rt.ptc_sensor_status!=SHU1_SENSOR_OK ||
       !isfinite(rt.chamber_instant_temp_c) || !isfinite(rt.ptc_instant_temp_c)) return "sensors";
    if(!(shu1_safety_temperature(rt.chamber_instant_temp_c,shu1_ntc_get_offset_c(0))<SHU1_CHAMBER_HARD_CUTOFF_C) ||
       !(shu1_safety_temperature(rt.ptc_instant_temp_c,shu1_ntc_get_offset_c(1))<SHU1_PTC_HARD_CUTOFF_C)) return "temperature";
    if(shu1_safety_latch_fault()==SHU1_HEATER_ZERO_CROSS_LOST && !rt.zero_cross_signal_present) return "zero_cross";
    return "";
#undef st
#undef rt
}

static bool clear_state_safe(void) {
    shu1_settings_t settings=shu1_state_get_settings();
    shu1_runtime_t runtime=shu1_state_get_runtime();
    return shu1_safety_latch_clear_block_reason(&settings,&runtime)[0]=='\0';
}

void shu1_safety_latch_service(void) {
    // Only the storage worker calls this. Never hold policy across NVS or its mutex.
    portENTER_CRITICAL(&g_latch_mux);
    bool clear=g_deferred_clear && !g_inhibited;
    uint64_t generation=g_clear_generation;
    g_deferred_clear=false;
    portEXIT_CRITICAL(&g_latch_mux);
    if(!clear) {(void)shu1_safety_latch_retry_persist();return;}
    if(!g_nvs_mutex) return;
    xSemaphoreTake(g_nvs_mutex,portMAX_DELAY);
    shu1_control_guard_t guard=shu1_control_guard_begin();
    bool safe=clear_state_safe();
    portENTER_CRITICAL(&g_latch_mux);
    safe=safe && g_latched && !g_inhibited && generation==g_generation;
    portEXIT_CRITICAL(&g_latch_mux);
    shu1_control_guard_end(&guard);
    esp_err_t err=safe ? persist_fault_locked(false,SHU1_HEATER_OK):ESP_ERR_INVALID_STATE;
    guard=shu1_control_guard_begin();
    safe=err==ESP_OK && clear_state_safe();
    portENTER_CRITICAL(&g_latch_mux);
    safe=safe && !g_inhibited && generation==g_generation;
    if(safe) {
        ++g_generation;g_latched=false;g_fault=SHU1_HEATER_OK;
        g_clear_requested=false;g_persist_pending=false;
    } else if(g_latched) {
        g_persist_pending=true;g_last_persist_attempt_us=0;
    }
    portEXIT_CRITICAL(&g_latch_mux);
    shu1_control_guard_end(&guard);
    xSemaphoreGive(g_nvs_mutex);
    shu1_event_log_add(safe ? "info":"warn",safe ? "heater_fault_cleared":"fault_clear_failed",
        safe ? "Fault clear persisted and current safe state revalidated; heating remains off":"Fault clear rejected or storage failed; latch remains active");
    if(!safe) (void)shu1_safety_latch_retry_persist();
}
