/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.CloudQueue
import androidx.compose.material.icons.filled.Dashboard
import androidx.compose.material.icons.filled.ErrorOutline
import androidx.compose.material.icons.filled.FactCheck
import androidx.compose.material.icons.filled.LocalFireDepartment
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material.icons.filled.Science
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            SnapHeaterTheme {
                SnapHeaterApp()
            }
        }
    }
}

private enum class AppTab(val label: String, val icon: ImageVector) {
    Dashboard("Status", Icons.Filled.Dashboard),
    Modes("Modes", Icons.Filled.Tune),
    Safety("Safety", Icons.Filled.FactCheck),
    Diagnostics("Diag", Icons.Filled.Science),
    Settings("Settings", Icons.Filled.Settings),
}

private data class HeaterSnapshot(
    val chamberC: Int = 31,
    val ptcC: Int = 38,
    val targetC: Int = 45,
    val safetyScore: Int = 62,
    val heaterLocked: Boolean = true,
    val gpioProbeLocked: Boolean = true,
    val fanOn: Boolean = false,
    val moonraker: String = "Read-only / waiting",
    val ble: String = "Advertising",
    val material: String = "PETG",
    val mode: String = "Auto standby",
)

@Composable
private fun SnapHeaterTheme(content: @Composable () -> Unit) {
    val scheme = androidx.compose.material3.lightColorScheme(
        primary = Color(0xFF0B6B63),
        onPrimary = Color.White,
        secondary = Color(0xFFB84A35),
        tertiary = Color(0xFF475D9C),
        background = Color(0xFFF6F4EF),
        surface = Color(0xFFFFFCF7),
        surfaceVariant = Color(0xFFE8E2D7),
        error = Color(0xFFB3261E),
    )
    MaterialTheme(colorScheme = scheme, typography = androidx.compose.material3.Typography(), content = content)
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SnapHeaterApp() {
    var selectedTab by remember { mutableStateOf(AppTab.Dashboard) }
    var snapshot by remember { mutableStateOf(HeaterSnapshot()) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text("SnapHeater U1", fontWeight = FontWeight.Bold)
                        Text(snapshot.mode, fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = MaterialTheme.colorScheme.surface),
            )
        },
        bottomBar = {
            NavigationBar(containerColor = MaterialTheme.colorScheme.surface) {
                AppTab.entries.forEach { tab ->
                    NavigationBarItem(
                        selected = selectedTab == tab,
                        onClick = { selectedTab = tab },
                        icon = { Icon(tab.icon, contentDescription = tab.label) },
                        label = { Text(tab.label) },
                    )
                }
            }
        },
    ) { padding ->
        Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
            when (selectedTab) {
                AppTab.Dashboard -> DashboardScreen(snapshot, padding)
                AppTab.Modes -> ModesScreen(snapshot, padding) { snapshot = snapshot.copy(mode = it) }
                AppTab.Safety -> SafetyScreen(snapshot, padding)
                AppTab.Diagnostics -> DiagnosticsScreen(snapshot, padding)
                AppTab.Settings -> SettingsScreen(snapshot, padding) { snapshot = snapshot.copy(targetC = it) }
            }
        }
    }
}

@Composable
private fun DashboardScreen(snapshot: HeaterSnapshot, padding: PaddingValues) {
    LazyColumn(
        modifier = Modifier.padding(padding).fillMaxSize(),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                MetricCard("Chamber", "${snapshot.chamberC} C", Icons.Filled.Thermostat, Modifier.weight(1f))
                MetricCard("PTC", "${snapshot.ptcC} C", Icons.Filled.LocalFireDepartment, Modifier.weight(1f))
            }
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.fillMaxWidth()) {
                MetricCard("Target", "${snapshot.targetC} C", Icons.Filled.Tune, Modifier.weight(1f))
                MetricCard("Safety", "${snapshot.safetyScore}%", Icons.Filled.FactCheck, Modifier.weight(1f))
            }
        }
        item {
            StatusPanel(snapshot)
        }
        item {
            WarningPanel()
        }
    }
}

@Composable
private fun ModesScreen(snapshot: HeaterSnapshot, padding: PaddingValues, onMode: (String) -> Unit) {
    val modes = listOf(
        "Auto standby" to "Printer-aware chamber mode",
        "Manual hold" to "Local target hold",
        "Preheat" to "Warm chamber before print",
        "Drying" to "Material drying profile",
        "Tempering" to "Post-print controlled cooldown",
        "Safe stop" to "Stop heating workflow",
    )
    LazyColumn(
        modifier = Modifier.padding(padding).fillMaxSize(),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        items(modes) { (name, detail) ->
            ModeRow(name, detail, selected = snapshot.mode == name, onClick = { onMode(name) })
        }
    }
}

@Composable
private fun SafetyScreen(snapshot: HeaterSnapshot, padding: PaddingValues) {
    val checks = listOf(
        "Build target esp32c3" to true,
        "Heater output locked" to snapshot.heaterLocked,
        "GPIO probe locked" to snapshot.gpioProbeLocked,
        "Fan verified" to false,
        "Chamber sensor valid" to false,
        "PTC sensor valid" to false,
        "Output polarity confirmed" to false,
        "Safety latch ready" to false,
    )
    LazyColumn(
        modifier = Modifier.padding(padding).fillMaxSize(),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        item { SectionTitle("Commissioning") }
        items(checks) { (label, ok) -> CheckRow(label, ok) }
        item { WarningPanel() }
    }
}

