/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui

import android.content.Context
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.runtime.saveable.listSaver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.sp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.data.FirmwareSnapHeaterRepository
import com.alphastudio.snapheateru1.data.SnapHeaterApiClient
import com.alphastudio.snapheateru1.data.normalizeBaseUrl
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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SnapHeaterApp() {
    val context = LocalContext.current
    val preferences = remember(context) {
        context.getSharedPreferences("snapheater_u1", Context.MODE_PRIVATE)
    }
    var appSessionName by rememberSaveable { mutableStateOf(AppSession.Connect.name) }
    var selectedTabName by rememberSaveable { mutableStateOf(AppTab.Dashboard.name) }
    var deviceAddress by rememberSaveable { mutableStateOf(preferences.getString("device_address", "") ?: "") }
    var connectedBaseUrl by rememberSaveable { mutableStateOf("") }
    var connectionStatus by rememberSaveable { mutableStateOf(context.getString(R.string.status_ready)) }
    var isConnecting by rememberSaveable { mutableStateOf(false) }
    var snapshot by rememberSaveable(stateSaver = HeaterSnapshotSaver) {
        mutableStateOf(HeaterSnapshot(ble = "Demo mode"))
    }
    val appSession = AppSession.valueOf(appSessionName)
    val selectedTab = AppTab.valueOf(selectedTabName)
    val modeLabel = stringResource(snapshot.mode.labelRes())
    val heatingAllowed = snapshot.heaterOutputBuildEnabled &&
        snapshot.outputSafetyLatchArmed &&
        snapshot.outputSafetyLatchReady &&
        snapshot.heaterOutputVerified &&
        snapshot.fanOutputVerified &&
        snapshot.sensorsVerified
    val safetyWarning = when {
        !snapshot.heaterOutputBuildEnabled -> stringResource(R.string.modes_block_build)
        !snapshot.heaterOutputVerified || !snapshot.fanOutputVerified || !snapshot.sensorsVerified -> stringResource(R.string.modes_block_verification)
        !snapshot.outputSafetyLatchArmed || !snapshot.outputSafetyLatchReady -> stringResource(R.string.modes_block_latch)
        else -> stringResource(R.string.modes_available)
    }
    val scope = rememberCoroutineScope()
    val firmwareRepository = remember(connectedBaseUrl) {
        if (connectedBaseUrl.isBlank()) null else FirmwareSnapHeaterRepository(SnapHeaterApiClient(connectedBaseUrl))
    }

    LaunchedEffect(appSessionName, connectedBaseUrl) {
        val repository = firmwareRepository ?: return@LaunchedEffect
        if (appSession != AppSession.Demo) return@LaunchedEffect
        while (true) {
            runCatching {
                withContext(Dispatchers.IO) { repository.snapshot() }
            }.onSuccess { latest ->
                snapshot = latest.copy(lastConfirmedSettings = snapshot.lastConfirmedSettings)
                connectionStatus = context.getString(R.string.status_connected_to, connectedBaseUrl)
            }.onFailure { error ->
                connectionStatus = context.getString(R.string.status_connection_lost, error.shortMessage())
                snapshot = snapshot.copy(ble = "LAN error")
            }
            delay(3000)
        }
    }

    if (appSession == AppSession.Connect) {
        Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
            ConnectScreen(
                deviceAddress = deviceAddress,
                connectionStatus = connectionStatus,
                isConnecting = isConnecting,
                onDeviceAddress = {
                    deviceAddress = it
                    preferences.edit().putString("device_address", it).apply()
                    connectionStatus = context.getString(R.string.status_ready)
                },
                onConnect = {
                    val baseUrl = normalizeBaseUrl(deviceAddress)
                    isConnecting = true
                    connectionStatus = context.getString(R.string.status_connecting)
                    scope.launch {
                        runCatching {
                            withContext(Dispatchers.IO) {
                                FirmwareSnapHeaterRepository(SnapHeaterApiClient(baseUrl)).checkHealth()
                            }
                        }.onSuccess { latest ->
                            connectedBaseUrl = baseUrl
                            preferences.edit().putString("device_address", baseUrl).apply()
                            snapshot = latest
                            connectionStatus = context.getString(R.string.status_connected_to, baseUrl)
                            appSessionName = AppSession.Demo.name
                        }.onFailure { error ->
                            connectionStatus = context.getString(R.string.status_connection_failed, error.shortMessage())
                        }
                        isConnecting = false
                    }
                },
                onDemoMode = {
                    connectedBaseUrl = ""
                    appSessionName = AppSession.Demo.name
                    snapshot = snapshot.copy(ble = context.getString(R.string.status_demo_mode))
                },
            )
        }
        return
    }

    SnapHeaterScaffold(
        selectedTab = selectedTab,
        snapshot = snapshot,
        modeLabel = modeLabel,
        connectionStatus = connectionStatus,
        onTab = { selectedTabName = it.name },
        onReconnect = {
            appSessionName = AppSession.Connect.name
            selectedTabName = AppTab.Dashboard.name
            connectedBaseUrl = ""
            connectionStatus = context.getString(R.string.status_ready)
            snapshot = snapshot.copy(ble = context.getString(R.string.status_disconnected))
        },
    ) { tab ->
        when (tab) {
            AppTab.Dashboard -> DashboardScreen(snapshot)
            AppTab.Modes -> ModesScreen(
                snapshot = snapshot,
                heatingAllowed = heatingAllowed,
                safetyWarning = safetyWarning,
                onMode = { mode ->
                    snapshot = snapshot.copy(
                        mode = mode,
                        lastConfirmedSettings = context.getString(R.string.common_pending),
                    )
                },
                onSnapshotChange = { updated -> snapshot = updated },
                onConfirmSettings = { confirmed ->
                    snapshot = confirmed
                    val repository = firmwareRepository
                    if (repository != null) {
                        scope.launch {
                            runCatching {
                                withContext(Dispatchers.IO) { repository.applySettings(confirmed) }
                            }.onSuccess { latest ->
                                snapshot = latest.copy(lastConfirmedSettings = confirmed.lastConfirmedSettings)
                                connectionStatus = context.getString(R.string.status_settings_confirmed)
                            }.onFailure { error ->
                                connectionStatus = context.getString(R.string.status_settings_failed, error.shortMessage())
                                snapshot = confirmed.copy(lastConfirmedSettings = context.getString(R.string.common_pending))
                            }
                        }
                    }
                },
            )
            AppTab.Safety -> SafetyScreen(
                snapshot = snapshot,
                onApplySafety = { updated, armLatch, disarmLatch ->
                    snapshot = updated.copy(lastConfirmedSettings = "Applying safety state")
                    val repository = firmwareRepository
                    if (repository != null) {
                        scope.launch {
                            runCatching {
                                withContext(Dispatchers.IO) { repository.applySafety(updated, armLatch, disarmLatch) }
                            }.onSuccess { latest ->
                                snapshot = latest.copy(lastConfirmedSettings = context.getString(R.string.status_safety_applied))
                                connectionStatus = context.getString(R.string.status_safety_applied)
                            }.onFailure { error ->
                                connectionStatus = context.getString(R.string.status_safety_failed, error.shortMessage())
                                snapshot = updated.copy(lastConfirmedSettings = context.getString(R.string.status_safety_pending))
                            }
                        }
                    } else {
                        val armed = when {
                            armLatch -> true
                            disarmLatch -> false
                            else -> updated.outputSafetyLatchArmed
                        }
                        snapshot = updated.copy(
                            outputSafetyLatchArmed = armed,
                            outputSafetyLatchReady = armed &&
                                updated.heaterOutputVerified &&
                                updated.fanOutputVerified &&
                                updated.sensorsVerified,
                            lastConfirmedSettings = context.getString(R.string.status_safety_applied_demo),
                        )
                    }
                },
            )
            AppTab.Diagnostics -> DiagnosticsScreen(snapshot)
            AppTab.Settings -> SettingsScreen(
                snapshot = snapshot,
                onTarget = { target -> snapshot = snapshot.copy(targetC = target) },
                onSnapshotChange = { updated -> snapshot = updated },
                onApplySettings = { updated ->
                    snapshot = updated.copy(lastConfirmedSettings = context.getString(R.string.status_applying_settings))
                    val repository = firmwareRepository
                    if (repository != null) {
                        scope.launch {
                            runCatching {
                                withContext(Dispatchers.IO) { repository.applySettings(updated) }
                            }.onSuccess { latest ->
                                snapshot = latest.copy(lastConfirmedSettings = context.getString(R.string.status_settings_applied))
                                connectionStatus = context.getString(R.string.status_settings_applied)
                            }.onFailure { error ->
                                connectionStatus = context.getString(R.string.status_settings_failed, error.shortMessage())
                                snapshot = updated.copy(lastConfirmedSettings = context.getString(R.string.status_settings_pending))
                            }
                        }
                    } else {
                        snapshot = updated.copy(lastConfirmedSettings = context.getString(R.string.status_settings_applied_demo))
                    }
                },
            )
        }
    }
}

