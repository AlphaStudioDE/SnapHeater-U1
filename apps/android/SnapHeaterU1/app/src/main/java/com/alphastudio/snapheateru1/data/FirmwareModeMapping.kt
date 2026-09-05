/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.AppMode

fun AppMode.toFirmwareWorkMode(): Int = when (this) {
    AppMode.AutoStandby -> 1
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
): AppMode = when {
    preheatRunning -> AppMode.Preheat
    dryingRunning -> AppMode.Drying
    temperingPhase == 1 || temperingPhase == 2 -> AppMode.Tempering
    workMode == 2 -> AppMode.ManualHold
    workMode == 0 -> AppMode.SafeStop
    else -> AppMode.AutoStandby
}
