/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Security
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.alphastudio.snapheateru1.ui.components.StatusPill
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun ConnectScreen(
    onDemoMode: () -> Unit,
) {
    var scanState by remember { mutableStateOf("Ready") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Spacer(modifier = Modifier.height(8.dp))
        StatusPill("Local only", StatusColors.Normal)

        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("SnapHeater U1", fontSize = 34.sp, fontWeight = FontWeight.Bold)
            Text(
                "Chamber heater companion",
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        ) {
            Column(
                modifier = Modifier.fillMaxWidth().padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(14.dp),
            ) {
                Row(horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        Icons.Filled.Bluetooth,
                        contentDescription = null,
                        modifier = Modifier.size(32.dp),
                        tint = MaterialTheme.colorScheme.primary,
                    )
                    Column {
                        Text("Device connection", fontWeight = FontWeight.Bold)
                        Text(scanState, color = if (scanState == "Ready") StatusColors.Normal else StatusColors.Warning)
                    }
                }

                Button(
                    onClick = { scanState = "No validated device found" },
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Icon(Icons.Filled.Bluetooth, contentDescription = null)
                    Spacer(modifier = Modifier.size(8.dp))
                    Text("Search for SnapHeater")
                }

                FilledTonalButton(
                    onClick = onDemoMode,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Icon(Icons.Filled.PlayArrow, contentDescription = null)
                    Spacer(modifier = Modifier.size(8.dp))
                    Text("Demo mode")
                }
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(16.dp),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                verticalAlignment = Alignment.Top,
            ) {
                Icon(
                    Icons.Filled.Security,
                    contentDescription = null,
                    tint = StatusColors.Good,
                )
                Column(verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    Text("Runtime safety active", fontWeight = FontWeight.Bold)
                    Text(
                        "Firmware uses the accepted Panda Breath pin map. Heating is governed by sensor, fault and latch state.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }

        OutlinedButton(
            onClick = onDemoMode,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Continue without device")
        }
    }
}
