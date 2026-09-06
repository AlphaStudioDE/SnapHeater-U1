/* SnapHeater U1 — output-runtime estimates, not electrical measurements. */
#pragma once
#include "app_state.h"
#include <math.h>

static inline void shu1_usage_accumulate(shu1_runtime_t *rt, int64_t dt, int64_t now_ms) {
    if (dt <= 0 || dt > 5000) return;
    if (rt->heater_output_on && rt->chamber_sensor_status == SHU1_SENSOR_OK &&
        isfinite(rt->chamber_instant_temp_c) && rt->chamber_instant_temp_c >= 35.0f &&
        rt->last_sensor_ms > 0 && now_ms >= rt->last_sensor_ms && now_ms - rt->last_sensor_ms <= 1500)
        rt->heater_usage_35c_ms += (uint64_t)dt;
    // Filtration also occurs during warm-up and cooldown, below 35 C.
    if (rt->fan_output_on) rt->fan_on_accum_ms += (uint64_t)dt;
}
