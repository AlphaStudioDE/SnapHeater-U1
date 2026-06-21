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
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.sp
import com.alphastudio.snapheateru1.data.MockSnapHeaterRepository
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.screens.DashboardScreen
import com.alphastudio.snapheateru1.ui.screens.DiagnosticsScreen
import com.alphastudio.snapheateru1.ui.screens.ModesScreen
import com.alphastudio.snapheateru1.ui.screens.SafetyScreen
import com.alphastudio.snapheateru1.ui.screens.SettingsScreen
import com.alphastudio.snapheateru1.ui.theme.SnapHeaterTheme

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SnapHeaterApp() {
    val repository = remember { MockSnapHeaterRepository() }
    var selectedTab by remember { mutableStateOf(AppTab.Dashboard) }
    var snapshot by remember { mutableStateOf(repository.snapshot()) }

    SnapHeaterScaffold(
        selectedTab = selectedTab,
        snapshot = snapshot,
        onTab = { selectedTab = it },
    ) { tab ->
        when (tab) {
            AppTab.Dashboard -> DashboardScreen(snapshot)
            AppTab.Modes -> ModesScreen(snapshot) { mode -> snapshot = repository.setMode(mode) }
            AppTab.Safety -> SafetyScreen(snapshot)
            AppTab.Diagnostics -> DiagnosticsScreen(snapshot)
            AppTab.Settings -> SettingsScreen(snapshot) { target -> snapshot = repository.setTarget(target) }
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
                        Text(snapshot.mode.label, fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
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
