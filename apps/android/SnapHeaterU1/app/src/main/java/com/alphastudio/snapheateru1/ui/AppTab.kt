/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.FactCheck
import androidx.compose.material.icons.filled.Dashboard
import androidx.compose.material.icons.filled.Science
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Tune
import androidx.annotation.StringRes
import androidx.compose.ui.graphics.vector.ImageVector
import com.alphastudio.snapheateru1.R

enum class AppTab(@StringRes val labelRes: Int, val icon: ImageVector) {
    Dashboard(R.string.snapheater_status, Icons.Filled.Dashboard),
    Modes(R.string.snapheater_modes, Icons.Filled.Tune),
    Safety(R.string.snapheater_safety, Icons.AutoMirrored.Filled.FactCheck),
    Diagnostics(R.string.tab_diag, Icons.Filled.Science),
    Settings(R.string.snapheater_settings, Icons.Filled.Settings),
}
