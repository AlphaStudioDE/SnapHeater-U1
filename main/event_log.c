/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "event_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

#define SHU1_EVENT_LOG_CAP 32

typedef struct {
    uint32_t seq;
    int64_t ms;
    char level[8];
    char code[32];
    char message[96];
} shu1_event_t;

static shu1_event_t g_events[SHU1_EVENT_LOG_CAP];
static uint32_t g_seq;
static uint32_t g_head;
static SemaphoreHandle_t g_lock;

void shu1_event_log_init(void) {
    g_lock = xSemaphoreCreateMutex();
    memset(g_events, 0, sizeof(g_events));
    g_seq = 0;
    g_head = 0;
    shu1_event_log_add("info", "boot", "event log initialized");
}

void shu1_event_log_add(const char *level, const char *code, const char *message) {
    if (!g_lock) return;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    shu1_event_t *e = &g_events[g_head % SHU1_EVENT_LOG_CAP];
    e->seq = ++g_seq;
    e->ms = esp_timer_get_time() / 1000;
    snprintf(e->level, sizeof(e->level), "%s", level ? level : "info");
    snprintf(e->code, sizeof(e->code), "%s", code ? code : "event");
    snprintf(e->message, sizeof(e->message), "%s", message ? message : "");
    g_head = (g_head + 1) % SHU1_EVENT_LOG_CAP;
    xSemaphoreGive(g_lock);
}

uint32_t shu1_event_log_count(void) {
    return g_seq;
}

cJSON *shu1_event_log_to_json(void) {
    cJSON *arr = cJSON_CreateArray();
    if (!arr || !g_lock) return arr;
    xSemaphoreTake(g_lock, portMAX_DELAY);
    uint32_t total = g_seq < SHU1_EVENT_LOG_CAP ? g_seq : SHU1_EVENT_LOG_CAP;
    uint32_t start = (g_seq < SHU1_EVENT_LOG_CAP) ? 0 : g_head;
    for (uint32_t i = 0; i < total; ++i) {
        const shu1_event_t *e = &g_events[(start + i) % SHU1_EVENT_LOG_CAP];
        if (e->seq == 0) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "seq", e->seq);
        cJSON_AddNumberToObject(o, "ms", (double)e->ms);
        cJSON_AddStringToObject(o, "level", e->level);
        cJSON_AddStringToObject(o, "code", e->code);
        cJSON_AddStringToObject(o, "message", e->message);
        cJSON_AddItemToArray(arr, o);
    }
    xSemaphoreGive(g_lock);
    return arr;
}
