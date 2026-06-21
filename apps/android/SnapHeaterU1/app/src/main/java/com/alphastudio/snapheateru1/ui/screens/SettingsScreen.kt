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

@Composable
fun SettingsScreen(snapshot: HeaterSnapshot, onTarget: (Int) -> Unit) {
    ScreenColumn {
        SectionTitle("Settings", "Local app preferences and mock target controls")

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
                StatusRow("Material profile", snapshot.material)
                StatusRow("Maximum UI target", "70 C")
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
                StatusRow("Local-only mode", "Enabled", strong = true)
                androidx.compose.foundation.layout.Row(
                    horizontalArrangement = Arrangement.SpaceBetween,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text("Push telemetry")
                    Switch(checked = false, onCheckedChange = null)
                }
                Text(
                    "Cloud features are intentionally out of scope for the first app shell.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
