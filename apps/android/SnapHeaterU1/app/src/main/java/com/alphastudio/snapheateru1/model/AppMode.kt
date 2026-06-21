/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.model

enum class AppMode(val label: String, val detail: String) {
    AutoStandby("Auto standby", "Printer-aware chamber mode"),
    ManualHold("Manual hold", "Local target hold"),
    Preheat("Preheat", "Warm chamber before print"),
    Drying("Drying", "Material drying profile"),
    Tempering("Tempering", "Post-print controlled cooldown"),
    SafeStop("Safe stop", "Stop heating workflow"),
}
