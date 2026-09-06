#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <string.h>

// Inspect every field, including duplicates. An authenticated STOP wins over
// unlock, heartbeat, reset, calibration and any simultaneous ON request.
static inline bool shu1_stop_requested(const cJSON *root) {
    for (const cJSON *p = root ? root->child : NULL; p; p=p->next) {
        if (!p->string) continue;
        if ((!strcmp(p->string,"work_on") && cJSON_IsFalse(p)) ||
            ((!strcmp(p->string,"safe_stop") || !strcmp(p->string,"emergency_stop") ||
              !strcmp(p->string,"disarm_output_safety_latch")) && cJSON_IsTrue(p))) return true;
    }
    return false;
}

static inline bool shu1_reset_request_valid(const cJSON *root) {
    if (!cJSON_IsObject(root)) return false;
    int confirmations = 0, revisions = 0;
    for (const cJSON *item = root->child; item; item = item->next) {
        if (!item->string) return false;
        if (strcmp(item->string, "factory_reset") == 0) {
            if (!cJSON_IsString(item) || strcmp(item->valuestring, "factory-reset") != 0) return false;
            ++confirmations;
        } else if (strcmp(item->string, "expected_revision") == 0) {
            ++revisions;
        } else return false;
    }
    return confirmations == 1 && revisions == 1;
}
