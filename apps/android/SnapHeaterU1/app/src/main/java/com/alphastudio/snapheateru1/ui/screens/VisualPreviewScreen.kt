/*
 * SnapHeater U1 — Copyright (c) 2026 Damian Borkowski — SPDX-License-Identifier: MIT
 */
package com.alphastudio.snapheateru1.ui.screens

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.AppTab
import com.alphastudio.snapheateru1.ui.LanguagePicker
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.components.ScreenColumn

// Deliberately no transport, preferences, repository, device address or side effects.
// Sample state is owned by this screen and discarded when leaving it.
@Composable
fun VisualPreviewScreen(onExit: () -> Unit) {
    var tabName by rememberSaveable { mutableStateOf(AppTab.Dashboard.name) }
    val tab = AppTab.valueOf(tabName)
    var sample by remember { mutableStateOf(HeaterSnapshot(
        chamberC = 38, ptcC = 52, targetC = 45, mode = AppMode.Preheat,
        fanOn = true, heaterOutputBuildEnabled = true,
        outputSafetyLatchReady = true, firmwareVersion = "UI PREVIEW",
        ble = "UI PREVIEW", moonraker = "UI PREVIEW",
    )) }
    val previewNote = stringResource(R.string.visual_preview_notice)
    fun stopSample() { sample = sample.copy(mode = AppMode.SafeStop) }
    BackHandler {
        if (tab != AppTab.Dashboard) tabName = AppTab.Dashboard.name else onExit()
    }
    Scaffold(
        topBar = {
            Surface(color = MaterialTheme.colorScheme.secondaryContainer) {
                Column(Modifier.fillMaxWidth().statusBarsPadding().padding(12.dp)) {
                    Text(previewNote, style = MaterialTheme.typography.bodyMedium)
                    TextButton(onClick = onExit) {
                        ActionLabel(Icons.AutoMirrored.Outlined.ArrowBack,
                            stringResource(R.string.visual_preview_exit))
                    }
                }
            }
        },
        bottomBar = {
            NavigationBar {
                listOf(AppTab.Dashboard, AppTab.Modes, AppTab.Settings).forEach { item ->
                    NavigationBarItem(
                        selected = tab == item || (item == AppTab.Settings &&
                            tab in listOf(AppTab.Diagnostics, AppTab.Safety)),
                        onClick = { tabName = item.name },
                        icon = { Icon(item.icon, contentDescription = null) },
                        label = { Text(stringResource(item.labelRes)) },
                    )
                }
            }
        },
    ) { padding ->
        Box(Modifier.fillMaxSize().padding(padding)) {
            when (tab) {
                AppTab.Dashboard -> DashboardScreen(sample, telemetryFresh = true,
                    onStart = { tabName = AppTab.Modes.name }, onSafeStop = { stopSample() })
                AppTab.Modes -> ModesScreen(sample, heatingAllowed = true,
                    safetyWarning = previewNote,
                    onMode = { if (it == AppMode.SafeStop) stopSample() },
                    onConfirmSettings = {
                        sample = it.copy(lastConfirmedSettings = previewNote)
                        tabName = AppTab.Dashboard.name
                    })
                AppTab.Safety -> SafetyScreen(sample, onApplySafety = { _, _, _ -> })
                AppTab.Diagnostics -> DiagnosticsScreen(sample)
                AppTab.History -> ScreenColumn { Text(previewNote) }
                AppTab.Settings -> ScreenColumn {
                    Text(stringResource(R.string.settings_title), style = MaterialTheme.typography.headlineMedium)
                    LanguagePicker()
                    Text(previewNote)
                    TextButton(onClick = { tabName = AppTab.Safety.name }) {
                        ActionLabel(Icons.Outlined.Shield, stringResource(R.string.snapheater_safety))
                    }
                    TextButton(onClick = { tabName = AppTab.Diagnostics.name }) {
                        ActionLabel(Icons.Outlined.MonitorHeart, stringResource(R.string.tab_diag))
                    }
                    TextButton(onClick = { tabName = AppTab.Dashboard.name }) {
                        ActionLabel(Icons.Outlined.Home, stringResource(R.string.snapheater_status))
                    }
                }
            }
        }
    }
}
