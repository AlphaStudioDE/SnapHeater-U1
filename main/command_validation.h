#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <string.h>

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
