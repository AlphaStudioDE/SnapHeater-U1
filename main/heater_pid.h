/*
 * SnapHeater U1 local controller policy based on DragonBreath findings.
 * Adapted policy attribution retained; not a clean-room implementation.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "dc_pid.h"

#define SHU1_PID_KP         0.1000f
#define SHU1_PID_KI         0.0010f
#define SHU1_PID_KD         0.0400f
#define SHU1_PID_D_ALPHA    0.20f
#define SHU1_PID_DT_S       0.50f
#define SHU1_PID_WINDOW_US  10000000LL

typedef struct {
    dc_pid_state_t controller;
    int64_t window_start_us;
    bool window_initialized;
    int64_t last_step_us;
    bool step_initialized;
    float cached_duty;
} shu1_pid_state_t;

static inline void shu1_pid_reset(shu1_pid_state_t *state) {
    if (!state) return;
    dc_pid_reset(&state->controller);
    state->window_start_us = 0;
    state->window_initialized = false;
    state->last_step_us = 0;
    state->step_initialized = false;
    state->cached_duty = 0.0f;
}

static inline float shu1_pid_approach_cap(float error_c) {
    if (error_c <= 0.0f) return 0.0f;
    if (error_c < 2.0f) return 0.40f;
    if (error_c < 5.0f) return 0.70f;
    return 1.0f;
}

static inline bool shu1_pid_step(shu1_pid_state_t *state, float target_c,
                                 float measurement_c, bool integrate, float *duty) {
    if (duty) *duty = 0.0f;
    if (!state || !duty) return false;
    float error = target_c - measurement_c;
    float cap = shu1_pid_approach_cap(error);
    float output_max = cap > 0.0f ? cap : 1.0f;
    float integral_max = output_max - SHU1_PID_KP * error;
    if (integral_max < 0.0f) integral_max = 0.0f;
    if (integral_max > 1.0f) integral_max = 1.0f;
    const dc_pid_config_t config = {
        .kp = SHU1_PID_KP, .ki = SHU1_PID_KI, .kd = SHU1_PID_KD,
        .derivative_alpha = SHU1_PID_D_ALPHA,
        .output_min = 0.0f, .output_max = output_max,
        .integral_min = 0.0f, .integral_max = integral_max,
    };
    dc_pid_result_t result;
    if (!dc_pid_step(&state->controller, &config, target_c, measurement_c,
                    SHU1_PID_DT_S, integrate, &result)) return false;
    if (measurement_c < target_c) *duty = result.output;
    return true;
}

static inline bool shu1_pid_window_on(shu1_pid_state_t *state, float duty,
                                      int64_t now_us) {
    if (!state || !isfinite(duty) || duty <= 0.0f) return false;
    if (duty > 1.0f) duty = 1.0f;
    if (!state->window_initialized || now_us < state->window_start_us) {
        state->window_start_us = now_us;
        state->window_initialized = true;
    }
    int64_t elapsed = now_us - state->window_start_us;
    if (elapsed >= SHU1_PID_WINDOW_US) {
        state->window_start_us += (elapsed / SHU1_PID_WINDOW_US) * SHU1_PID_WINDOW_US;
        elapsed = now_us - state->window_start_us;
    }
    return elapsed < (int64_t)(duty * (float)SHU1_PID_WINDOW_US);
}

// OFF notifications may wake the safety loop faster than 500 ms. They must
// still cut power immediately, but must not accelerate the fixed-dt integrator.
static inline bool shu1_pid_step_timed(shu1_pid_state_t *state, float target,
                                       float measurement, bool integrate,
                                       int64_t now_us, float *duty) {
    if (duty) *duty = 0.0f;
    if (!state || !duty || !isfinite(target) || !isfinite(measurement)) return false;
    if (state->step_initialized && now_us < state->last_step_us) shu1_pid_reset(state);
    if (!state->step_initialized || now_us - state->last_step_us >= 500000LL) {
        if (!shu1_pid_step(state, target, measurement, integrate, &state->cached_duty))
            return false;
        state->last_step_us = now_us;
        state->step_initialized = true;
    }
    // A changed target/measurement may REMOVE demand between PID samples.
    *duty = fminf(state->cached_duty, shu1_pid_approach_cap(target - measurement));
    return true;
}

// Diagnostic only: a zero SSR phase is not the same as zero admitted duty.
static inline const char *shu1_pid_constraint(bool ok, bool foldback,
                                             float target, float measurement,
                                             float duty) {
    if (!ok) return "pid_error";
    if (foldback) return "element_foldback";
    if (measurement >= target) return "target_reached";
    float cap = shu1_pid_approach_cap(target - measurement);
    if (cap < 1.0f && duty >= cap - 0.0005f) return "approach_limit";
    return "none";
}
