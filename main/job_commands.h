#pragma once
#include "app_state.h"
#include "app_config.h"
#include "cJSON.h"

// OFF is handled by each transport before these helpers. Only a complete,
// explicit start replaces a job; configuration-only writes never start heating.
static inline bool shu1_explicit_job_start(const cJSON *root) {
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, "work_mode");
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "work_on")) &&
        cJSON_IsNumber(mode) && mode->valueint >= SHU1_MODE_AUTO &&
        mode->valueint <= SHU1_MODE_HEALTH_TEST;
}

static inline void shu1_finish_job_command(shu1_settings_t *s, const cJSON *root, int64_t now_ms) {
    if (!shu1_explicit_job_start(root)) return;
    s->work_on = true;
    const cJSON *minutes = cJSON_GetObjectItemCaseSensitive(root, "drying_duration_min");
    if (s->work_mode == SHU1_MODE_DRYING && s->drying_running && cJSON_IsNumber(minutes)) {
        int duration = minutes->valueint;
        if (duration < 1) duration = 1;
        if (duration > 1440) duration = 1440;
        s->drying_end_ms = now_ms + (int64_t)duration * 60000;
    }
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "tempering_start_now"))) {
        s->work_mode = SHU1_MODE_POWER_ON; // No printer-completion prerequisite.
        s->tempering_enabled = true;
        int start = s->target_temp_c;
        if (start < 0) start = 0;
        if (start > CONFIG_SHU1_MAX_TARGET_TEMP_C) start = CONFIG_SHU1_MAX_TARGET_TEMP_C;
        int end = s->tempering_end_temp_c;
        if (end < 0) end = 0;
        if (end > start) end = start;
        int duration = s->tempering_duration_min;
        if (duration < 1) duration = 1;
        if (duration > 240) duration = 240;
        s->tempering_start_temp_c = start;
        s->tempering_current_target_c = start;
        s->tempering_end_temp_c = end;
        s->tempering_duration_min = duration;
        s->tempering_start_ms = now_ms;
        s->tempering_end_ms = now_ms + (int64_t)duration * 60000;
        s->tempering_phase = SHU1_TEMPERING_ACTIVE;
        s->tempering_complete_pending = false;
    }
}
