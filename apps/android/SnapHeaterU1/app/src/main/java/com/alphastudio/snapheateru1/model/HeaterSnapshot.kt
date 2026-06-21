/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.model

data class HeaterSnapshot(
    val chamberC: Int = 31,
    val ptcC: Int = 38,
    val targetC: Int = 45,
    val safetyScore: Int = 62,
    val heaterLocked: Boolean = true,
    val gpioProbeLocked: Boolean = true,
    val fanOn: Boolean = false,
    val moonraker: String = "Read-only / waiting",
    val ble: String = "Advertising",
    val material: String = "PETG",
    val mode: AppMode = AppMode.AutoStandby,
)
