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
// Synchronous backend retained for host tests; runtime MUST use defer_trip below.
esp_err_t shu1_safety_latch_trip(shu1_heater_fault_t fault);
/* Clearable, RAM-only user panic. Runtime failures use inhibit(), never this. */
void shu1_safety_latch_trip_volatile(shu1_heater_fault_t fault);
// Storage-worker only. Rate-limited internally; NEVER call from the control loop.
esp_err_t shu1_safety_latch_retry_persist(void);
void shu1_safety_latch_request_clear(void);
// One-shot attempt: an unsafe request must not become a deferred automatic clear.
bool shu1_safety_latch_take_clear_request(void);
esp_err_t shu1_safety_latch_clear(void);
// Runtime control path: bounded RAM transitions; persistence belongs to storage worker.
void shu1_safety_latch_defer_trip(shu1_heater_fault_t fault);
void shu1_safety_latch_defer_clear(void);
void shu1_safety_latch_service(void);
const char *shu1_safety_latch_clear_block_reason(const shu1_settings_t *settings,const shu1_runtime_t *runtime);
