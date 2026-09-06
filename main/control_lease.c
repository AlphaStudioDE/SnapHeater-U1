#include "control_lease.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>

static SemaphoreHandle_t g_lock;
static shu1_control_source_t g_owner;
static uint32_t g_revision;
static char g_id[SHU1_LEASE_ID_LEN + 1];
static int64_t g_deadline_us;

static bool remote_source(shu1_control_source_t source) {
    return source == SHU1_CONTROL_REST || source == SHU1_CONTROL_BLE;
}

static void advance_revision_locked(void) {
    ++g_revision;
    if (g_revision == 0 || g_revision == SHU1_CONTROL_REVISION_ANY) ++g_revision;
}

static void issue_locked(char out[SHU1_LEASE_ID_LEN + 1]) {
    uint8_t bytes[16];
    esp_fill_random(bytes, sizeof(bytes));
    for (size_t i = 0; i < sizeof(bytes); ++i) snprintf(g_id + i * 2, 3, "%02x", bytes[i]);
    g_id[SHU1_LEASE_ID_LEN] = '\0';
    g_deadline_us = esp_timer_get_time() + (int64_t)SHU1_LEASE_TIMEOUT_MS * 1000;
    if (out) memcpy(out, g_id, sizeof(g_id));
}

esp_err_t shu1_control_lease_init(void) {
    if (!g_lock) g_lock = xSemaphoreCreateMutex();
    return g_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

shu1_control_result_t shu1_control_claim(shu1_control_source_t source,
                                         bool takeover,
                                         uint32_t expected_revision,
                                         char out[SHU1_LEASE_ID_LEN + 1]) {
    if (out) out[0] = '\0';
    if (!g_lock || source == SHU1_CONTROL_NONE) return SHU1_CONTROL_NOT_OWNER;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    if (expected_revision != SHU1_CONTROL_REVISION_ANY && expected_revision != g_revision) {
        xSemaphoreGive(g_lock);
        return SHU1_CONTROL_STALE;
    }
    if (g_owner != SHU1_CONTROL_NONE && g_owner != source && !takeover) {
        xSemaphoreGive(g_lock);
        return SHU1_CONTROL_BUSY;
    }
    bool same_live_remote = g_owner == source && remote_source(source) && g_id[0] &&
                            esp_timer_get_time() <= g_deadline_us;
    g_owner = source;
    if (!same_live_remote) {
        g_id[0] = '\0';
        g_deadline_us = 0;
        if (remote_source(source)) issue_locked(out);
    } else if (out) {
        memcpy(out, g_id, sizeof(g_id));
    }
    advance_revision_locked();
    xSemaphoreGive(g_lock);
    return SHU1_CONTROL_OK;
}

shu1_control_result_t shu1_control_authorize(shu1_control_source_t source,
                                             const char *lease_id,
                                             uint32_t expected_revision) {
    if (!g_lock) return SHU1_CONTROL_NOT_OWNER;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    shu1_control_result_t result = SHU1_CONTROL_OK;
    if (expected_revision != SHU1_CONTROL_REVISION_ANY && expected_revision != g_revision)
        result = SHU1_CONTROL_STALE;
    else if (g_owner != source)
        result = SHU1_CONTROL_NOT_OWNER;
    else if (remote_source(source) &&
             (!lease_id || !g_id[0] || strcmp(lease_id, g_id) != 0 ||
              esp_timer_get_time() > g_deadline_us))
        result = SHU1_CONTROL_INVALID_LEASE;
    if (result == SHU1_CONTROL_OK) advance_revision_locked();
    xSemaphoreGive(g_lock);
    return result;
}

bool shu1_control_lease_heartbeat_for(shu1_control_source_t source,
                                      const char *lease_id) {
    if (!g_lock || !lease_id || !remote_source(source)) return false;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    bool valid = g_owner == source && g_id[0] && strcmp(lease_id, g_id) == 0 &&
                 esp_timer_get_time() <= g_deadline_us;
    if (valid) g_deadline_us = esp_timer_get_time() + (int64_t)SHU1_LEASE_TIMEOUT_MS * 1000;
    xSemaphoreGive(g_lock);
    return valid;
}

void shu1_control_release_any(void) {
    if (!g_lock) return;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    g_owner = SHU1_CONTROL_NONE;
    g_id[0] = '\0';
    g_deadline_us = 0;
    advance_revision_locked();
    xSemaphoreGive(g_lock);
}

bool shu1_control_lease_expired(void) {
    if (!g_lock) return false;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    bool expired = remote_source(g_owner) && g_id[0] && esp_timer_get_time() > g_deadline_us;
    xSemaphoreGive(g_lock);
    return expired;
}

void shu1_control_snapshot(shu1_control_snapshot_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!g_lock) return;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    out->owner = g_owner;
    out->revision = g_revision;
    out->lease_active = remote_source(g_owner) && g_id[0];
    int64_t us = out->lease_active ? g_deadline_us - esp_timer_get_time() : 0;
    out->lease_remaining_ms = us > 0 ? (uint32_t)(us / 1000) : 0;
    xSemaphoreGive(g_lock);
}

void shu1_control_lease_get_id_for(shu1_control_source_t source,
                                   char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!g_lock) return;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    if (g_owner == source && remote_source(source)) snprintf(out, out_size, "%s", g_id);
    xSemaphoreGive(g_lock);
}

const char *shu1_control_source_str(shu1_control_source_t source) {
    switch (source) {
        case SHU1_CONTROL_REST: return "rest";
        case SHU1_CONTROL_BLE: return "ble";
        case SHU1_CONTROL_PHYSICAL: return "physical";
        case SHU1_CONTROL_LOCAL_JOB: return "local_job";
        default: return "none";
    }
}
