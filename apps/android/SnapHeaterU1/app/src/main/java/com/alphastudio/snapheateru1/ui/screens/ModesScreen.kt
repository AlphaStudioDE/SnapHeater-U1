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
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle

@Composable
fun ModesScreen(snapshot: HeaterSnapshot, onMode: (AppMode) -> Unit) {
    ScreenColumn {
        SectionTitle("Modes", "Choose a workflow. Unsafe outputs stay blocked in mock and locked firmware states.")

        AppMode.entries.forEach { mode ->
            val selected = snapshot.mode == mode
            Card(
                shape = RoundedCornerShape(8.dp),
                colors = CardDefaults.cardColors(
                    containerColor = if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface,
                ),
            ) {
                Column(
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth().padding(14.dp),
                ) {
                    Text(mode.label, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    Text(mode.detail, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    if (selected) {
                        Button(onClick = { onMode(AppMode.SafeStop) }, modifier = Modifier.fillMaxWidth()) {
                            Text("Safe stop")
                        }
                    } else {
                        OutlinedButton(onClick = { onMode(mode) }, modifier = Modifier.fillMaxWidth()) {
                            Text("Select")
                        }
                    }
                }
            }
        }
    }
}
