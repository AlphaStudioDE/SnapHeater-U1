/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.saveable.listSaver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.sp
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.AppSession
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.screens.ConnectScreen
import com.alphastudio.snapheateru1.ui.screens.DashboardScreen
import com.alphastudio.snapheateru1.ui.screens.DiagnosticsScreen
import com.alphastudio.snapheateru1.ui.screens.ModesScreen
import com.alphastudio.snapheateru1.ui.screens.SafetyScreen
import com.alphastudio.snapheateru1.ui.screens.SettingsScreen
import com.alphastudio.snapheateru1.ui.theme.SnapHeaterTheme

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SnapHeaterApp() {
    var appSessionName by rememberSaveable { mutableStateOf(AppSession.Connect.name) }
    var selectedTabName by rememberSaveable { mutableStateOf(AppTab.Dashboard.name) }
    var snapshot by rememberSaveable(stateSaver = HeaterSnapshotSaver) {
        mutableStateOf(HeaterSnapshot(ble = "Demo mode"))
    }
    val appSession = AppSession.valueOf(appSessionName)
    val selectedTab = AppTab.valueOf(selectedTabName)

    if (appSession == AppSession.Connect) {
        Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
            ConnectScreen(
                onDemoMode = {
                    appSessionName = AppSession.Demo.name
                    snapshot = snapshot.copy(ble = "Demo mode")
                },
            )
        }
        return
    }

    SnapHeaterScaffold(
        selectedTab = selectedTab,
        snapshot = snapshot,
        onTab = { selectedTabName = it.name },
    ) { tab ->
        when (tab) {
            AppTab.Dashboard -> DashboardScreen(snapshot)
            AppTab.Modes -> ModesScreen(
                snapshot = snapshot,
                onMode = { mode -> snapshot = snapshot.copy(mode = mode) },
                onTarget = { target -> snapshot = snapshot.copy(targetC = target) },
            )
            AppTab.Safety -> SafetyScreen(snapshot)
            AppTab.Diagnostics -> DiagnosticsScreen(snapshot)
            AppTab.Settings -> SettingsScreen(snapshot) { target -> snapshot = snapshot.copy(targetC = target) }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SnapHeaterScaffold(
    selectedTab: AppTab,
    snapshot: HeaterSnapshot,
    onTab: (AppTab) -> Unit,
    content: @Composable (AppTab) -> Unit,
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text("SnapHeater U1", fontWeight = FontWeight.Bold)
                        Text(
                            "${snapshot.ble} / ${snapshot.mode.label}",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
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
                        onClick = { onTab(tab) },
                        icon = { Icon(tab.icon, contentDescription = tab.label) },
                        label = { Text(tab.label) },
                    )
                }
            }
        },
    ) { padding ->
        Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
            Box(modifier = Modifier.fillMaxSize().padding(padding)) {
                content(selectedTab)
            }
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

private val HeaterSnapshotSaver = listSaver<HeaterSnapshot, Any>(
    save = { snapshot ->
        listOf(
            snapshot.chamberC,
            snapshot.ptcC,
            snapshot.targetC,
            snapshot.safetyScore,
            snapshot.heaterLocked,
            snapshot.gpioProbeLocked,
            snapshot.fanOn,
            snapshot.moonraker,
            snapshot.ble,
            snapshot.material,
            snapshot.mode.name,
        )
    },
    restore = { values ->
        HeaterSnapshot(
            chamberC = values[0] as Int,
            ptcC = values[1] as Int,
            targetC = values[2] as Int,
            safetyScore = values[3] as Int,
            heaterLocked = values[4] as Boolean,
            gpioProbeLocked = values[5] as Boolean,
            fanOn = values[6] as Boolean,
            moonraker = values[7] as String,
            ble = values[8] as String,
            material = values[9] as String,
            mode = AppMode.valueOf(values[10] as String),
        )
    },
)
