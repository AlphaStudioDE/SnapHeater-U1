/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusRow
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun ModesScreen(
    snapshot: HeaterSnapshot,
    onMode: (AppMode) -> Unit,
    onSnapshotChange: (HeaterSnapshot) -> Unit,
) {
    ScreenColumn {
        SectionTitle("Modes", "Choose a workflow. Unsafe outputs stay blocked in mock and locked firmware states.")

        AppMode.entries.forEach { mode ->
            val selected = snapshot.mode == mode
            Card(
                shape = RoundedCornerShape(8.dp),
                colors = CardDefaults.cardColors(
                    containerColor = if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface,
                ),
            ) {
                Column(
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth().padding(14.dp),
                ) {
                    Text(mode.label, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    Text(mode.detail, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    if (selected) {
                        ModeSettingsPanel(
                            snapshot = snapshot,
                            onSnapshotChange = onSnapshotChange,
                        )
                        Button(onClick = { onMode(AppMode.SafeStop) }, modifier = Modifier.fillMaxWidth()) {
                            Text("Safe stop")
                        }
                    } else {
                        OutlinedButton(onClick = { onMode(mode) }, modifier = Modifier.fillMaxWidth()) {
                            Text("Select")
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ModeSettingsPanel(
    snapshot: HeaterSnapshot,
    onSnapshotChange: (HeaterSnapshot) -> Unit,
) {
    Column(
        verticalArrangement = Arrangement.spacedBy(10.dp),
        modifier = Modifier.fillMaxWidth().padding(top = 6.dp),
    ) {
        Text("Mode settings", style = MaterialTheme.typography.labelLarge, fontWeight = FontWeight.Bold)
        StatusRow("Saved state", snapshot.lastConfirmedSettings, valueColor = confirmColor(snapshot))

        when (snapshot.mode) {
            AppMode.AutoStandby -> {
                StatusRow("Printer awareness", "Moonraker read-only", valueColor = StatusColors.Warning)
                StatusRow("Material profile", snapshot.material)
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings())
                }
            }
            AppMode.ManualHold -> {
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings())
                }
                ToggleRow("Fan assist", snapshot.manualFanAssist) {
                    onSnapshotChange(snapshot.copy(manualFanAssist = it).pendingSettings())
                }
                StatusRow("Runtime guard", "Enabled", valueColor = StatusColors.Good)
            }
            AppMode.Preheat -> {
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings())
                }
                DurationSlider("Heat soak", snapshot.preheatHeatSoakMin, 5f..45f) {
                    onSnapshotChange(snapshot.copy(preheatHeatSoakMin = it).pendingSettings())
                }
                StatusRow("Start condition", "Manual confirm", valueColor = StatusColors.Warning)
            }
            AppMode.Drying -> {
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings())
                }
                DurationSlider("Drying time", snapshot.dryingTimeMin, 30f..360f) {
                    onSnapshotChange(snapshot.copy(dryingTimeMin = it).pendingSettings())
                }
                StatusRow("Profile", snapshot.material)
            }
            AppMode.Tempering -> {
                DurationSlider("Cooldown time", snapshot.temperingDurationMin, 10f..180f) {
                    onSnapshotChange(snapshot.copy(temperingDurationMin = it).pendingSettings())
                }
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings())
                }
                StatusRow("Ramp behavior", "Controlled cooldown", valueColor = StatusColors.Good)
            }
            AppMode.SafeStop -> {
                StatusRow("Heater", "Off / locked", strong = true, valueColor = StatusColors.Good)
                StatusRow("Fan", "Cooldown allowed", valueColor = StatusColors.Warning)
                Text(
                    "Safe stop keeps the UI in a recovery state until the next mode is selected.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Button(
            onClick = {
                onSnapshotChange(
                    snapshot.copy(lastConfirmedSettings = confirmedSummary(snapshot)),
                )
            },
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Confirm settings")
        }
    }
}

@Composable
private fun TargetSlider(targetC: Int, onTarget: (Int) -> Unit) {
    Text("Target: $targetC C", color = StatusColors.Warning, fontWeight = FontWeight.Bold)
    Slider(
        value = targetC.toFloat(),
        onValueChange = { onTarget(it.toInt()) },
        valueRange = 30f..70f,
        steps = 39,
    )
}

@Composable
private fun DurationSlider(
    label: String,
    value: Int,
    range: ClosedFloatingPointRange<Float>,
    onValue: (Int) -> Unit,
) {
    Text("$label: $value min", color = StatusColors.Normal, fontWeight = FontWeight.Bold)
    Slider(
        value = value.toFloat(),
        onValueChange = { onValue(it.toInt()) },
        valueRange = range,
        steps = ((range.endInclusive - range.start) / 5f).toInt().coerceAtLeast(0),
    )
}

@Composable
private fun ToggleRow(label: String, checked: Boolean, onChecked: (Boolean) -> Unit) {
    Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
        Text(label)
        Switch(checked = checked, onCheckedChange = onChecked)
    }
}

private fun confirmColor(snapshot: HeaterSnapshot) =
    if (snapshot.lastConfirmedSettings.startsWith(snapshot.mode.label)) StatusColors.Good else StatusColors.Warning

private fun HeaterSnapshot.pendingSettings() =
    copy(lastConfirmedSettings = "Pending ${mode.label} settings")

private fun confirmedSummary(snapshot: HeaterSnapshot): String = when (snapshot.mode) {
    AppMode.AutoStandby -> "${snapshot.mode.label} confirmed / target ${snapshot.targetC} C"
    AppMode.ManualHold -> "${snapshot.mode.label} confirmed / target ${snapshot.targetC} C / fan ${if (snapshot.manualFanAssist) "on" else "off"}"
    AppMode.Preheat -> "${snapshot.mode.label} confirmed / target ${snapshot.targetC} C / soak ${snapshot.preheatHeatSoakMin} min"
    AppMode.Drying -> "${snapshot.mode.label} confirmed / target ${snapshot.targetC} C / ${snapshot.dryingTimeMin} min"
    AppMode.Tempering -> "${snapshot.mode.label} confirmed / ${snapshot.temperingDurationMin} min / target ${snapshot.targetC} C"
    AppMode.SafeStop -> "${snapshot.mode.label} confirmed / heater locked"
}
