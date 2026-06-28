/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.screens

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
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusRow
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun SettingsScreen(
    snapshot: HeaterSnapshot,
    onTarget: (Int) -> Unit,
    onSnapshotChange: (HeaterSnapshot) -> Unit,
    onApplySettings: (HeaterSnapshot) -> Unit,
) {
    ScreenColumn {
        SectionTitle("Settings", "Feature controls mirroring firmware settings and future BLE writes")

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text("Temperature target", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                Text("${snapshot.targetC} C", style = MaterialTheme.typography.headlineSmall, color = MaterialTheme.colorScheme.primary)
                Slider(
                    value = snapshot.targetC.toFloat(),
                    onValueChange = { onTarget(it.toInt()) },
                    valueRange = 30f..70f,
                    steps = 39,
                )
                StatusRow("Material profile", snapshot.material, valueColor = StatusColors.Normal)
                StatusRow("Active recipe", snapshot.activeRecipeName)
                StatusRow("Maximum UI target", "70 C")
            }
        }

        SettingsCard("Material and print intelligence") {
            ToggleRow("Auto material profile", snapshot.autoMaterialProfileEnabled) {
                onSnapshotChange(snapshot.copy(autoMaterialProfileEnabled = it))
            }
            ToggleRow("Profile mismatch warning", snapshot.mismatchWarningEnabled) {
                onSnapshotChange(snapshot.copy(mismatchWarningEnabled = it))
            }
            ToggleRow("PLA protection", snapshot.plaProtectionEnabled) {
                onSnapshotChange(snapshot.copy(plaProtectionEnabled = it))
            }
            ToggleRow("Print risk score", snapshot.startPrintWarningEnabled) {
                onSnapshotChange(snapshot.copy(startPrintWarningEnabled = it))
            }
            StatusRow("Current risk", "${snapshot.printRiskScore}%", valueColor = riskColor(snapshot.printRiskScore))
            StatusRow("Advice", snapshot.materialAdvice)
        }

        SettingsCard("Chamber intelligence") {
            ToggleRow("Heat soak / stability lock", snapshot.heatSoakEnabled) {
                onSnapshotChange(snapshot.copy(heatSoakEnabled = it))
            }
            ToggleRow("Anti-warp", snapshot.antiWarpEnabled) {
                onSnapshotChange(snapshot.copy(antiWarpEnabled = it))
            }
            ToggleRow("Large print protection", snapshot.largePrintProtectionEnabled) {
                onSnapshotChange(snapshot.copy(largePrintProtectionEnabled = it))
            }
            ToggleRow("Safe overnight", snapshot.safeOvernightEnabled) {
                onSnapshotChange(snapshot.copy(safeOvernightEnabled = it))
            }
            ToggleRow("Pause hold", snapshot.pauseHoldEnabled) {
                onSnapshotChange(snapshot.copy(pauseHoldEnabled = it))
            }
            ToggleRow("Smart resume", snapshot.smartResumeEnabled) {
                onSnapshotChange(snapshot.copy(smartResumeEnabled = it))
            }
        }

        SettingsCard("Post-print and service") {
            ToggleRow("Virtual door detection", snapshot.virtualDoorDetectionEnabled) {
                onSnapshotChange(snapshot.copy(virtualDoorDetectionEnabled = it))
            }
            ToggleRow("Airflow detection", snapshot.airflowDetectionEnabled) {
                onSnapshotChange(snapshot.copy(airflowDetectionEnabled = it))
            }
            ToggleRow("Temperature history", snapshot.tempHistoryEnabled) {
                onSnapshotChange(snapshot.copy(tempHistoryEnabled = it))
            }
            ToggleRow("Incident reports", snapshot.incidentReportEnabled) {
                onSnapshotChange(snapshot.copy(incidentReportEnabled = it))
            }
            ToggleRow("Local recipes", snapshot.localRecipesEnabled) {
                onSnapshotChange(snapshot.copy(localRecipesEnabled = it))
            }
            ToggleRow("Scheduled preheat", snapshot.scheduledPreheatEnabled) {
                onSnapshotChange(snapshot.copy(scheduledPreheatEnabled = it))
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
                Text("Connectivity and productization", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                ToggleRow("Local-only mode", snapshot.localOnlyMode) {
                    onSnapshotChange(snapshot.copy(localOnlyMode = it))
                }
                ToggleRow("Firmware demo mode", snapshot.demoModeEnabled) {
                    onSnapshotChange(snapshot.copy(demoModeEnabled = it))
                }
                ToggleRow("Showcase mode", snapshot.showcaseModeEnabled) {
                    onSnapshotChange(snapshot.copy(showcaseModeEnabled = it))
                }
                ToggleRow("U1 Symbiont Mode", snapshot.symbiontModeEnabled) {
                    onSnapshotChange(snapshot.copy(symbiontModeEnabled = it))
                }
                ToggleRow("Symbiont ventilation", snapshot.symbiontVentilationAllowed) {
                    onSnapshotChange(snapshot.copy(symbiontVentilationAllowed = it))
                }
                Text(
                    "Cloud features stay out of scope. Future writes should go through local BLE or LAN REST only.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Button(
            onClick = { onApplySettings(snapshot) },
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Apply settings")
        }
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
