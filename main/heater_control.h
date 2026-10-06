/* SnapHeater U1 local policy based on DragonBreath v1.1.19 findings.
 * plastikman/DragonBreath f311a22c39a3e3aef92d10f0f6c3f2d5d63e02b9.
 * SPDX-License-Identifier: MIT */
#pragma once
#include <math.h>
#include <stdbool.h>
#define SHU1_HEATER_HYSTERESIS_C 1.0f
typedef struct { bool demand; } shu1_heater_control_t;

static inline void shu1_heater_control_reset(shu1_heater_control_t *s) {
    if (s) s->demand = false;
}

// No time-proportioned SSR pulses. Downstream safety can always cut immediately.
// This pure policy does not write GPIOs or change the Panda fan control.
static inline bool shu1_heater_control_step(shu1_heater_control_t *s,
    float target, float measured, bool inhibited, float *duty) {
    if (duty) *duty = 0.0f;
    if (!s || !duty || !isfinite(target) || !isfinite(measured) || target <= 0.0f) {
        shu1_heater_control_reset(s);
        return false;
    }
    if (measured >= target) s->demand = false;
    else if (measured < target - SHU1_HEATER_HYSTERESIS_C) s->demand = true;
    *duty = s->demand && !inhibited ? 1.0f : 0.0f;
    return true;
}

static inline const char *shu1_heater_control_constraint(bool ok, bool foldback,
    bool inhibited, float target, float measured, float duty) {
    if (!ok) return "control_error";
    if (foldback) return "element_foldback";
    if (inhibited) return "safety_inhibit";
    if (measured >= target) return "target_reached";
    if (duty == 0.0f) return "hysteresis_hold";
    return "none";
}
