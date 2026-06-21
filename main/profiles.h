/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *shu1_profile_name(int profile);
int shu1_profile_from_name(const char *name);
bool shu1_apply_material_profile(shu1_settings_t *settings, int profile, bool apply_preheat_defaults);

#ifdef __cplusplus
}
#endif
