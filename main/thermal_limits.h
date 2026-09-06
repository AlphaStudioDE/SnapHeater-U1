#pragma once
#include <math.h>

// Safety uses the hotter of calibrated and uncorrected table temperatures.
// Positive correction remains conservative; negative correction cannot delay cut.
static inline float shu1_safety_temperature(float calibrated, float offset) {
    if (!isfinite(calibrated) || !isfinite(offset)) return NAN;
    return calibrated - fminf(offset,0.0f);
}

static inline float shu1_foldback_limit(int rref, int requested) {
    const float board=rref==33 ? 99.0f : 102.0f;
    return requested<=0 ? board : fminf(board,fmaxf(90.0f,(float)requested));
}
