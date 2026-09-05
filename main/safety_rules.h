/* Pure final safety governors, shared by production and host regression tests. */
#pragma once
#include <stdbool.h>

static inline bool shu1_safety_airflow(bool requested, bool heat_mode, bool faulted) {
    return requested || heat_mode || faulted;
}
static inline bool shu1_safety_heat_allowed(bool requested, bool maintenance,
                                           bool watchdog_armed, bool faulted) {
    return requested && !maintenance && watchdog_armed && !faulted;
}
