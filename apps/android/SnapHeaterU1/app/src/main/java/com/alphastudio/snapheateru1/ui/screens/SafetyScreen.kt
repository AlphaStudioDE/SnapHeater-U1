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
        SafetyStep("Independent thermal cutoff installed", false)
        SafetyStep("Fan fail test completed", false)
        SafetyStep("Moonraker command scope reviewed", false)

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
                Text(
                    "This screen documents readiness only. Unlocking remains a firmware configuration and hardware validation decision.",
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
