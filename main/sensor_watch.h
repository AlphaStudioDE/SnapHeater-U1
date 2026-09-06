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
#define SHU1_RAW_MIN_TRANSITIONS 6U

typedef enum { SHU1_SAMPLE_HEALTHY, SHU1_SAMPLE_STALE,
               SHU1_SAMPLE_INVALID, SHU1_SAMPLE_FROZEN, SHU1_SAMPLE_WARNING } shu1_sample_health_t;

typedef struct {
    uint32_t sequence;
    int chamber_raw, ptc_raw;
    int64_t previous_us, still_us, on_us, off_us, warning_us;
    unsigned transitions;
    bool tracking, previous_on, frozen, chamber_recovered, ptc_recovered;
} shu1_sensor_watch_t;

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
    if (w->frozen) {
        /* OFF/pause does not erase suspicion. Both raw channels must show
         * change before the existing explicit fault-clear path may be used.
         * This is evidence of activity, not proof of accurate measurement. */
        w->chamber_recovered |= s->chamber_raw != w->chamber_raw;
        w->ptc_recovered |= s->ptc_raw != w->ptc_raw;
        if (!w->chamber_recovered || !w->ptc_recovered) return SHU1_SAMPLE_FROZEN;
        w->frozen = false;
        w->tracking = false;
    }
    if (w->warning_us) {
        if (s->chamber_raw != w->chamber_raw || s->ptc_raw != w->ptc_raw) {
            w->warning_us = 0; w->tracking = false;
        } else {
            // A pause, a polling gap or phone acknowledgement cannot extend this deadline.
            if (now_us - w->warning_us >= SHU1_RAW_WARNING_GRACE_US) {
                w->frozen = true; w->chamber_recovered = w->ptc_recovered = false;
                return SHU1_SAMPLE_FROZEN;
            }
            return SHU1_SAMPLE_WARNING;
        }
    }
    if (!heating_job) { w->tracking = false; return SHU1_SAMPLE_HEALTHY; }
    int64_t dt = now_us - w->previous_us;
    if (!w->tracking || dt <= 0 || dt > SHU1_SAMPLE_MAX_AGE_US ||
        s->chamber_raw != w->chamber_raw || s->ptc_raw != w->ptc_raw) {
        w->tracking = true;
        w->chamber_raw = s->chamber_raw; w->ptc_raw = s->ptc_raw;
        w->still_us = now_us; w->on_us = w->off_us = 0; w->transitions = 0;
    } else {
        /* The supplied SSR state is the last applied logical output, held
         * during the interval just measured, not a future PID request. */
        if (applied_ssr_on) {
            if (w->on_us < SHU1_RAW_MIN_OUTPUT_TIME_US) w->on_us += dt;
        } else if (w->off_us < SHU1_RAW_MIN_OUTPUT_TIME_US) w->off_us += dt;
        if (applied_ssr_on != w->previous_on && w->transitions < SHU1_RAW_MIN_TRANSITIONS)
            ++w->transitions;
        if (now_us - w->still_us >= SHU1_RAW_STILL_WINDOW_US &&
            w->on_us >= SHU1_RAW_MIN_OUTPUT_TIME_US &&
            w->off_us >= SHU1_RAW_MIN_OUTPUT_TIME_US &&
            w->transitions >= SHU1_RAW_MIN_TRANSITIONS) {
            w->warning_us = now_us;
        }
    }
    w->previous_us = now_us; w->previous_on = applied_ssr_on;
    return w->warning_us ? SHU1_SAMPLE_WARNING : SHU1_SAMPLE_HEALTHY;
}