@Composable
private fun DiagnosticsScreen(snapshot: HeaterSnapshot, padding: PaddingValues) {
    val events = listOf(
        "boot: firmware started",
        "ble: advertising as SnapHeater U1",
        "moonraker: read-only mode",
        "safety: heater output locked",
        "adc: waiting for hardware validation",
    )
    LazyColumn(
        modifier = Modifier.padding(padding).fillMaxSize(),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        item {
            CardBlock {
                StatusLine("BLE", snapshot.ble, Icons.Filled.Bluetooth)
                StatusLine("Moonraker", snapshot.moonraker, Icons.Filled.CloudQueue)
                StatusLine("Heater", "Locked by build", Icons.Filled.Lock)
            }
        }
        item { SectionTitle("Event log") }
        items(events) { event ->
            CardBlock {
                Text(event, fontWeight = FontWeight.Medium)
            }
        }
    }
}

@Composable
private fun SettingsScreen(snapshot: HeaterSnapshot, padding: PaddingValues, onTarget: (Int) -> Unit) {
    var localOnly by remember { mutableStateOf(true) }
    var target by remember { mutableFloatStateOf(snapshot.targetC.toFloat()) }
    LazyColumn(
        modifier = Modifier.padding(padding).fillMaxSize(),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            CardBlock {
                Text("Material", fontWeight = FontWeight.Bold)
                Text(snapshot.material, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        item {
            CardBlock {
                Text("Target limit", fontWeight = FontWeight.Bold)
                Text("${target.roundToInt()} C")
                Slider(value = target, onValueChange = { target = it; onTarget(it.roundToInt()) }, valueRange = 25f..60f)
            }
        }
        item {
            CardBlock {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
                    Column {
                        Text("Local-only mode", fontWeight = FontWeight.Bold)
                        Text("BLE/Wi-Fi LAN control", color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    Switch(checked = localOnly, onCheckedChange = { localOnly = it })
                }
            }
        }
    }
}

@Composable
private fun MetricCard(title: String, value: String, icon: ImageVector, modifier: Modifier = Modifier) {
    CardBlock(modifier = modifier.height(118.dp)) {
        Icon(icon, contentDescription = title, tint = MaterialTheme.colorScheme.primary)
        Spacer(Modifier.height(8.dp))
        Text(value, fontSize = 28.sp, fontWeight = FontWeight.Bold)
        Text(title, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun StatusPanel(snapshot: HeaterSnapshot) {
    CardBlock {
        SectionTitle("System")
        StatusLine("Heater", if (snapshot.heaterLocked) "Locked" else "Enabled", Icons.Filled.Lock)
        StatusLine("Fan", if (snapshot.fanOn) "Running" else "Standby", Icons.Filled.PowerSettingsNew)
        StatusLine("BLE", snapshot.ble, Icons.Filled.Bluetooth)
        StatusLine("U1", snapshot.moonraker, Icons.Filled.CloudQueue)
    }
}

@Composable
private fun WarningPanel() {
    Card(
        colors = CardDefaults.cardColors(containerColor = Color(0xFFFFE8D8)),
        shape = RoundedCornerShape(8.dp),
    ) {
        Row(modifier = Modifier.padding(14.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Icon(Icons.Filled.ErrorOutline, contentDescription = null, tint = MaterialTheme.colorScheme.secondary)
            Text("Hardware output remains locked until GPIO, fan, sensors and safety latch are verified.")
        }
    }
}

@Composable
private fun ModeRow(name: String, detail: String, selected: Boolean, onClick: () -> Unit) {
    CardBlock {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(1f)) {
                Text(name, fontWeight = FontWeight.Bold)
                Text(detail, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Button(
                onClick = onClick,
                colors = ButtonDefaults.buttonColors(
                    containerColor = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.tertiary,
                ),
                shape = RoundedCornerShape(8.dp),
            ) {
                Text(if (selected) "Active" else "Select")
            }
        }
    }
}

@Composable
private fun CheckRow(label: String, ok: Boolean) {
    CardBlock {
        StatusLine(label, if (ok) "OK" else "Pending", if (ok) Icons.Filled.CheckCircle else Icons.Filled.ErrorOutline)
    }
}

@Composable
private fun StatusLine(label: String, value: String, icon: ImageVector) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth().padding(vertical = 5.dp)) {
        Box(
            modifier = Modifier.size(34.dp).background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(8.dp)),
            contentAlignment = Alignment.Center,
        ) {
            Icon(icon, contentDescription = null, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(20.dp))
        }
        Column {
            Text(label, fontWeight = FontWeight.Medium)
            Text(value, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp)
        }
    }
}

@Composable
private fun SectionTitle(text: String) {
    Text(text, fontWeight = FontWeight.Bold, fontSize = 18.sp)
}

@Composable
private fun CardBlock(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Card(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp),
    ) {
        Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            content()
        }
    }
}

@Preview(showBackground = true)
@Composable
private fun AppPreview() {
    SnapHeaterTheme {
        SnapHeaterApp()
    }
}
