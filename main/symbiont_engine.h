#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "cJSON.h"

// Serialized by the Moonraker module. No socket, GPIO, NVS or job writes here.
typedef struct {
    int sequence, pending, phase, demand, pending_demand, last_channel;
    int64_t sent_ms, due_ms;
    bool active, top, query_top, top_required;
    const char *status;
} shu1_symbiont_engine_t;

static inline bool shu1_symbiont_speed(const cJSON *object, double *speed) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(object,"speed");
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<0 || v->valuedouble>1) return false;
    *speed=v->valuedouble;return true;
}

static inline void shu1_symbiont_notify(shu1_symbiont_engine_t *s, const cJSON *objects, int64_t now) {
    if (!s->active) return;
    const cJSON *fan=cJSON_GetObjectItemCaseSensitive(objects,"fan_generic cavity_fan");
    const cJSON *top=cJSON_GetObjectItemCaseSensitive(objects,"purifier");
    if (fan || top || cJSON_GetObjectItemCaseSensitive(objects,"webhooks")) {
        // Partial notifications are hints, never authority to lower a fan.
        // Invalidate an older observation and request a full alarm/speed snapshot.
        s->pending=0;s->phase=0;s->due_ms=now;
    }
}

static inline void shu1_symbiont_response(shu1_symbiont_engine_t *s, const cJSON *root, int64_t now) {
    const cJSON *id=cJSON_GetObjectItemCaseSensitive(root,"id");
    if (!s->active || !s->pending || !cJSON_IsNumber(id) || id->valuedouble!=s->pending) return;
    s->pending=0;s->due_ms=now+500;
    const cJSON *result=cJSON_GetObjectItemCaseSensitive(root,"result");
    if (cJSON_GetObjectItemCaseSensitive(root,"error") || !cJSON_IsObject(result)) {
        s->phase=0;s->status="command_error";s->due_ms=now+2000;return;
    }
    if (s->pending_demand!=s->demand) {s->phase=0;return;}
    if (s->phase!=0) {
        const char *state=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(result,"state"));
        s->status=state && !strcmp(state,"success") ? "verifying":"command_error";
        // Always obtain a new full observation before the next write, even to the other fan.
        s->phase=0;s->due_ms=now;return;
    }
    const cJSON *objects=cJSON_GetObjectItemCaseSensitive(result,"status");
    const char *ready=cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(objects,"webhooks"),"state"));
    const cJSON *top=cJSON_GetObjectItemCaseSensitive(objects,"purifier");
    const cJSON *detected=cJSON_GetObjectItemCaseSensitive(top,"power_detected");
    const cJSON *alarm=cJSON_GetObjectItemCaseSensitive(top,"critical_temp_reported");
    const cJSON *fault=cJSON_GetObjectItemCaseSensitive(top,"fan_fault_reported");
    double aux=0,exhaust=0;
    if (!ready || strcmp(ready,"ready") ||
        !shu1_symbiont_speed(cJSON_GetObjectItemCaseSensitive(objects,"fan_generic cavity_fan"),&aux) ||
        (s->query_top && (!cJSON_IsBool(detected) || !cJSON_IsBool(alarm) || !cJSON_IsBool(fault)))) {
        s->status="invalid_data";return;
    }
    s->top=s->query_top && cJSON_IsTrue(detected);
    if (s->top) s->top_required=true;
    if ((s->top_required && !s->top) || (s->query_top && (cJSON_IsTrue(alarm) || cJSON_IsTrue(fault)))) {
        s->status="top_cover_fault";return;
    }
    if (s->top && !shu1_symbiont_speed(cJSON_GetObjectItemCaseSensitive(top,"exhaust_fan"),&exhaust)) {
        s->status="invalid_data";return;
    }
    bool aux_diff=fabs(aux*100-s->demand)>1.1;
    bool top_diff=s->top && fabs(exhaust*100-s->demand)>1.1;
    if (top_diff && (!aux_diff || s->last_channel==1)) s->phase=2;
    else if (aux_diff) s->phase=1;
    s->status=s->phase ? "correcting":"verified";
    if (s->phase) s->due_ms=now;
}

static inline bool shu1_symbiont_next(shu1_symbiont_engine_t *s, bool active,
    bool connected, bool exact_aux, bool has_top, int demand, int64_t now, char *out, size_t size) {
    if (!active || !connected || !exact_aux) {
        s->active=false;s->pending=0;s->phase=0;s->top=false;s->top_required=false;
        s->status=!active ? "auto_or_idle":!connected ? "disconnected":"unsupported_fan";
        return false;
    }
    if (demand<0 || demand>100) return false;
    if (!s->active) {s->active=true;s->due_ms=0;s->phase=0;}
    if (s->demand!=demand) {s->demand=demand;s->pending=0;s->phase=0;s->due_ms=0;}
    if (s->pending && now-s->sent_ms>=2000) {
        s->pending=0;s->phase=0;s->status="timeout";s->due_ms=now+1000;
    }
    if (s->pending || now<s->due_ms) return false;
    // A stale query must never authorize a write after a stalled worker.
    if (s->phase && now-s->sent_ms>2000) s->phase=0;
    if (s->sequence<=-1000000000) {s->status="id_exhausted";return false;}
    if (!s->sequence) s->sequence=-1000;
    int id=--s->sequence;
    int n;
    if (!s->phase) {
        s->query_top=has_top;
        n=snprintf(out,size,"{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"printer.objects.query\",\"params\":{\"objects\":{\"webhooks\":[\"state\"],\"fan_generic cavity_fan\":[\"speed\"]%s}}}",id,
            has_top ? ",\"purifier\":null":"");
    } else if (s->phase==1) {
        n=snprintf(out,size,"{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"printer.control.generic_fan\",\"params\":{\"name\":\"cavity_fan\",\"speed\":%d}}",id,demand);
    } else {
        n=snprintf(out,size,"{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"printer.control.purifier\",\"params\":{\"fan\":\"exhaust\",\"speed\":%d,\"skip_delay\":1}}",id,demand);
    }
    if (n<0 || (size_t)n>=size) {s->phase=0;s->status="encoding_error";return false;}
    if(s->phase) s->last_channel=s->phase;
    s->pending=id;s->pending_demand=demand;s->sent_ms=now;return true;
}
