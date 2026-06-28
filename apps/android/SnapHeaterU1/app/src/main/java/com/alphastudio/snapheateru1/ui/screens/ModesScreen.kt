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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusRow
import com.alphastudio.snapheateru1.ui.detailRes
import com.alphastudio.snapheateru1.ui.labelRes
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun ModesScreen(
    snapshot: HeaterSnapshot,
    heatingAllowed: Boolean,
    safetyWarning: String,
    onMode: (AppMode) -> Unit,
    onSnapshotChange: (HeaterSnapshot) -> Unit,
    onConfirmSettings: (HeaterSnapshot) -> Unit,
) {
    ScreenColumn {
        SectionTitle(stringResource(R.string.modes_title), stringResource(R.string.modes_subtitle))
        if (!heatingAllowed) {
            Card(
                shape = RoundedCornerShape(8.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
            ) {
                Text(
                    safetyWarning,
                    color = StatusColors.Warning,
                    modifier = Modifier.fillMaxWidth().padding(14.dp),
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
        }

        AppMode.entries.forEach { mode ->
            val selected = snapshot.mode == mode
            val blocked = mode != AppMode.SafeStop && !heatingAllowed
            val modeLabel = stringResource(mode.labelRes())
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
                    Text(modeLabel, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    Text(stringResource(mode.detailRes()), color = MaterialTheme.colorScheme.onSurfaceVariant)
                    if (selected) {
                        ModeSettingsPanel(
                            snapshot = snapshot,
                            heatingAllowed = heatingAllowed,
                            onSnapshotChange = onSnapshotChange,
                            onConfirmSettings = onConfirmSettings,
                        )
                        Button(onClick = { onMode(AppMode.SafeStop) }, modifier = Modifier.fillMaxWidth()) {
                            Text(stringResource(R.string.mode_safe_stop))
                        }
                    } else {
                        OutlinedButton(
                            onClick = { onMode(mode) },
                            enabled = !blocked,
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            Text(stringResource(R.string.modes_select))
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
    heatingAllowed: Boolean,
    onSnapshotChange: (HeaterSnapshot) -> Unit,
    onConfirmSettings: (HeaterSnapshot) -> Unit,
) {
    val blocked = snapshot.mode != AppMode.SafeStop && !heatingAllowed
    val modeLabel = stringResource(snapshot.mode.labelRes())
    val pendingLabel = stringResource(R.string.common_pending)
    Column(
        verticalArrangement = Arrangement.spacedBy(10.dp),
        modifier = Modifier.fillMaxWidth().padding(top = 6.dp),
    ) {
        Text(stringResource(R.string.modes_mode_settings), style = MaterialTheme.typography.labelLarge, fontWeight = FontWeight.Bold)
        StatusRow(stringResource(R.string.label_saved_state), snapshot.lastConfirmedSettings, valueColor = confirmColor(snapshot, modeLabel))

        when (snapshot.mode) {
            AppMode.AutoStandby -> {
                StatusRow(stringResource(R.string.label_printer_awareness), stringResource(R.string.value_moonraker_readonly), valueColor = StatusColors.Warning)
                StatusRow(stringResource(R.string.label_material_profile), snapshot.material)
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings(pendingLabel))
                }
            }
            AppMode.ManualHold -> {
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings(pendingLabel))
                }
                ToggleRow(stringResource(R.string.label_fan_assist), snapshot.manualFanAssist) {
                    onSnapshotChange(snapshot.copy(manualFanAssist = it).pendingSettings(pendingLabel))
                }
                StatusRow(stringResource(R.string.label_runtime_guard), stringResource(R.string.value_enabled), valueColor = StatusColors.Good)
            }
            AppMode.Preheat -> {
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings(pendingLabel))
                }
                DurationSlider(stringResource(R.string.label_heat_soak), snapshot.preheatHeatSoakMin, 5f..45f) {
                    onSnapshotChange(snapshot.copy(preheatHeatSoakMin = it).pendingSettings(pendingLabel))
                }
                StatusRow(stringResource(R.string.label_start_condition), stringResource(R.string.value_manual_confirm), valueColor = StatusColors.Warning)
            }
            AppMode.Drying -> {
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings(pendingLabel))
                }
                DurationSlider(stringResource(R.string.label_drying_time), snapshot.dryingTimeMin, 30f..360f) {
                    onSnapshotChange(snapshot.copy(dryingTimeMin = it).pendingSettings(pendingLabel))
                }
                StatusRow(stringResource(R.string.label_profile), snapshot.material)
            }
            AppMode.Tempering -> {
                DurationSlider(stringResource(R.string.label_cooldown_time), snapshot.temperingDurationMin, 10f..180f) {
                    onSnapshotChange(snapshot.copy(temperingDurationMin = it).pendingSettings(pendingLabel))
                }
                TargetSlider(snapshot.targetC) {
                    onSnapshotChange(snapshot.copy(targetC = it).pendingSettings(pendingLabel))
                }
                StatusRow(stringResource(R.string.label_ramp_behavior), stringResource(R.string.value_controlled_cooldown), valueColor = StatusColors.Good)
            }
            AppMode.SafeStop -> {
                StatusRow(stringResource(R.string.label_heater), stringResource(R.string.value_off_locked), strong = true, valueColor = StatusColors.Good)
                StatusRow(stringResource(R.string.label_fan), stringResource(R.string.value_cooldown_allowed), valueColor = StatusColors.Warning)
                Text(
                    stringResource(R.string.modes_safe_stop_note),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Button(
            onClick = {
                onConfirmSettings(
                    snapshot.copy(lastConfirmedSettings = confirmedSummary(snapshot, modeLabel)),
                )
            },
            enabled = !blocked,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(stringResource(R.string.modes_confirm_settings))
        }
    }
}

@Composable
private fun TargetSlider(targetC: Int, onTarget: (Int) -> Unit) {
    Text(stringResource(R.string.modes_target, targetC), color = StatusColors.Warning, fontWeight = FontWeight.Bold)
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
    Text(stringResource(R.string.modes_duration, label, value), color = StatusColors.Normal, fontWeight = FontWeight.Bold)
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

private fun confirmColor(snapshot: HeaterSnapshot, modeLabel: String) =
    if (snapshot.lastConfirmedSettings.startsWith(modeLabel)) StatusColors.Good else StatusColors.Warning

private fun HeaterSnapshot.pendingSettings(pendingLabel: String) =
    copy(lastConfirmedSettings = pendingLabel)

private fun confirmedSummary(snapshot: HeaterSnapshot, modeLabel: String): String = when (snapshot.mode) {
    AppMode.AutoStandby -> "$modeLabel / ${snapshot.targetC} C"
    AppMode.ManualHold -> "$modeLabel / ${snapshot.targetC} C / fan ${if (snapshot.manualFanAssist) "on" else "off"}"
    AppMode.Preheat -> "$modeLabel / ${snapshot.targetC} C / soak ${snapshot.preheatHeatSoakMin} min"
    AppMode.Drying -> "$modeLabel / ${snapshot.targetC} C / ${snapshot.dryingTimeMin} min"
    AppMode.Tempering -> "$modeLabel / ${snapshot.temperingDurationMin} min / ${snapshot.targetC} C"
    AppMode.SafeStop -> "$modeLabel / outputs stopped"
}
