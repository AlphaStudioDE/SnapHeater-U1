/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
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
fun SafetyScreen(snapshot: HeaterSnapshot) {
    ScreenColumn {
        SectionTitle("Safety unlock", "Checklist mirror for the firmware-side staged validation process")

        SafetyStep("Firmware built with heater output disabled", true)
        SafetyStep("GPIO probe confirms expected pins", !snapshot.gpioProbeLocked)
        SafetyStep("Independent thermal cutoff installed", snapshot.setupValidationPassed)
        SafetyStep("Fan fail test completed", snapshot.setupValidationPassed)
        SafetyStep("Moonraker command scope reviewed", snapshot.moonraker.contains("read", ignoreCase = true))

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
                    "Heater output",
                    if (snapshot.heaterLocked) "Locked" else "Available",
                    strong = true,
                    valueColor = if (snapshot.heaterLocked) StatusColors.Good else StatusColors.Warning,
                )
                StatusRow("Readiness", "${snapshot.safetyScore}%", valueColor = safetyColor(snapshot.safetyScore))
                StatusRow("Setup validation", if (snapshot.setupValidationPassed) "Passed" else "Pending", valueColor = if (snapshot.setupValidationPassed) StatusColors.Good else StatusColors.Warning)
                StatusRow("Output latch", if (snapshot.outputSafetyLatchArmed) "Armed" else "Not armed", valueColor = if (snapshot.outputSafetyLatchArmed) StatusColors.Warning else StatusColors.Good)
                Text(
                    "This screen documents readiness only. Unlocking remains a firmware configuration and hardware validation decision.",
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
                StatusRow("Sensors verified", if (snapshot.setupValidationPassed) "Yes" else "No", valueColor = if (snapshot.setupValidationPassed) StatusColors.Good else StatusColors.Warning)
                StatusRow("Fan verified", if (snapshot.setupValidationPassed) "Yes" else "No", valueColor = if (snapshot.setupValidationPassed) StatusColors.Good else StatusColors.Warning)
                StatusRow("Heater verified", if (snapshot.heaterLocked) "Locked" else "User enabled", valueColor = if (snapshot.heaterLocked) StatusColors.Good else StatusColors.Warning)
                StatusRow("GPIO probe", if (snapshot.gpioProbeLocked) "Build locked" else "Probe build", valueColor = if (snapshot.gpioProbeLocked) StatusColors.Good else StatusColors.Warning)
                Text(
                    "The app should require explicit user acknowledgement before any future BLE/REST arm_output_safety_latch write.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
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
