/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "event_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "settings_store.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SHU1_EVENT_LOG_CAP 32

typedef struct {
    uint32_t seq;
    int64_t ms;
    char level[9]; // "critical" plus NUL; do not silently drop critical phone alerts.
    char code[32];
    char message[96];
} shu1_event_t;

static shu1_event_t g_events[SHU1_EVENT_LOG_CAP];
static uint32_t g_seq;
static uint32_t g_head;
static SemaphoreHandle_t g_lock;
static char g_boot[17];

static void copy_event_text(char *dest, size_t capacity, const char *src) {
    size_t i=0;
    while(i+1<capacity && src[i]) {dest[i]=src[i];++i;}
    dest[i]='\0';
}

void shu1_event_log_init(void) {
    g_lock = xSemaphoreCreateMutex();
    memset(g_events, 0, sizeof(g_events));
    g_seq = 0;
    g_head = 0;
    snprintf(g_boot,sizeof(g_boot),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
    shu1_event_log_add("info", "boot", "event log initialized");
}

void shu1_event_log_add(const char *level, const char *code, const char *message) {
    if (!g_lock) return;
    // Diagnostics may be lost under contention; thermal control must not wait.
    if(xSemaphoreTake(g_lock,0)!=pdTRUE) return;
    shu1_event_t *e = &g_events[g_head % SHU1_EVENT_LOG_CAP];
    e->seq = ++g_seq;
    e->ms = esp_timer_get_time() / 1000;
    copy_event_text(e->level, sizeof(e->level), level ? level : "info");
    copy_event_text(e->code, sizeof(e->code), code ? code : "event");
    copy_event_text(e->message, sizeof(e->message), message ? message : "");
    g_head = (g_head + 1) % SHU1_EVENT_LOG_CAP;
    xSemaphoreGive(g_lock);
}

uint32_t shu1_event_log_count(void) {
    return g_seq;
}

// Read-only, bounded advisory queue. No commands, secrets or free-form messages.
cJSON *shu1_event_notifications(void) {
    if (!g_lock) return NULL;
    shu1_event_t *copy=calloc(SHU1_EVENT_LOG_CAP,sizeof(*copy));
    if(!copy) return NULL;
    xSemaphoreTake(g_lock,portMAX_DELAY);
    uint32_t total=g_seq<SHU1_EVENT_LOG_CAP?g_seq:SHU1_EVENT_LOG_CAP;
    uint32_t start=g_seq<SHU1_EVENT_LOG_CAP?0:g_head;
    for(uint32_t i=0;i<total;i++) copy[i]=g_events[(start+i)%SHU1_EVENT_LOG_CAP];
    const int64_t now_ms=esp_timer_get_time()/1000;
    xSemaphoreGive(g_lock);
    cJSON *root=cJSON_CreateObject();
    if(!root) {free(copy);return NULL;}
    char id[13]; shu1_device_id(id,sizeof(id));
    bool ok=cJSON_AddStringToObject(root,"device_id",id)!=NULL &&
        cJSON_AddStringToObject(root,"boot",g_boot)!=NULL &&
        cJSON_AddNumberToObject(root,"now_ms",(double)now_ms)!=NULL;
    cJSON *rows=cJSON_AddArrayToObject(root,"events");
    ok=ok && rows!=NULL;
    for(uint32_t i=0;i<total && ok;i++) {
        cJSON *r=cJSON_CreateObject();
        if(!r) {ok=false;break;}
        ok=cJSON_AddNumberToObject(r,"seq",copy[i].seq)!=NULL &&
            cJSON_AddNumberToObject(r,"ms",(double)copy[i].ms)!=NULL &&
            cJSON_AddStringToObject(r,"level",copy[i].level)!=NULL &&
            cJSON_AddStringToObject(r,"code",copy[i].code)!=NULL;
        cJSON_AddItemToArray(rows,r);
    }
    free(copy);
    if(!ok) {cJSON_Delete(root);return NULL;}
    return root;
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
