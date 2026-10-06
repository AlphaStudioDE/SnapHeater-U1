/* Passive warm-up advisory, not a fan/RPM sensor. SPDX-License-Identifier: MIT */
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"

typedef struct {
    int64_t start_ms, previous_ms, on_ms;
    float start_chamber_c;
    bool tracking;
} shu1_airflow_watch_t;
typedef enum { SHU1_AIRFLOW_OBSERVING, SHU1_AIRFLOW_NO_ANOMALY,
               SHU1_AIRFLOW_SUSPECT } shu1_airflow_result_t;

static inline shu1_airflow_result_t shu1_airflow_watch_step(shu1_airflow_watch_t *w,
    bool warmup, bool applied_ssr_on, float chamber_c, float ptc_c, int64_t now_ms) {
    if (!warmup || !isfinite(chamber_c) || !isfinite(ptc_c)) {
        w->tracking = false;
        return SHU1_AIRFLOW_OBSERVING;
    }
    int64_t dt = now_ms - w->previous_ms;
    if (!w->tracking || dt <= 0 || dt > 1500) {
        *w = (shu1_airflow_watch_t){ .tracking = true, .start_ms = now_ms,
            .previous_ms = now_ms, .start_chamber_c = chamber_c };
        return SHU1_AIRFLOW_OBSERVING;
    }
    w->previous_ms = now_ms;
    // Last applied output represents the interval just measured. OFF phases
    // do not reset the observation window or its accumulated heating evidence.
    if (applied_ssr_on) w->on_ms += dt;
    if (now_ms - w->start_ms < SHU1_AIRFLOW_WINDOW_MS) return SHU1_AIRFLOW_OBSERVING;
    shu1_airflow_result_t result = SHU1_AIRFLOW_OBSERVING;
    if (w->on_ms >= 30000) {
        result = ptc_c - chamber_c >= SHU1_AIRFLOW_PTC_DELTA_C &&
            chamber_c - w->start_chamber_c < SHU1_AIRFLOW_MIN_CHAMBER_RISE_C ?
            SHU1_AIRFLOW_SUSPECT : SHU1_AIRFLOW_NO_ANOMALY;
    }
    w->start_ms = now_ms;
    w->start_chamber_c = chamber_c;
    w->on_ms = 0;
    return result;
}
