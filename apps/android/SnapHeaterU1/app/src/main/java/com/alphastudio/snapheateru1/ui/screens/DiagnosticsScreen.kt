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
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.ble.SnapHeaterBleContract
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusRow
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun DiagnosticsScreen(snapshot: HeaterSnapshot) {
    ScreenColumn {
        SectionTitle("Diagnostics", "Connection, firmware and event details for bring-up")

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                StatusRow("Device name", SnapHeaterBleContract.DeviceName)
                StatusRow("BLE state", snapshot.ble)
                StatusRow("Service", SnapHeaterBleContract.ServiceUuid.toString().take(13) + "...")
                StatusRow("Firmware", snapshot.firmwareVersion)
                StatusRow("Heater build", if (snapshot.heaterOutputBuildEnabled) "Enabled" else "Disabled", valueColor = if (snapshot.heaterOutputBuildEnabled) StatusColors.Warning else StatusColors.Good)
                StatusRow("Moonraker", snapshot.moonraker, valueColor = StatusColors.Warning)
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
                Text("Panda Breath hardware", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow("Map", snapshot.hardwareMapName, valueColor = StatusColors.Good)
                StatusRow("Heater relay", "GPIO${snapshot.heaterGpio}", valueColor = StatusColors.Warning)
                StatusRow("Fan TRIAC gate", "GPIO${snapshot.fanGpio}", valueColor = StatusColors.Good)
                StatusRow("Zero-cross", "GPIO${snapshot.zeroCrossGpio}", valueColor = StatusColors.Good)
                StatusRow("Chamber / PTC ADC", "CH${snapshot.chamberAdcChannel} / CH${snapshot.ptcAdcChannel}")
                StatusRow("K1/K2/K3 LEDs", "GPIO${snapshot.ledAutoGpio} / GPIO${snapshot.ledOnGpio} / GPIO${snapshot.ledOffGpio}")
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
                Text("Fan TRIAC timing", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow("Driver", if (snapshot.fanTriacControl) "Phase-angle / zero-cross" else "Plain GPIO", valueColor = if (snapshot.fanTriacControl) StatusColors.Good else StatusColors.Warning)
                StatusRow("Mains", "${snapshot.acMainsHz} Hz")
                StatusRow("Run power", "${snapshot.fanTriacRunPercent}%")
                StatusRow("Min delay", "${snapshot.fanTriacMinDelayUs} us")
                StatusRow("Gate pulse", "${snapshot.fanTriacGatePulseUs} us")
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text("Event log", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                LogLine("00:01", "App started in mock repository mode")
                LogLine("00:03", "Hardware map loaded: ${snapshot.hardwareMapName}")
                LogLine("00:05", "TRIAC fan: GPIO${snapshot.fanGpio} gate, GPIO${snapshot.zeroCrossGpio} zero-cross")
                LogLine("00:08", "Heater build enabled: ${snapshot.heaterOutputBuildEnabled}")
                LogLine("00:11", "Temperature history enabled: ${snapshot.tempHistoryEnabled}")
                LogLine("00:13", "Incident reports enabled: ${snapshot.incidentReportEnabled}")
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
                Text("Reports and maintenance", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow("Temperature history", if (snapshot.tempHistoryEnabled) "Enabled" else "Disabled", valueColor = if (snapshot.tempHistoryEnabled) StatusColors.Good else StatusColors.Warning)
                StatusRow("Incident report", if (snapshot.incidentReportEnabled) "Enabled" else "Disabled", valueColor = if (snapshot.incidentReportEnabled) StatusColors.Good else StatusColors.Warning)
                StatusRow("Airflow warning", if (snapshot.airflowWarningPending) "Pending" else "Clear", valueColor = if (snapshot.airflowWarningPending) StatusColors.Warning else StatusColors.Good)
                StatusRow("Filter warning", if (snapshot.filterWarningPending) "Pending" else "Clear", valueColor = if (snapshot.filterWarningPending) StatusColors.Warning else StatusColors.Good)
                StatusRow("OTA rollback", if (snapshot.otaRollbackReady) "Available" else "Placeholder")
            }
        }
    }
}

@Composable
private fun LogLine(time: String, message: String) {
    Text(
        "$time  $message",
        style = MaterialTheme.typography.bodySmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}
