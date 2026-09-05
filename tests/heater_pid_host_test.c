#include <assert.h>
#include <string.h>
#include "heater_pid.h"
#include "dc_pid.c"
int main(void) {
    shu1_pid_state_t s = {0};
    float duty = -1;
    assert(shu1_pid_approach_cap(0) == 0);
    assert(shu1_pid_approach_cap(1.9f) == 0.4f);
    assert(shu1_pid_approach_cap(2) == 0.7f);
    assert(shu1_pid_approach_cap(5) == 1);
    // Synthetic measurements at a 55 C target, NOT a thermal plant simulation.
    for (int i = 0; i < 3600; ++i) {
        float measured = 25.0f + 30.0f * (float)i / 3599.0f;
        assert(shu1_pid_step(&s, 55, measured, true, &duty));
        assert(isfinite(duty) && duty >= 0 && duty <= shu1_pid_approach_cap(55-measured));
    }
    assert(duty == 0);
    float saved = s.controller.integral;
    assert(shu1_pid_step(&s, 55, 54, false, &duty));
    assert(s.controller.integral <= saved); // no windup behind inhibited output
    assert(!shu1_pid_step(&s, 55, NAN, true, &duty) && duty == 0);
    assert(!shu1_pid_step(&s, INFINITY, 50, true, &duty) && duty == 0);
    assert(!shu1_pid_window_on(&s, NAN, 0));
    assert(!shu1_pid_window_on(&s, INFINITY, 0));
    shu1_pid_reset(&s);
    assert(!shu1_pid_window_on(&s, 0, 1) && !s.window_initialized);
    assert(shu1_pid_window_on(&s, 0.4f, 1000000));
    assert(shu1_pid_window_on(&s, 0.4f, 4999999));
    assert(!shu1_pid_window_on(&s, 0.4f, 5000000));
    assert(shu1_pid_window_on(&s, 0.4f, 11000000));
    assert(!shu1_pid_window_on(&s, 0, 11000001));
    shu1_pid_reset(&s);
    assert(shu1_pid_step_timed(&s, 55, 54, true, 1000000, &duty));
    saved = s.controller.integral;
    for (int i = 1; i < 500; ++i) {
        assert(shu1_pid_step_timed(&s, 55, 54, true, 1000000+i*1000, &duty));
        assert(s.controller.integral == saved);
    }
    assert(shu1_pid_step_timed(&s, 55, 54, true, 1500000, &duty));
    assert(s.controller.integral > saved);
    assert(shu1_pid_step_timed(&s, 50, 54, true, 1500001, &duty) && duty == 0);
    assert(shu1_pid_step_timed(&s, 55, 54, false, 1, &duty)); // timer reset
    assert(s.last_step_us == 1);
    assert(!strcmp(shu1_pid_constraint(false, true, 55, 54, 0), "pid_error"));
    assert(!strcmp(shu1_pid_constraint(true, true, 55, 54, 0), "element_foldback"));
    assert(!strcmp(shu1_pid_constraint(true, false, 55, 55, 0), "target_reached"));
    assert(!strcmp(shu1_pid_constraint(true, false, 55, 54, .4f), "approach_limit"));
    assert(!strcmp(shu1_pid_constraint(true, false, 55, 54, .1f), "none"));
    return 0;
}
