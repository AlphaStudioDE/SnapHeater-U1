/*
 * SnapHeater U1 — Copyright (c) 2026 Damian Borkowski — SPDX-License-Identifier: MIT
 */
#include "wifi_sta.h"
#include "app_state.h"
#include "settings_store.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>

#define CONNECTED BIT0
#define FAILED BIT1
#define SCANNED BIT2
#define STOPPED BIT3
#define SETUP_TIMEOUT_MS 15000
#define AP_LIMIT 8
typedef struct { bool scan; char ssid[33]; char password[65]; } setup_request_t;
static EventGroupHandle_t events;
static QueueHandle_t requests;
static SemaphoreHandle_t status_lock;
static atomic_bool testing, ready, busy, connected;
static atomic_int disconnect_reason;
static int retries;
static char phase[24] = "idle", current_ssid[33], current_ip[16];
static wifi_ap_record_t networks[AP_LIMIT];
static uint16_t network_count;
static uint32_t operation_id;

static void set_phase(const char *value) {
    xSemaphoreTake(status_lock, portMAX_DELAY);
    snprintf(phase, sizeof(phase), "%s", value);
    xSemaphoreGive(status_lock);
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (!atomic_load(&testing)) {
            wifi_config_t cfg;
            if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK && cfg.sta.ssid[0]) esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_STOP) {
        atomic_store(&connected, false);
        xEventGroupSetBits(events, STOPPED);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        xEventGroupSetBits(events, SCANNED);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        atomic_store(&connected, false);
        xEventGroupClearBits(events, CONNECTED);
        wifi_event_sta_disconnected_t *event = data;
        atomic_store(&disconnect_reason, event->reason);
        if (atomic_load(&testing)) xEventGroupSetBits(events, FAILED);
        else if (retries++ < 10) esp_wifi_connect();
        else xEventGroupSetBits(events, FAILED);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        wifi_config_t cfg = {0};
        esp_wifi_get_config(WIFI_IF_STA, &cfg);
        xSemaphoreTake(status_lock, portMAX_DELAY);
        snprintf(current_ssid, sizeof(current_ssid), "%.*s", 32, cfg.sta.ssid);
        snprintf(current_ip, sizeof(current_ip), IPSTR, IP2STR(&event->ip_info.ip));
        xSemaphoreGive(status_lock);
        retries = 0;
        atomic_store(&connected, true);
        xEventGroupSetBits(events, CONNECTED);
    }
}

static const char *failure_phase(int reason) {
    switch (reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "auth_failed";
        case WIFI_REASON_NO_AP_FOUND: return "not_found";
        default: return "connection_failed";
    }
}

static bool stop_wifi(void) {
    xEventGroupClearBits(events, STOPPED);
    if (esp_wifi_stop() != ESP_OK) return false;
    return (xEventGroupWaitBits(events, STOPPED, pdTRUE, pdFALSE, pdMS_TO_TICKS(2000)) & STOPPED) != 0;
}

