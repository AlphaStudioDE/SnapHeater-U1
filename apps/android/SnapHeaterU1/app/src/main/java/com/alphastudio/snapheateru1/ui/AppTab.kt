/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Dashboard
import androidx.compose.material.icons.filled.FactCheck
import androidx.compose.material.icons.filled.Science
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Tune
import androidx.compose.ui.graphics.vector.ImageVector

enum class AppTab(val label: String, val icon: ImageVector) {
    Dashboard("Status", Icons.Filled.Dashboard),
    Modes("Modes", Icons.Filled.Tune),
    Safety("Safety", Icons.Filled.FactCheck),
    Diagnostics("Diag", Icons.Filled.Science),
    Settings("Settings", Icons.Filled.Settings),
}
