/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

void shu1_event_log_init(void);
void shu1_event_log_add(const char *level, const char *code, const char *message);
cJSON *shu1_event_log_to_json(void);
uint32_t shu1_event_log_count(void);

#ifdef __cplusplus
}
#endif
