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
import androidx.compose.material3.Checkbox
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.MaterialTheme
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
fun SafetyScreen(
    snapshot: HeaterSnapshot,
    onApplySafety: (HeaterSnapshot, Boolean, Boolean) -> Unit,
) {
    val verificationReady = snapshot.heaterOutputVerified && snapshot.fanOutputVerified && snapshot.sensorsVerified
    val latchPrerequisitesReady = verificationReady && (!snapshot.moonrakerVerified || snapshot.setupValidationPassed)

    ScreenColumn {
        SectionTitle("Runtime safety", "Firmware build follows the accepted Panda Breath pin map")

        SafetyStep("Firmware built with accepted Panda Breath GPIO", snapshot.hardwareMapName == "panda_breath_accepted")
        SafetyStep("Heater output build-enabled", snapshot.heaterOutputBuildEnabled)
        SafetyStep("TRIAC fan driver uses zero-cross input", snapshot.fanTriacControl && snapshot.zeroCrossGpio == 7)
        SafetyStep("Runtime safety latch ready", snapshot.outputSafetyLatchReady)
        SafetyStep("Moonraker command scope reviewed", snapshot.moonrakerVerified)

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text("Current gate", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow(
                    "Heater build",
                    if (snapshot.heaterOutputBuildEnabled) "Enabled" else "Disabled",
                    strong = true,
                    valueColor = if (snapshot.heaterOutputBuildEnabled) StatusColors.Warning else StatusColors.Good,
                )
                StatusRow("Hardware map", snapshot.hardwareMapName, valueColor = StatusColors.Good)
                StatusRow("Readiness", "${snapshot.safetyScore}%", valueColor = safetyColor(snapshot.safetyScore))
                StatusRow("Setup validation", if (snapshot.setupValidationPassed) "Passed" else "Pending", valueColor = if (snapshot.setupValidationPassed) StatusColors.Good else StatusColors.Warning)
                StatusRow("Output latch", if (snapshot.outputSafetyLatchArmed) "Armed" else "Not armed", valueColor = if (snapshot.outputSafetyLatchArmed) StatusColors.Warning else StatusColors.Good)
                StatusRow("Latch ready", if (snapshot.outputSafetyLatchReady) "Ready" else "Blocked", valueColor = if (snapshot.outputSafetyLatchReady) StatusColors.Good else StatusColors.Warning)
                Text(
                    "This screen mirrors firmware readiness. Build output is enabled, but normal heating still depends on runtime sensor, fault and latch state.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text("Output Safety Latch workflow", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow("Sensors verified", yesNo(snapshot.sensorsVerified), valueColor = verifyColor(snapshot.sensorsVerified))
                StatusRow("Fan driver", if (snapshot.fanTriacControl) "GPIO${snapshot.fanGpio} + ZC GPIO${snapshot.zeroCrossGpio}" else "Plain GPIO${snapshot.fanGpio}", valueColor = if (snapshot.fanTriacControl) StatusColors.Good else StatusColors.Warning)
                StatusRow("Fan output verified", yesNo(snapshot.fanOutputVerified), valueColor = verifyColor(snapshot.fanOutputVerified))
                StatusRow("Heater output", "GPIO${snapshot.heaterGpio}", valueColor = if (snapshot.heaterOutputVerified) StatusColors.Good else StatusColors.Warning)
                StatusRow("Heater output verified", yesNo(snapshot.heaterOutputVerified), valueColor = verifyColor(snapshot.heaterOutputVerified))
                StatusRow("Moonraker verified", yesNo(snapshot.moonrakerVerified), valueColor = verifyColor(snapshot.moonrakerVerified))
                StatusRow("Arm prerequisites", if (latchPrerequisitesReady) "Ready" else "Incomplete", valueColor = if (latchPrerequisitesReady) StatusColors.Good else StatusColors.Warning)
                StatusRow("GPIO probe API", if (snapshot.gpioProbeLocked) "Disabled" else "Enabled", valueColor = if (snapshot.gpioProbeLocked) StatusColors.Good else StatusColors.Warning)
                Text(
                    "Use these confirmations only after physical inspection on the real device. The app stores verification flags first, then arms the runtime latch in a separate action.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )

                SafetyAction("Confirm sensors inspected", snapshot.sensorsVerified) {
                    onApplySafety(snapshot.copy(sensorsVerified = true), false, false)
                }
                SafetyAction("Confirm fan output inspected", snapshot.fanOutputVerified) {
                    onApplySafety(snapshot.copy(fanOutputVerified = true), false, false)
                }
                SafetyAction("Confirm heater output inspected", snapshot.heaterOutputVerified) {
                    onApplySafety(snapshot.copy(heaterOutputVerified = true), false, false)
                }
                SafetyAction("Confirm Moonraker read-only path", snapshot.moonrakerVerified) {
                    onApplySafety(snapshot.copy(moonrakerVerified = true), false, false)
                }

                Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
                    Button(
                        onClick = { onApplySafety(snapshot, true, false) },
                        enabled = latchPrerequisitesReady && !snapshot.outputSafetyLatchArmed,
                        modifier = Modifier.weight(1f),
                    ) {
                        Text("Arm latch")
                    }
                    OutlinedButton(
                        onClick = { onApplySafety(snapshot, false, true) },
                        enabled = snapshot.outputSafetyLatchArmed,
                        modifier = Modifier.weight(1f),
                    ) {
                        Text("Disarm")
                    }
                }
            }
        }
    }
}

@Composable
private fun SafetyAction(label: String, done: Boolean, onClick: () -> Unit) {
    OutlinedButton(
        onClick = onClick,
        enabled = !done,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Text(if (done) "$label: done" else label)
    }
}

@Composable
private fun SafetyStep(label: String, checked: Boolean) {
    Card(
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
    ) {
        androidx.compose.foundation.layout.Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            modifier = Modifier.fillMaxWidth().padding(10.dp),
        ) {
            Checkbox(checked = checked, onCheckedChange = null)
            Text(label, modifier = Modifier.weight(1f), color = if (checked) StatusColors.Good else StatusColors.Warning)
        }
    }
}

private fun safetyColor(score: Int) = when {
    score >= 80 -> StatusColors.Good
    score >= 50 -> StatusColors.Warning
    else -> StatusColors.Danger
}

private fun yesNo(value: Boolean) = if (value) "Yes" else "No"

private fun verifyColor(value: Boolean) = if (value) StatusColors.Good else StatusColors.Warning