static void setup_worker(void *arg) {
    (void)arg;
    setup_request_t request;
    while (xQueueReceive(requests, &request, portMAX_DELAY) == pdTRUE) {
        if (request.scan) {
            wifi_scan_config_t scan = { .show_hidden = false };
            xEventGroupClearBits(events, SCANNED);
            esp_err_t err = esp_wifi_scan_start(&scan, false);
            EventBits_t bits = err == ESP_OK
                ? xEventGroupWaitBits(events, SCANNED, pdTRUE, pdFALSE, pdMS_TO_TICKS(SETUP_TIMEOUT_MS)) : 0;
            if (bits & SCANNED) {
                wifi_ap_record_t found[AP_LIMIT] = {0};
                uint16_t count = AP_LIMIT;
                err = esp_wifi_scan_get_ap_records(&count, found);
                xSemaphoreTake(status_lock, portMAX_DELAY);
                network_count = err == ESP_OK ? count : 0;
                memcpy(networks, found, sizeof(networks));
                xSemaphoreGive(status_lock);
                set_phase(err == ESP_OK ? "scan_complete" : "scan_failed");
            } else {
                esp_wifi_scan_stop();
                esp_wifi_clear_ap_list();
                set_phase(err == ESP_OK ? "timeout" : "scan_failed");
            }
        } else {
            wifi_config_t previous = {0}, candidate = {0};
            bool have_previous = esp_wifi_get_config(WIFI_IF_STA, &previous) == ESP_OK;
            memcpy(candidate.sta.ssid, request.ssid, strlen(request.ssid));
            memcpy(candidate.sta.password, request.password, strlen(request.password));
            candidate.sta.threshold.authmode = request.password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
            candidate.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
            atomic_store(&testing, true);
            bool stopped = stop_wifi();
            xEventGroupClearBits(events, CONNECTED | FAILED);
            atomic_store(&disconnect_reason, 0);
            esp_err_t err = stopped ? esp_wifi_set_config(WIFI_IF_STA, &candidate) : ESP_FAIL;
            if (err == ESP_OK) err = esp_wifi_start();
            if (err == ESP_OK) err = esp_wifi_connect();
            EventBits_t bits = err == ESP_OK
                ? xEventGroupWaitBits(events, CONNECTED | FAILED, pdFALSE, pdFALSE, pdMS_TO_TICKS(SETUP_TIMEOUT_MS)) : 0;
            bool success = err == ESP_OK && (bits & CONNECTED) && atomic_load(&connected);
            if (success) {
                shu1_device_config_t cfg;
                shu1_device_config_defaults(&cfg);
                esp_err_t loaded = shu1_settings_store_load_device_config(&cfg);
                snprintf(cfg.wifi_ssid, sizeof(cfg.wifi_ssid), "%s", request.ssid);
                snprintf(cfg.wifi_password, sizeof(cfg.wifi_password), "%s", request.password);
                err = loaded == ESP_OK || loaded == ESP_ERR_NVS_NOT_FOUND
                    ? shu1_settings_store_save_device_config(&cfg) : loaded;
                memset(cfg.wifi_password, 0, sizeof(cfg.wifi_password));
                success = err == ESP_OK;
                set_phase(success ? "connected" : "save_failed");
            } else {
                set_phase(err != ESP_OK ? "connection_failed" :
                    (bits & FAILED) ? failure_phase(atomic_load(&disconnect_reason)) : "timeout");
            }
            if (!success && have_previous) {
                // A failed candidate never replaces the persisted credentials.
                // Restore the previous runtime network too, without restarting the MCU.
                stop_wifi();
                esp_wifi_set_config(WIFI_IF_STA, &previous);
                xEventGroupClearBits(events, CONNECTED | FAILED);
                retries = 0;
                atomic_store(&testing, false);
                esp_wifi_start();
            } else atomic_store(&testing, false);
            memset(&previous, 0, sizeof(previous));
            memset(&candidate, 0, sizeof(candidate));
        }
        memset(&request, 0, sizeof(request));
        {
            SHU1_CONTROL_GUARD(guard);
            shu1_control_maintenance_end();
            atomic_store(&busy, false);
        }
    }
    vTaskDelete(NULL);
}

esp_err_t shu1_wifi_setup_request(const cJSON *request) {
    if (!atomic_load(&ready) || atomic_load(&busy) || !cJSON_IsObject(request)) return ESP_ERR_INVALID_STATE;
    cJSON *action = cJSON_GetObjectItemCaseSensitive(request, "action");
    if (!cJSON_IsString(action)) return ESP_ERR_INVALID_ARG;
    setup_request_t job = {0};
    job.scan = strcmp(action->valuestring, "scan") == 0;
    if (!job.scan) {
        if (strcmp(action->valuestring, "connect") != 0) return ESP_ERR_INVALID_ARG;
        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(request, "ssid");
        cJSON *password = cJSON_GetObjectItemCaseSensitive(request, "password");
        if (!cJSON_IsString(ssid) || !cJSON_IsString(password) ||
            strlen(ssid->valuestring) < 1 || strlen(ssid->valuestring) > 32 ||
            (strlen(password->valuestring) != 0 &&
             (strlen(password->valuestring) < 8 || strlen(password->valuestring) > 63))) return ESP_ERR_INVALID_ARG;
        snprintf(job.ssid, sizeof(job.ssid), "%s", ssid->valuestring);
        snprintf(job.password, sizeof(job.password), "%s", password->valuestring);
    }
    if (!shu1_control_network_setup_begin()) return ESP_ERR_INVALID_STATE;
    atomic_store(&busy, true);
    xSemaphoreTake(status_lock, portMAX_DELAY);
    ++operation_id;
    network_count = job.scan ? 0 : network_count;
    snprintf(phase, sizeof(phase), "%s", job.scan ? "scanning" : "connecting");
    xSemaphoreGive(status_lock);
    esp_err_t result = xQueueSend(requests, &job, 0) == pdTRUE ? ESP_OK : ESP_FAIL;
    memset(&job, 0, sizeof(job));
    if (result != ESP_OK) {
        set_phase("connection_failed");
        atomic_store(&busy, false);
        shu1_control_maintenance_end();
    }
    return result;
}

