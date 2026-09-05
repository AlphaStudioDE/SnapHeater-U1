/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui

enum class HomeLayoutStyle(val label: String) {
    SimpleDaily("Simple"),
    BentoDashboard("Bento"),
    FocusDial("Focus dial");

    companion object {
        val Default = FocusDial

        fun fromPreference(value: String?): HomeLayoutStyle =
            entries.firstOrNull { it.name == value } ?: Default
    }
}
