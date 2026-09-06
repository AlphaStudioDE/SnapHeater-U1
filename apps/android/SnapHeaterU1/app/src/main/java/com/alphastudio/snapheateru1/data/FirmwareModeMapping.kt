/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.AppMode
import org.json.JSONObject

// AUTO variants share the existing firmware controller; only the finish policy differs.
fun JSONObject.withAutoFinishPolicy(mode: AppMode): JSONObject = apply {
    if (mode == AppMode.AutoStandby || mode == AppMode.AutoStandbyTempering) {
        val temper = mode == AppMode.AutoStandbyTempering
        put("tempering_enabled", temper)
        put("finish_conditioning_mode", if (temper) 2 else 1)
        put("tempering_end_temp", 35) // Ramp to 35 C; firmware stops heating at the deadline.
        put("cancel_tempering", true)
        put("preheat_running", false)
        put("isrunning", false)
    }
}

fun AppMode.toFirmwareWorkMode(): Int = when (this) {
    AppMode.AutoStandby, AppMode.AutoStandbyTempering -> 1
    AppMode.ManualHold -> 2
    AppMode.Preheat -> 4
    AppMode.Drying -> 3
    AppMode.Tempering -> 1
    AppMode.SafeStop -> 0
}

fun firmwareModeToAppMode(
    workMode: Int,
    preheatRunning: Boolean,
    dryingRunning: Boolean,
    temperingPhase: Int,
    workOn: Boolean? = null,
    temperingEnabled: Boolean = false,
    finishConditioningMode: Int = 1,
): AppMode = when {
    workOn == false -> AppMode.SafeStop
    preheatRunning -> AppMode.Preheat
    dryingRunning -> AppMode.Drying
    temperingPhase == 1 || temperingPhase == 2 -> AppMode.Tempering
    workMode == 2 -> AppMode.ManualHold
    workMode == 0 -> AppMode.SafeStop
    workMode == 1 && temperingEnabled && finishConditioningMode == 2 -> AppMode.AutoStandbyTempering
    else -> AppMode.AutoStandby
}
