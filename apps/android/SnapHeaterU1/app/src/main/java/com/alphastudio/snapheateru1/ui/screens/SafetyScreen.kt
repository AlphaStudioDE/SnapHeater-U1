/*
 * SnapHeater U1 — Copyright (c) 2026 Damian Borkowski — SPDX-License-Identifier: MIT
 */
package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn

@Composable
fun SafetyScreen(snapshot: HeaterSnapshot, onApplySafety: (HeaterSnapshot, Boolean, Boolean) -> Unit) {
    ScreenColumn {
        Text(stringResource(R.string.safety_title), style = MaterialTheme.typography.headlineMedium)
        Text(stringResource(R.string.automatic_safety_help))
        if (!snapshot.heaterOutputBuildEnabled) Text(stringResource(R.string.modes_block_build))
        Text(stringResource(R.string.daily_cooling_note))
    }
}
