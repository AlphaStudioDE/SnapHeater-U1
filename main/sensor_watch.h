/* SnapHeater U1 — passive sensor diagnostics. SPDX-License-Identifier: MIT */
#pragma once
#include "ntc.h"
#include <math.h>

/* Engineering bounds, not experimentally validated thermal constants.
 * No probe pulses: this monitor can only remove permission to heat.
 * Raw invariance is suspicion, not proof of a broken thermistor. */
#define SHU1_SAMPLE_MAX_AGE_US INT64_C(1500000)
#define SHU1_RAW_STILL_WINDOW_US INT64_C(60000000)
#define SHU1_RAW_WARNING_GRACE_US INT64_C(300000000)
#define SHU1_RAW_MIN_OUTPUT_TIME_US INT64_C(30000000)

typedef enum { SHU1_SAMPLE_HEALTHY, SHU1_SAMPLE_STALE,
               SHU1_SAMPLE_INVALID, SHU1_SAMPLE_FROZEN, SHU1_SAMPLE_WARNING } shu1_sample_health_t;

typedef struct {
    uint32_t sequence;
    int raw[2], suspect_raw[2];
    int64_t previous_us, still_us[2], on_us[2], warning_us;
    unsigned suspect_mask;
    bool tracking, frozen;
} shu1_sensor_watch_t;

static inline const char *shu1_sensor_warning_code(unsigned mask) {
    switch (mask) {
        case 1: return "sensor_freeze_warning_chamber";
        case 2: return "sensor_freeze_warning_ptc";
        case 3: return "sensor_freeze_warning_both";
        default: return "sensor_freeze_warning";
    }
}

static inline bool shu1_sample_fresh(shu1_sensor_watch_t *w,
    const shu1_sensor_sample_t *s, int64_t attempt_us, int64_t now_us) {
    bool fresh = s->sequence != 0 && s->sequence != w->sequence &&
        s->started_us >= attempt_us && s->completed_us >= s->started_us &&
        now_us >= s->completed_us && now_us - s->started_us <= SHU1_SAMPLE_MAX_AGE_US;
    if (fresh) w->sequence = s->sequence;
    return fresh;
}

static inline shu1_sample_health_t shu1_sensor_watch_step(shu1_sensor_watch_t *w,
    const shu1_sensor_sample_t *s, bool fresh, bool heating_job,
    bool applied_ssr_on, int64_t now_us) {
    if (!fresh) { w->tracking = false; return SHU1_SAMPLE_STALE; }
    if (s->chamber_status != SHU1_SENSOR_OK || s->ptc_status != SHU1_SENSOR_OK ||
        !isfinite(s->chamber_instant_c) || !isfinite(s->ptc_instant_c) ||
        s->chamber_raw <= (int)SHU1_NTC_RAW_SHORT_MAX || s->chamber_raw >= (int)SHU1_NTC_RAW_OPEN_MIN ||
        s->ptc_raw <= (int)SHU1_NTC_RAW_SHORT_MAX || s->ptc_raw >= (int)SHU1_NTC_RAW_OPEN_MIN) {
        w->tracking = false;
        return SHU1_SAMPLE_INVALID;
    }
    const int raw[2] = { s->chamber_raw, s->ptc_raw };
    if (w->frozen || w->warning_us) {
        // Only activity in the SUSPECT channel may remove its suspicion.
        // Movement in the other channel cannot hide a stuck reading.
        for (unsigned ch = 0; ch < 2; ++ch)
            if (raw[ch] != w->suspect_raw[ch]) w->suspect_mask &= ~(1U << ch);
        if (w->suspect_mask == 0) {
            w->frozen = false; w->warning_us = 0; w->tracking = false;
        } else if (w->frozen || now_us < w->warning_us ||
                   now_us - w->warning_us >= SHU1_RAW_WARNING_GRACE_US) {
            // A pause or acknowledgement cannot extend a pending deadline.
            w->frozen = true;
            return SHU1_SAMPLE_FROZEN;
        }
    } else {
        w->suspect_mask = 0; // explicit OFF may have cancelled a warning
    }
    int64_t dt = now_us - w->previous_us;
    const bool interval_ok = w->tracking && dt > 0 && dt <= SHU1_SAMPLE_MAX_AGE_US;
    for (unsigned ch = 0; ch < 2; ++ch) {
        if (!heating_job || !interval_ok || raw[ch] != w->raw[ch]) {
            w->raw[ch] = raw[ch]; w->still_us[ch] = now_us; w->on_us[ch] = 0;
        } else {
            // Last applied logical SSR output, not the upcoming command.
            // Continuous ON must also be diagnosable; no PWM/cycle count needed.
            if (applied_ssr_on && w->on_us[ch] < SHU1_RAW_MIN_OUTPUT_TIME_US)
                w->on_us[ch] += dt;
            if (now_us - w->still_us[ch] >= SHU1_RAW_STILL_WINDOW_US &&
                w->on_us[ch] >= SHU1_RAW_MIN_OUTPUT_TIME_US && !(w->suspect_mask & (1U << ch))) {
                w->suspect_mask |= 1U << ch;
                w->suspect_raw[ch] = raw[ch];
                if (!w->warning_us) w->warning_us = now_us;
            }
        }
    }
    w->tracking = heating_job;
    w->previous_us = now_us;
    return w->warning_us ? SHU1_SAMPLE_WARNING : SHU1_SAMPLE_HEALTHY;
}
