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
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun DashboardScreen(snapshot: HeaterSnapshot) {
    ScreenColumn {
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                MetricTile("Chamber", "${snapshot.chamberC} C", "Current enclosure temperature", chamberColor(snapshot))
                MetricTile("Target", "${snapshot.targetC} C", "Active target limit", StatusColors.Warning)
            }
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                MetricTile("PTC", "${snapshot.ptcC} C", "Heater body sensor", ptcColor(snapshot.ptcC))
                MetricTile("Safety", "${snapshot.safetyScore}%", "Validation readiness", safetyColor(snapshot.safetyScore))
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
                StatusRow("Moonraker", snapshot.moonraker, valueColor = StatusColors.Warning)
                StatusRow("Fan", if (snapshot.fanOn) "On" else "Off", valueColor = if (snapshot.fanOn) StatusColors.Good else StatusColors.Normal)
                StatusRow(
                    "Heater output",
                    if (snapshot.heaterLocked) "Locked" else "Available",
                    strong = true,
                    valueColor = if (snapshot.heaterLocked) StatusColors.Good else StatusColors.Warning,
                )
                LinearProgressIndicator(
                    progress = { snapshot.safetyScore / 100f },
                    modifier = Modifier.fillMaxWidth(),
                    color = safetyColor(snapshot.safetyScore),
                    trackColor = MaterialTheme.colorScheme.outlineVariant,
                )
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
                SectionTitle("Printer and chamber intelligence", "Read-only U1 context with warning-first decisions")
                StatusRow("Printer", snapshot.printerState, valueColor = StatusColors.Normal)
                StatusRow("Progress", "${snapshot.printProgressPct}%")
                StatusRow("Active material", snapshot.activeMaterial)
                StatusRow("Warm-up ETA", "${snapshot.warmupEtaMin} min", valueColor = StatusColors.Warning)
                StatusRow("Heat soak", if (snapshot.heatSoakReady) "Ready" else "Waiting", valueColor = if (snapshot.heatSoakReady) StatusColors.Good else StatusColors.Warning)
                StatusRow("Stability", "${snapshot.stabilityScore}%", valueColor = safetyColor(snapshot.stabilityScore))
                StatusRow("Print risk", "${snapshot.printRiskScore}% / ${snapshot.printRiskMessage}", valueColor = riskColor(snapshot.printRiskScore))
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
                SectionTitle("Post-print and service", "Firmware feature state that the app must surface")
                StatusRow("Virtual door", if (snapshot.virtualDoorOpen) "Open detected" else "No event", valueColor = if (snapshot.virtualDoorOpen) StatusColors.Warning else StatusColors.Good)
                StatusRow("Filter life used", "${snapshot.filterLifePct}%", valueColor = serviceColor(snapshot.filterLifePct))
                StatusRow("Heater wear", "${snapshot.heaterWearPct}%", valueColor = serviceColor(snapshot.heaterWearPct))
                StatusRow("Session energy", "${snapshot.sessionEnergyWh} Wh")
                StatusRow("Estimated total", "${snapshot.estimatedEnergyWh} Wh")
            }
        }

        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            StatusPill(snapshot.ble, StatusColors.Normal)
            StatusPill(
                if (snapshot.gpioProbeLocked) "GPIO probe locked" else "GPIO probe passed",
                if (snapshot.gpioProbeLocked) StatusColors.Warning else StatusColors.Good,
            )
            StatusPill("Local control only", StatusColors.Normal)
        }

        Text(
            "The app cannot unlock heating by itself. Firmware safety gates and hardware validation remain authoritative.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontWeight = FontWeight.Medium,
        )
    }
}

private fun chamberColor(snapshot: HeaterSnapshot) = when {
    snapshot.chamberC >= snapshot.targetC - 2 && snapshot.chamberC <= snapshot.targetC + 2 -> StatusColors.Good
    snapshot.chamberC > snapshot.targetC + 5 -> StatusColors.Warning
    else -> StatusColors.Normal
}

private fun ptcColor(ptcC: Int) = when {
    ptcC >= 90 -> StatusColors.Danger
    ptcC >= 75 -> StatusColors.Warning
    else -> StatusColors.Normal
}

private fun safetyColor(score: Int) = when {
    score >= 80 -> StatusColors.Good
    score >= 50 -> StatusColors.Warning
    else -> StatusColors.Danger
}

private fun riskColor(score: Int) = when {
    score >= 70 -> StatusColors.Danger
    score >= 40 -> StatusColors.Warning
    else -> StatusColors.Good
}

private fun serviceColor(percent: Int) = when {
    percent >= 90 -> StatusColors.Danger
    percent >= 70 -> StatusColors.Warning
    else -> StatusColors.Good
}
