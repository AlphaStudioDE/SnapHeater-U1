#pragma once
#include <stdbool.h>
#include <math.h>

// Printer ventilation only. NEVER apply this curve to the Panda TRIAC fan.
static inline int shu1_symbiont_curve(bool *cooling, float temperature, float target) {
    if (!isfinite(temperature) || !isfinite(target) || target <= 0) {
        *cooling = false;
        return 0;
    }
    float delta = temperature - target;
    if (delta <= 2) *cooling = false;
    else if (delta > 5) *cooling = true;
    if (!*cooling) return 0;
    float speed = delta >= 7 ? 100 : delta >= 5 ? 50 + (delta-5)*25 :
                  delta >= 3 ? 30 + (delta-3)*10 : 30;
    return (int)lroundf(speed);
}
