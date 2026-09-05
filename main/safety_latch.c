/*
 * Persistent hazardous-fault behavior adapted from DragonBreath commit
 * 5e2f668 (MIT): fail-safe load, persist on trip, and persist-first clear.
 */
#include "safety_latch.h"
#include "heater.h"

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
    portEXIT_CRITICAL(&g_latch_mux);
    if (!pending) return ESP_OK;

    const int64_t now = esp_timer_get_time();
    if (g_last_persist_attempt_us != 0 && now - g_last_persist_attempt_us < 2000000)
        return ESP_ERR_INVALID_STATE;
    if (!g_nvs_mutex || xSemaphoreTake(g_nvs_mutex, 0) != pdTRUE)
        return ESP_ERR_TIMEOUT;

    // Match DragonBreath: re-read only while holding the persistence lock. A
    // concurrent successful clear can therefore never be overwritten by a stale
    // retry that captured the old latched state before acquiring the mutex.
    portENTER_CRITICAL(&g_latch_mux);
    const bool still_latched = g_latched;
    const shu1_heater_fault_t fault = g_fault;
    g_last_persist_attempt_us = now;
    portEXIT_CRITICAL(&g_latch_mux);
    esp_err_t err = still_latched
        ? persist_fault_locked(true, fault)
        : ESP_OK;
    if (err == ESP_OK) {
        portENTER_CRITICAL(&g_latch_mux);
        g_persist_pending = false;
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

bool shu1_safety_latch_clear_requested(void) {
    portENTER_CRITICAL(&g_latch_mux);
    bool value = g_clear_requested;
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
