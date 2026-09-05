/*
 * Generic PID controller from dragon-core v0.32.0 (MIT).
 * Source: https://github.com/justinh-rahb/dragon-core/tree/v0.32.0/components/dc_pid
 */
#pragma once

#include <stdbool.h>

typedef struct {
    float kp;
    float ki;
    float kd;
    float derivative_alpha;
    float output_min;
    float output_max;
    float integral_min;
    float integral_max;
} dc_pid_config_t;

typedef struct {
    float integral;
    float prev_measurement;
    float derivative_filtered;
    bool initialized;
} dc_pid_state_t;

typedef struct {
    float output;
    float p;
    float i;
    float d;
    bool saturated_low;
    bool saturated_high;
} dc_pid_result_t;

void dc_pid_reset(dc_pid_state_t *state);
bool dc_pid_step(dc_pid_state_t *state, const dc_pid_config_t *config,
                 float setpoint, float measurement, float dt_s,
                 bool integrate, dc_pid_result_t *result);
