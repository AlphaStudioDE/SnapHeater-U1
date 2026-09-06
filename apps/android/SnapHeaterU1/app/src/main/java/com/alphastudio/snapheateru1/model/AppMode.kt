/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.model

enum class AppMode {
    AutoStandby,
    AutoStandbyTempering,
    ManualHold,
    Preheat,
    Drying,
    Tempering,
    SafeStop,
}

fun AppMode.requiresPrinter(): Boolean =
    this == AppMode.AutoStandby || this == AppMode.AutoStandbyTempering