private fun Throwable.shortMessage(): String = message?.take(80) ?: this::class.java.simpleName

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SnapHeaterScaffold(
    selectedTab: AppTab,
    snapshot: HeaterSnapshot,
    modeLabel: String,
    connectionStatus: String,
    onTab: (AppTab) -> Unit,
    onReconnect: () -> Unit,
    content: @Composable (AppTab) -> Unit,
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text(stringResource(R.string.app_name), fontWeight = FontWeight.Bold)
                        Text(
                            "${snapshot.ble} / $modeLabel / $connectionStatus",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                },
                actions = {
                    IconButton(onClick = onReconnect) {
                        Icon(Icons.Filled.Close, contentDescription = stringResource(R.string.action_change_device))
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
                        icon = { Icon(tab.icon, contentDescription = stringResource(tab.labelRes)) },
                        label = { Text(stringResource(tab.labelRes)) },
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
            snapshot.setupValidationPassed,
            snapshot.outputSafetyLatchArmed,
            snapshot.heaterLocked,
            snapshot.gpioProbeLocked,
            snapshot.fanOn,
            snapshot.moonraker,
            snapshot.ble,
            snapshot.material,
            snapshot.mode.name,
            snapshot.manualFanAssist,
            snapshot.preheatHeatSoakMin,
            snapshot.dryingTimeMin,
            snapshot.temperingDurationMin,
            snapshot.lastConfirmedSettings,
            snapshot.heatSoakEnabled,
            snapshot.virtualDoorDetectionEnabled,
            snapshot.autoMaterialProfileEnabled,
            snapshot.mismatchWarningEnabled,
            snapshot.plaProtectionEnabled,
            snapshot.antiWarpEnabled,
            snapshot.largePrintProtectionEnabled,
            snapshot.safeOvernightEnabled,
            snapshot.pauseHoldEnabled,
            snapshot.smartResumeEnabled,
            snapshot.startPrintWarningEnabled,
            snapshot.airflowDetectionEnabled,
            snapshot.tempHistoryEnabled,
            snapshot.incidentReportEnabled,
            snapshot.localRecipesEnabled,
            snapshot.scheduledPreheatEnabled,
            snapshot.localOnlyMode,
            snapshot.demoModeEnabled,
            snapshot.showcaseModeEnabled,
            snapshot.symbiontModeEnabled,
            snapshot.symbiontVentilationAllowed,
            snapshot.heaterOutputBuildEnabled,
            snapshot.hardwareMapName,
            snapshot.hardwareSafetyState,
            snapshot.heaterGpio,
            snapshot.fanGpio,
            snapshot.zeroCrossGpio,
            snapshot.chamberAdcChannel,
            snapshot.ptcAdcChannel,
            snapshot.ledAutoGpio,
            snapshot.ledOnGpio,
            snapshot.ledOffGpio,
            snapshot.fanTriacControl,
            snapshot.acMainsHz,
            snapshot.fanTriacRunPercent,
            snapshot.fanTriacMinDelayUs,
            snapshot.fanTriacGatePulseUs,
            snapshot.outputSafetyLatchReady,
            snapshot.heaterOutputVerified,
            snapshot.fanOutputVerified,
            snapshot.sensorsVerified,
            snapshot.moonrakerVerified,
        )
    },
    restore = { values ->
        val heaterBuildEnabled = values.getOrNull(39) as? Boolean ?: true
        HeaterSnapshot(
            chamberC = values[0] as Int,
            ptcC = values[1] as Int,
            targetC = values[2] as Int,
            safetyScore = values[3] as Int,
            setupValidationPassed = values[4] as Boolean,
            outputSafetyLatchArmed = values[5] as Boolean,
            heaterOutputBuildEnabled = heaterBuildEnabled,
            heaterLocked = if (heaterBuildEnabled) false else values[6] as Boolean,
            gpioProbeLocked = values[7] as Boolean,
            fanOn = values[8] as Boolean,
            moonraker = values[9] as String,
            ble = values[10] as String,
            material = values[11] as String,
            mode = AppMode.valueOf(values[12] as String),
            manualFanAssist = values[13] as Boolean,
            preheatHeatSoakMin = values[14] as Int,
            dryingTimeMin = values[15] as Int,
            temperingDurationMin = values[16] as Int,
            lastConfirmedSettings = values[17] as String,
            heatSoakEnabled = values[18] as Boolean,
            virtualDoorDetectionEnabled = values[19] as Boolean,
            autoMaterialProfileEnabled = values[20] as Boolean,
            mismatchWarningEnabled = values[21] as Boolean,
            plaProtectionEnabled = values[22] as Boolean,
            antiWarpEnabled = values[23] as Boolean,
            largePrintProtectionEnabled = values[24] as Boolean,
            safeOvernightEnabled = values[25] as Boolean,
            pauseHoldEnabled = values[26] as Boolean,
            smartResumeEnabled = values[27] as Boolean,
            startPrintWarningEnabled = values[28] as Boolean,
            airflowDetectionEnabled = values[29] as Boolean,
            tempHistoryEnabled = values[30] as Boolean,
            incidentReportEnabled = values[31] as Boolean,
            localRecipesEnabled = values[32] as Boolean,
            scheduledPreheatEnabled = values[33] as Boolean,
            localOnlyMode = values[34] as Boolean,
            demoModeEnabled = values[35] as Boolean,
            showcaseModeEnabled = values[36] as Boolean,
            symbiontModeEnabled = values[37] as Boolean,
            symbiontVentilationAllowed = values[38] as Boolean,
            hardwareMapName = values.getOrNull(40) as? String ?: "panda_breath_accepted",
            hardwareSafetyState = values.getOrNull(41) as? String ?: "heater_output_build_enabled_runtime_latch_required",
            heaterGpio = values.getOrNull(42) as? Int ?: 18,
            fanGpio = values.getOrNull(43) as? Int ?: 3,
            zeroCrossGpio = values.getOrNull(44) as? Int ?: 7,
            chamberAdcChannel = values.getOrNull(45) as? Int ?: 0,
            ptcAdcChannel = values.getOrNull(46) as? Int ?: 1,
            ledAutoGpio = values.getOrNull(47) as? Int ?: 6,
            ledOnGpio = values.getOrNull(48) as? Int ?: 5,
            ledOffGpio = values.getOrNull(49) as? Int ?: 4,
            fanTriacControl = values.getOrNull(50) as? Boolean ?: true,
            acMainsHz = values.getOrNull(51) as? Int ?: 50,
            fanTriacRunPercent = values.getOrNull(52) as? Int ?: 100,
            fanTriacMinDelayUs = values.getOrNull(53) as? Int ?: 200,
            fanTriacGatePulseUs = values.getOrNull(54) as? Int ?: 100,
            outputSafetyLatchReady = values.getOrNull(55) as? Boolean ?: false,
            heaterOutputVerified = values.getOrNull(56) as? Boolean ?: false,
            fanOutputVerified = values.getOrNull(57) as? Boolean ?: false,
            sensorsVerified = values.getOrNull(58) as? Boolean ?: false,
            moonrakerVerified = values.getOrNull(59) as? Boolean ?: false,
        )
    },
)