bool shu1_wifi_status_json(cJSON *root) {
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    if (!wifi) return false;
    if (!status_lock) return cJSON_AddBoolToObject(wifi, "supported", false) != NULL;
    xSemaphoreTake(status_lock, portMAX_DELAY);
    bool ok = cJSON_AddBoolToObject(wifi, "supported", atomic_load(&ready)) &&
        cJSON_AddBoolToObject(wifi, "connected", atomic_load(&connected)) &&
        cJSON_AddBoolToObject(wifi, "busy", atomic_load(&busy)) &&
        cJSON_AddStringToObject(wifi, "phase", phase) &&
        cJSON_AddStringToObject(wifi, "ssid", current_ssid) &&
        cJSON_AddStringToObject(wifi, "ip", atomic_load(&connected) ? current_ip : "") &&
        cJSON_AddNumberToObject(wifi, "operation", operation_id) &&
        cJSON_AddNumberToObject(wifi, "timeout_ms", SETUP_TIMEOUT_MS);
    cJSON *aps = ok ? cJSON_AddArrayToObject(wifi, "networks") : NULL;
    ok = aps != NULL;
    for (uint16_t i = 0; ok && i < network_count; ++i) {
        cJSON *ap = cJSON_CreateObject();
        if (!ap) { ok = false; break; }
        ok = cJSON_AddStringToObject(ap, "ssid", (char *)networks[i].ssid) &&
             cJSON_AddNumberToObject(ap, "rssi", networks[i].rssi) &&
             cJSON_AddBoolToObject(ap, "secure", networks[i].authmode != WIFI_AUTH_OPEN);
        if (!ok || !cJSON_AddItemToArray(aps, ap)) { cJSON_Delete(ap); ok = false; }
    }
    xSemaphoreGive(status_lock);
    return ok;
}

esp_err_t shu1_wifi_start(void) {
    events = xEventGroupCreate();
    status_lock = xSemaphoreCreateMutex();
    requests = xQueueCreate(1, sizeof(setup_request_t));
    if (!events || !status_lock || !requests) return ESP_ERR_NO_MEM;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    // Candidate credentials must never reach NVS until DHCP succeeds.
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL, NULL));
    shu1_device_config_t cfg;
    shu1_device_config_defaults(&cfg);
    shu1_settings_store_load_device_config(&cfg);
    wifi_config_t wifi = {0};
    memcpy(wifi.sta.ssid, cfg.wifi_ssid, strnlen(cfg.wifi_ssid, 32));
    memcpy(wifi.sta.password, cfg.wifi_password, strnlen(cfg.wifi_password, 64));
    wifi.sta.threshold.authmode = cfg.wifi_password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wifi.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    memset(cfg.wifi_password, 0, sizeof(cfg.wifi_password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
    memset(&wifi, 0, sizeof(wifi));
    ESP_ERROR_CHECK(esp_wifi_start());
    if (xTaskCreate(setup_worker, "wifi_setup", 6144, NULL, 3, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    atomic_store(&ready, true);
    // Services can start without an IP and accept connections once Wi-Fi is ready.
    return ESP_OK;
}
