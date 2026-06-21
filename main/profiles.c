/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

#include "profiles.h"
#include "app_config.h"
#include <string.h>
#include <strings.h>
#include <stddef.h>

typedef struct {
    int id;
    const char *name;
    int chamber_target_c;
    int preheat_hold_min;
    int drying_mode;
    int drying_temp_c;
    int drying_timer_h;
} shu1_profile_def_t;

static const shu1_profile_def_t g_profiles[] = {
    {SHU1_PROFILE_CUSTOM, "custom", CONFIG_SHU1_TARGET_TEMP_C, 15, SHU1_DRYING_CUSTOM, 50, 12},
    {SHU1_PROFILE_PLA,    "PLA",    45, 15, SHU1_DRYING_PLA,    55, 12},
    {SHU1_PROFILE_PETG,   "PETG",   55, 20, SHU1_DRYING_PETG,   60, 12},
    {SHU1_PROFILE_ABS,    "ABS",    60, 30, SHU1_DRYING_ABS,    60, 12},
    {SHU1_PROFILE_ASA,    "ASA",    60, 30, SHU1_DRYING_ABS,    60, 12},
    {SHU1_PROFILE_TPU,    "TPU",    40, 15, SHU1_DRYING_CUSTOM, 45, 8},
    {SHU1_PROFILE_NYLON,  "NYLON",  60, 45, SHU1_DRYING_CUSTOM, 60, 12},
    {SHU1_PROFILE_PC,     "PC",     60, 45, SHU1_DRYING_CUSTOM, 60, 12},
};

static int clamp_target(int v) {
    if (v < 0) return 0;
    if (v > CONFIG_SHU1_MAX_TARGET_TEMP_C) return CONFIG_SHU1_MAX_TARGET_TEMP_C;
    return v;
}

static const shu1_profile_def_t *find_profile(int profile) {
    for (size_t i = 0; i < sizeof(g_profiles) / sizeof(g_profiles[0]); ++i) {
        if (g_profiles[i].id == profile) return &g_profiles[i];
    }
    return &g_profiles[0];
}

const char *shu1_profile_name(int profile) {
    return find_profile(profile)->name;
}

int shu1_profile_from_name(const char *name) {
    if (!name || !*name) return SHU1_PROFILE_CUSTOM;
    for (size_t i = 0; i < sizeof(g_profiles) / sizeof(g_profiles[0]); ++i) {
        if (strcasecmp(name, g_profiles[i].name) == 0) return g_profiles[i].id;
    }
    return SHU1_PROFILE_CUSTOM;
}

bool shu1_apply_material_profile(shu1_settings_t *s, int profile, bool apply_preheat_defaults) {
    if (!s) return false;
    const shu1_profile_def_t *p = find_profile(profile);
    s->material_profile = p->id;
    s->target_temp_c = clamp_target(p->chamber_target_c);
    s->drying_mode = p->drying_mode;
    s->custom_temp_c = clamp_target(p->drying_temp_c);
    s->custom_timer_h = p->drying_timer_h;
    if (apply_preheat_defaults) {
        s->preheat_target_temp_c = clamp_target(p->chamber_target_c);
        s->preheat_hold_min = p->preheat_hold_min;
    }
    return true;
}
