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
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.shape.RoundedCornerShape
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.MetricTile
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusPill
import com.alphastudio.snapheateru1.ui.components.StatusRow

@Composable
fun DashboardScreen(snapshot: HeaterSnapshot) {
    ScreenColumn {
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                MetricTile("Chamber", "${snapshot.chamberC} C", "Current enclosure temperature")
                MetricTile("Target", "${snapshot.targetC} C", "Active target limit", MaterialTheme.colorScheme.secondary)
            }
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                MetricTile("PTC", "${snapshot.ptcC} C", "Heater body sensor", MaterialTheme.colorScheme.tertiary)
                MetricTile("Safety", "${snapshot.safetyScore}%", "Validation readiness")
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                SectionTitle("Live state", "Mock data until BLE is validated on real hardware")
                StatusRow("BLE", snapshot.ble, strong = true)
                StatusRow("Moonraker", snapshot.moonraker)
                StatusRow("Fan", if (snapshot.fanOn) "On" else "Off")
                StatusRow("Heater output", if (snapshot.heaterLocked) "Locked" else "Available", strong = true)
                LinearProgressIndicator(
                    progress = { snapshot.safetyScore / 100f },
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            StatusPill(if (snapshot.gpioProbeLocked) "GPIO probe locked" else "GPIO probe passed")
            StatusPill("Local control only", MaterialTheme.colorScheme.primary)
        }

        Text(
            "The app cannot unlock heating by itself. Firmware safety gates and hardware validation remain authoritative.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontWeight = FontWeight.Medium,
        )
    }
}
