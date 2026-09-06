/*
 * SnapHeater U1 — Copyright (c) 2026 Damian Borkowski — SPDX-License-Identifier: MIT
 */
package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.material.icons.outlined.*
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.components.actionIcon
import androidx.compose.material.icons.Icons

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.ui.labelRes
import com.alphastudio.snapheateru1.ui.components.ScreenColumn

@Composable
fun DashboardScreen(
    snapshot: HeaterSnapshot,
    telemetryFresh: Boolean = false,
    onStart: () -> Unit = {},
    onSafeStop: () -> Unit = {},
    stopPending: Boolean = false,
) {
    val stopped = snapshot.mode == AppMode.SafeStop
    ScreenColumn {
        Text(stringResource(R.string.daily_title), style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold)
        Text(stringResource(R.string.daily_intro), color = MaterialTheme.colorScheme.onSurfaceVariant)
        Card(shape = RoundedCornerShape(24.dp), modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(stringResource(R.string.dashboard_chamber), style = MaterialTheme.typography.labelLarge)
                Text(if (telemetryFresh) "${snapshot.chamberC} °C" else "— °C",
                    fontSize = 64.sp, fontWeight = FontWeight.Bold)
                if (telemetryFresh && !stopped && !stopPending) {
                    Text(stringResource(R.string.modes_target, snapshot.targetC),
                        color = MaterialTheme.colorScheme.primary)
                }
                HorizontalDivider()
                if (stopPending) {
                    Text(stringResource(R.string.heating_stopping), style = MaterialTheme.typography.titleLarge)
                } else if (telemetryFresh) {
                    Text(stringResource(snapshot.mode.labelRes()), style = MaterialTheme.typography.titleLarge)
                    Text(stringResource(if (snapshot.fanOn && stopped) R.string.heating_cooling
                        else if (snapshot.fanOn) R.string.daily_fan_on else R.string.daily_fan_off),
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                } else Text(stringResource(R.string.daily_connection_warning))
            }
        }
        if (!snapshot.heaterOutputBuildEnabled) {
            Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.secondaryContainer)) {
                Column(Modifier.fillMaxWidth().padding(20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(stringResource(R.string.daily_blocked), fontWeight = FontWeight.Bold)
                    Text(stringResource(if (!snapshot.heaterOutputBuildEnabled)
                        R.string.modes_block_build else R.string.modes_block_latch))
                }
            }
        }
        Button(onClick = onStart, modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp)) {
            ActionLabel(Icons.Outlined.Widgets, stringResource(R.string.daily_choose))
        }
        OutlinedButton(onClick = onSafeStop, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) {
            ActionLabel(Icons.Outlined.PowerSettingsNew, stringResource(R.string.daily_stop))
        }
        Text(stringResource(R.string.daily_cooling_note), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}
