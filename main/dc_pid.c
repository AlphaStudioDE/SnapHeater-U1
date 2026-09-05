/* dragon-core dc_pid v0.32.0, MIT; adapted only for local component layout. */
#include "dc_pid.h"

#include <math.h>
#include <string.h>

static float clampf(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static bool config_valid(const dc_pid_config_t *c) {
    return c && isfinite(c->kp) && isfinite(c->ki) && isfinite(c->kd) &&
           c->kp >= 0.0f && c->ki >= 0.0f && c->kd >= 0.0f &&
           isfinite(c->derivative_alpha) && c->derivative_alpha >= 0.0f &&
           c->derivative_alpha <= 1.0f &&
           (c->kd == 0.0f || c->derivative_alpha > 0.0f) &&
           isfinite(c->output_min) && isfinite(c->output_max) &&
           c->output_min < c->output_max && isfinite(c->integral_min) &&
           isfinite(c->integral_max) && c->integral_min <= c->integral_max &&
           c->integral_min <= 0.0f && c->integral_max >= 0.0f;
}

static bool state_valid(const dc_pid_state_t *state) {
    return state && isfinite(state->integral) &&
           (!state->initialized || (isfinite(state->prev_measurement) &&
                                    isfinite(state->derivative_filtered)));
}

void dc_pid_reset(dc_pid_state_t *state) {
    if (state) memset(state, 0, sizeof(*state));
}

bool dc_pid_step(dc_pid_state_t *state, const dc_pid_config_t *config,
                 float setpoint, float measurement, float dt_s,
                 bool integrate, dc_pid_result_t *result) {
    if (result) memset(result, 0, sizeof(*result));
    if (!state || !result) return false;
    if (!state_valid(state)) {
        dc_pid_reset(state);
        return false;
    }
    if (!config_valid(config) || !isfinite(setpoint) || !isfinite(measurement) ||
        !isfinite(dt_s) || dt_s <= 0.0f) return false;

    const float error = setpoint - measurement;
    if (!isfinite(error)) return false;
    const float previous = state->initialized ? state->prev_measurement : measurement;
    float derivative = state->initialized ? state->derivative_filtered : 0.0f;
    if (config->kd > 0.0f) {
        const float measurement_delta = measurement - previous;
        if (!isfinite(measurement_delta)) return false;
        const float raw = -measurement_delta / dt_s;
        if (!isfinite(raw)) return false;
        derivative += config->derivative_alpha * (raw - derivative);
        if (!isfinite(derivative)) return false;
    } else {
        derivative = 0.0f;
    }

    const float p = config->kp * error;
    const float d = config->kd * derivative;
    if (!isfinite(p) || !isfinite(d)) return false;
    float integral = clampf(state->integral, config->integral_min, config->integral_max);
    if (integrate) {
        const float integral_delta = config->ki * error * dt_s;
        if (!isfinite(integral_delta)) return false;
        const float candidate_unclamped = integral + integral_delta;
        if (!isfinite(candidate_unclamped)) return false;
        const float candidate = clampf(candidate_unclamped,
                                       config->integral_min, config->integral_max);
        const float candidate_output = p + candidate + d;
        if (!isfinite(candidate_output)) return false;
        const bool pushes_high = candidate_output > config->output_max && error > 0.0f;
        const bool pushes_low = candidate_output < config->output_min && error < 0.0f;
        if (!pushes_high && !pushes_low) integral = candidate;
    }

    const float raw_output = p + integral + d;
    if (!isfinite(raw_output)) return false;
    state->integral = integral;
    state->prev_measurement = measurement;
    state->derivative_filtered = derivative;
    state->initialized = true;
    result->output = clampf(raw_output, config->output_min, config->output_max);
    result->p = p;
    result->i = integral;
    result->d = d;
    result->saturated_low = raw_output < config->output_min;
    result->saturated_high = raw_output > config->output_max;
    return true;
}
