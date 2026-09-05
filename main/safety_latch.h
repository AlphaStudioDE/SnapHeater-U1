/* DragonBreath-derived persistent hazardous-fault latch (MIT). */
#pragma once

#include <stdbool.h>
#include "app_state.h"
#include "esp_err.h"

esp_err_t shu1_safety_latch_init(void);
void shu1_safety_latch_inhibit(void);
bool shu1_safety_latch_is_inhibited(void);
bool shu1_safety_latch_is_set(void);
shu1_heater_fault_t shu1_safety_latch_fault(void);
// Control-task only: physically cuts SSR before any mutex/NVS operation.
esp_err_t shu1_safety_latch_trip(shu1_heater_fault_t fault);
/* Clearable, RAM-only user panic. Runtime failures use inhibit(), never this. */
void shu1_safety_latch_trip_volatile(shu1_heater_fault_t fault);
// Retry a failed NVS write. Rate-limited internally; safe to call every control loop.
esp_err_t shu1_safety_latch_retry_persist(void);
void shu1_safety_latch_request_clear(void);
bool shu1_safety_latch_clear_requested(void);
esp_err_t shu1_safety_latch_clear(void);
