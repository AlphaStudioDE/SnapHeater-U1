/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.material.icons.outlined.*
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.components.actionIcon
import androidx.compose.material.icons.Icons

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusRow
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun SettingsScreen(
    snapshot: HeaterSnapshot,
    statusText: String,
    busy: Boolean,
    onBack: () -> Unit,
    onTarget: (Int) -> Unit,
    onSnapshotChange: (HeaterSnapshot) -> Unit,
    onVirtualDoorDetectionChange: (Boolean) -> Unit,
    onApplySettings: (HeaterSnapshot) -> Unit,
    onSchedule: (HeaterSnapshot) -> Unit,
    restProvisionEnabled: Boolean,
    onProvisionRest: (String) -> Unit,
) {
    ScreenColumn {
        SectionTitle(stringResource(R.string.settings_title), stringResource(R.string.settings_subtitle))

        OutlinedButton(onClick = onBack) { ActionLabel(Icons.AutoMirrored.Outlined.ArrowBack, stringResource(R.string.settings_title)) }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text(stringResource(R.string.settings_temperature_target), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                Text("${snapshot.targetC} C", style = MaterialTheme.typography.headlineSmall, color = MaterialTheme.colorScheme.primary)
                Slider(
                    value = snapshot.targetC.coerceIn(30, 55).toFloat(),
                    onValueChange = { onTarget(it.toInt()) },
                    valueRange = 30f..55f,
                    steps = 24,
                )
                StatusRow(stringResource(R.string.label_material_profile), snapshot.material, valueColor = StatusColors.Normal)
                StatusRow(stringResource(R.string.settings_max_ui_target), "55 °C")
            }
        }

        SettingsCard(stringResource(R.string.settings_material_print)) {
            ToggleRow(stringResource(R.string.settings_auto_material_profile), snapshot.autoMaterialProfileEnabled) {
                onSnapshotChange(snapshot.copy(autoMaterialProfileEnabled = it))
            }
            ToggleRow(stringResource(R.string.settings_profile_mismatch), snapshot.mismatchWarningEnabled) {
                onSnapshotChange(snapshot.copy(mismatchWarningEnabled = it))
            }
            ToggleRow(stringResource(R.string.settings_pla_protection), snapshot.plaProtectionEnabled) {
                onSnapshotChange(snapshot.copy(plaProtectionEnabled = it))
            }
            ToggleRow(stringResource(R.string.settings_print_risk_score), snapshot.startPrintWarningEnabled) {
                onSnapshotChange(snapshot.copy(startPrintWarningEnabled = it))
            }
            StatusRow(stringResource(R.string.settings_current_risk), "${snapshot.printRiskScore}%", valueColor = riskColor(snapshot.printRiskScore))
            StatusRow(stringResource(R.string.settings_advice), snapshot.materialAdvice)
        }

        SettingsCard(stringResource(R.string.settings_chamber)) {
            ToggleRow(stringResource(R.string.settings_heat_soak_stability), snapshot.heatSoakEnabled) {
                onSnapshotChange(snapshot.copy(heatSoakEnabled = it))
            }
            ToggleRow(stringResource(R.string.settings_pause_hold), snapshot.pauseHoldEnabled) {
                onSnapshotChange(snapshot.copy(pauseHoldEnabled = it))
            }
        }

        SettingsCard(stringResource(R.string.settings_post_service)) {
            com.alphastudio.snapheateru1.ui.components.RestAccessCard(restProvisionEnabled, onProvisionRest)
            ToggleRow(stringResource(R.string.settings_virtual_door), snapshot.virtualDoorDetectionEnabled) {
                onVirtualDoorDetectionChange(it)
            }
            ToggleRow(stringResource(R.string.settings_airflow_detection), snapshot.airflowDetectionEnabled) {
                onSnapshotChange(snapshot.copy(airflowDetectionEnabled = it))
            }
            ToggleRow(stringResource(R.string.settings_temperature_history), snapshot.tempHistoryEnabled) {
                onSnapshotChange(snapshot.copy(tempHistoryEnabled = it))
            }
            ToggleRow(stringResource(R.string.settings_incident_reports), snapshot.incidentReportEnabled) {
                onSnapshotChange(snapshot.copy(incidentReportEnabled = it))
            }
            ToggleRow(stringResource(R.string.settings_scheduled_preheat), snapshot.scheduledPreheatEnabled) {
                onSnapshotChange(snapshot.copy(scheduledPreheatEnabled = it))
            }
            if (snapshot.scheduledPreheatEnabled) {
                Text(stringResource(R.string.schedule_delay, snapshot.scheduledPreheatDelayMin))
                Slider(value = snapshot.scheduledPreheatDelayMin.coerceIn(1, 1440).toFloat(),
                    onValueChange = { onSnapshotChange(snapshot.copy(scheduledPreheatDelayMin = it.toInt())) }, valueRange = 1f..1440f)
                Text(stringResource(R.string.schedule_target, snapshot.scheduledPreheatTargetC))
                Slider(value = snapshot.scheduledPreheatTargetC.coerceIn(30, 55).toFloat(),
                    onValueChange = { onSnapshotChange(snapshot.copy(scheduledPreheatTargetC = it.toInt())) }, valueRange = 30f..55f, steps = 24)
                Text(stringResource(R.string.schedule_hold, snapshot.scheduledPreheatHoldMin))
                Slider(value = snapshot.scheduledPreheatHoldMin.coerceIn(1, 240).toFloat(),
                    onValueChange = { onSnapshotChange(snapshot.copy(scheduledPreheatHoldMin = it.toInt())) }, valueRange = 1f..240f)
            }
            OutlinedButton(enabled = !busy, onClick = { onSchedule(snapshot) }) {
                ActionLabel(Icons.Outlined.Schedule, stringResource(if (snapshot.scheduledPreheatEnabled) R.string.schedule_confirm else R.string.schedule_cancel))
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text(stringResource(R.string.settings_connectivity), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                ToggleRow(stringResource(R.string.settings_local_only_mode), snapshot.localOnlyMode) {
                    onSnapshotChange(snapshot.copy(localOnlyMode = it))
                }
                Text(stringResource(R.string.ventilation_title))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(enabled = !busy, onClick = {
                        onSnapshotChange(snapshot.copy(symbiontModeEnabled = false, symbiontVentilationAllowed = false))
                    }) {
                        ActionLabel(if (!snapshot.symbiontModeEnabled) Icons.Outlined.Check else Icons.Outlined.Autorenew, "Auto")
                    }
                    OutlinedButton(enabled = !busy, onClick = {
                        onSnapshotChange(snapshot.copy(symbiontModeEnabled = true, symbiontVentilationAllowed = true))
                    }) {
                        ActionLabel(if (snapshot.symbiontModeEnabled) Icons.Outlined.Check else Icons.Outlined.Air, "Symbiont")
                    }
                }
                Text(stringResource(R.string.ventilation_help), style = MaterialTheme.typography.bodySmall)
                Text(stringResource(when (snapshot.ventilationStatus) {
                    "verified" -> R.string.ventilation_verified
                    "correcting", "verifying" -> R.string.ventilation_pending
                    "", "auto_or_idle" -> R.string.ventilation_inactive
                    else -> R.string.ventilation_unavailable
                }), style = MaterialTheme.typography.bodySmall)
                Text(
                    stringResource(R.string.settings_cloud_note),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Button(
            enabled = !busy,
            onClick = { onApplySettings(snapshot) },
            modifier = Modifier.fillMaxWidth(),
        ) {
            ActionLabel(Icons.Outlined.Check, stringResource(R.string.settings_apply))
        }
        Text(statusText, style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
private fun SettingsCard(title: String, content: @Composable ColumnScope.() -> Unit) {
    Card(
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
    ) {
        Column(
            verticalArrangement = Arrangement.spacedBy(10.dp),
            modifier = Modifier.fillMaxWidth().padding(14.dp),
        ) {
            Text(title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
            content()
        }
    }
}

@Composable
private fun ToggleRow(label: String, checked: Boolean, onChecked: (Boolean) -> Unit) {
    Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
        Text(label)
        Switch(checked = checked, onCheckedChange = onChecked)
    }
}

private fun riskColor(score: Int) = when {
    score >= 70 -> StatusColors.Danger
    score >= 40 -> StatusColors.Warning
    else -> StatusColors.Good
}
