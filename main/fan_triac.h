/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t edge_count;
    uint64_t rejected_edge_count;
    uint64_t signal_gap_count; // Accepted intervals exceeding the presence timeout.
    uint32_t last_period_us;
    uint32_t min_period_us;
    uint32_t max_period_us;
    int64_t last_edge_ms;
    bool signal_present;
} shu1_zero_cross_stats_t;

esp_err_t shu1_fan_triac_init(void);
esp_err_t shu1_fan_triac_preinit_off(void);
void shu1_fan_triac_set(bool on);
void shu1_fan_triac_force_off(void);
bool shu1_fan_triac_is_active(void);
bool shu1_fan_triac_is_running(void);
shu1_zero_cross_stats_t shu1_fan_triac_zero_cross_stats(void);

#ifdef __cplusplus
}
#endif
