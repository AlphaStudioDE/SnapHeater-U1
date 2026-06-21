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
                StatusRow("Firmware channel", "Mock")
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
                LogLine("00:03", "Safety state loaded: heater locked")
                LogLine("00:08", "Moonraker bridge unavailable in prototype")
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
