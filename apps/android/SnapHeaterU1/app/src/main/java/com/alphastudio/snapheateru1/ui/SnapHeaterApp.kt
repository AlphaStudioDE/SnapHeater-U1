/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui

import androidx.compose.material.icons.outlined.*
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.components.actionIcon

import android.content.Context
import android.content.pm.PackageManager
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
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
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.Button
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.ble.SnapHeaterBleScanException
import com.alphastudio.snapheateru1.ble.SnapHeaterBleScanner
import com.alphastudio.snapheateru1.data.BleSnapHeaterRepository
import com.alphastudio.snapheateru1.data.FirmwareSnapHeaterRepository
import com.alphastudio.snapheateru1.data.SnapHeaterApiClient
import com.alphastudio.snapheateru1.data.normalizeBaseUrl
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.requiresPrinter
import com.alphastudio.snapheateru1.ui.screens.PrinterSetupScreen
import com.alphastudio.snapheateru1.ui.screens.WifiSetupScreen
import com.alphastudio.snapheateru1.model.AppSession
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.screens.ConnectScreen
import com.alphastudio.snapheateru1.ui.screens.DashboardScreen
import com.alphastudio.snapheateru1.ui.screens.DiagnosticsScreen
import com.alphastudio.snapheateru1.ui.screens.ModesScreen
import com.alphastudio.snapheateru1.ui.screens.SafetyScreen
import com.alphastudio.snapheateru1.ui.screens.SettingsScreen
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.theme.SnapHeaterTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SnapHeaterApp() {
    var visualPreview by rememberSaveable { mutableStateOf(false) }
    if (visualPreview) {
        com.alphastudio.snapheateru1.ui.screens.VisualPreviewScreen(onExit = { visualPreview = false })
        return // No repository, scan or polling effect exists in this branch.
    }
    val context = LocalContext.current
    val deviceContext = context.applicationContext
    val credentials = remember(deviceContext) { com.alphastudio.snapheateru1.data.RestCredentials(deviceContext) }
    var restToken by remember { mutableStateOf("") }
    fun restRepository(address: String): FirmwareSnapHeaterRepository =
        FirmwareSnapHeaterRepository(SnapHeaterApiClient(address, credentials.get(normalizeBaseUrl(address))))
    val preferences = remember(context) {
        context.getSharedPreferences("snapheater_u1", Context.MODE_PRIVATE)
    }
    var appSessionName by rememberSaveable { mutableStateOf(AppSession.Connect.name) }
    var selectedTabName by rememberSaveable { mutableStateOf(AppTab.Dashboard.name) }
    var deviceAddress by rememberSaveable { mutableStateOf(preferences.getString("device_address", "") ?: "") }
    var savedDevices by remember(deviceContext) {
        mutableStateOf(preferences.getStringSet("confirmed_devices", emptySet()).orEmpty().sorted())
    }
    var connectedBaseUrl by rememberSaveable { mutableStateOf("") }
    var connectionStatus by rememberSaveable { mutableStateOf(context.getString(R.string.status_ready)) }
    var isConnecting by rememberSaveable { mutableStateOf(false) }
    var isScanning by rememberSaveable { mutableStateOf(false) }
    var advancedSettings by rememberSaveable { mutableStateOf(false) }
    var commandPending by remember { mutableStateOf(false) }
    var stopPending by remember { mutableStateOf(false) }
    var connectionHealthy by remember { mutableStateOf(false) }
    var printerWizard by rememberSaveable { mutableStateOf(true) }
    var printerSkipped by rememberSaveable { mutableStateOf(true) }
    var printerConfigSaved by remember { mutableStateOf(false) }
    var printerSetupStatus by remember { mutableStateOf("") }
    var wifiStepComplete by rememberSaveable { mutableStateOf(false) }
    var wifiSetupError by remember { mutableStateOf("") }
    var scanMessage by rememberSaveable { mutableStateOf(context.getString(R.string.status_ready)) }
    var snapshot by rememberSaveable(stateSaver = HeaterSnapshotSaver) {
        mutableStateOf(HeaterSnapshot(ble = "Disconnected"))
    }
    val appSession = AppSession.entries.firstOrNull { it.name == appSessionName } ?: AppSession.Connect
    val selectedTab = AppTab.valueOf(selectedTabName)
    val modeLabel = stringResource(snapshot.mode.labelRes())
    val heatingAllowed = connectionHealthy && !commandPending && !stopPending && !snapshot.wifi.busy && snapshot.heaterOutputBuildEnabled
    val safetyWarning = when {
        !connectionHealthy -> stringResource(R.string.daily_connection_warning)
        stopPending -> stringResource(R.string.heating_stopping)
        commandPending -> stringResource(R.string.common_pending)
        snapshot.wifi.busy -> stringResource(R.string.wifi_connecting)
        !snapshot.heaterOutputBuildEnabled -> stringResource(R.string.modes_block_build)
        else -> stringResource(R.string.modes_available)
    }
    val scope = rememberCoroutineScope()
    var settingsDraft by remember(advancedSettings, connectedBaseUrl) { mutableStateOf(snapshot) }
    val bleScanner = remember(deviceContext) { SnapHeaterBleScanner(deviceContext) }
    val firmwareRepository = remember(deviceContext, connectedBaseUrl) {
        when {
            connectedBaseUrl.startsWith("ble://", ignoreCase = true) ->
                BleSnapHeaterRepository(deviceContext, connectedBaseUrl.removePrefix("ble://"))
            connectedBaseUrl.isNotBlank() ->
                restRepository(connectedBaseUrl)
            else -> null
        }
    }

    var savedNamesRevision by remember { mutableStateOf(0) }
    fun deviceNameKey(address: String): String =
        "device_name_" + preferences.getString("device_id_$address", address)
    val savedDeviceNames = remember(savedDevices, savedNamesRevision) {
        savedDevices.associateWith { address ->
            preferences.getString(deviceNameKey(address), null)
                ?: preferences.getString("device_id_$address", null)?.let { "SH_${it.takeLast(4)}" }
                ?: "SH_?"
        }
    }

    fun rememberConnectedDevice(address: String, deviceId: String) {
        val canonical = if (address.startsWith("ble://", ignoreCase = true))
            "ble://" + address.substringAfter("://").uppercase()
        else normalizeBaseUrl(address)
        savedDevices = (savedDevices + canonical).distinct().sorted()
        preferences.edit().putStringSet("confirmed_devices", savedDevices.toSet()).apply()
        if (deviceId.matches(Regex("[A-Fa-f0-9]{12}"))) {
            val stableId = deviceId.uppercase()
            val previousId = preferences.getString("device_id_$canonical", null)
            val oldName = if (previousId == null || previousId == stableId)
                preferences.getString(deviceNameKey(canonical), null) else null
            val edit = preferences.edit().putString("device_id_$canonical", stableId)
            if (oldName != null && !preferences.contains("device_name_$stableId"))
                edit.putString("device_name_$stableId", oldName)
            edit.apply()
        }
        savedNamesRevision++
        printerWizard = true
        wifiStepComplete = false
        wifiSetupError = ""
        printerSkipped = true
        printerConfigSaved = false
        printerSetupStatus = ""
    }

    fun connectSavedDevice(address: String) {
        if (isConnecting || isScanning) return
        deviceAddress = address
        isConnecting = true
        connectionHealthy = false
        connectionStatus = context.getString(R.string.status_connecting)
        scope.launch {
            runCatching {
                withContext(Dispatchers.IO) {
                    if (address.startsWith("ble://"))
                        BleSnapHeaterRepository(deviceContext, address.removePrefix("ble://")).snapshot()
                    else restRepository(address).checkHealth()
                }
            }.onSuccess { latest ->
                rememberConnectedDevice(address, latest.deviceId)
                connectedBaseUrl = address
                snapshot = latest
                connectionHealthy = true
                preferences.edit().putString("device_address", address).apply()
                connectionStatus = context.getString(R.string.status_connected_to, address)
                appSessionName = AppSession.Connected.name
            }.onFailure { error ->
                connectionStatus = context.getString(R.string.status_connection_failed, error.shortMessage())
            }
            isConnecting = false
        }
    }

    fun requestSafeStop() {
        if (stopPending) return
        val pendingStop = snapshot.copy(
            mode = AppMode.SafeStop,
            lastConfirmedSettings = context.getString(R.string.common_pending),
        )
        connectionStatus = context.getString(R.string.heating_stopping)
        val repository = firmwareRepository
        if (repository != null) {
            stopPending = true
            scope.launch {
                runCatching {
                    withContext(Dispatchers.IO) { repository.applySettings(pendingStop) }
                }.onSuccess { latest ->
                    snapshot = latest.copy(lastConfirmedSettings = context.getString(R.string.mode_safe_stop))
                    connectionStatus = context.getString(R.string.status_settings_confirmed)
                }.onFailure { error ->
                    connectionStatus = context.getString(R.string.status_settings_failed, error.shortMessage())
                }
                stopPending = false
            }
        }
    }

    fun hasBlePermissions(): Boolean {
        return SnapHeaterBleScanner.requiredPermissions().all { permission ->
            ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
        }
    }

    fun startBleScan() {
        isScanning = true
        scanMessage = context.getString(R.string.status_scanning)
        scope.launch {
            runCatching {
                withContext(Dispatchers.IO) { bleScanner.findFirst() }
            }.onSuccess { device ->
                deviceAddress = "ble://${device.address}"
                preferences.edit().putString("ble_device_address", device.address).apply()
                scanMessage = context.getString(R.string.connect_ble_found, device.name, device.address, device.rssi)
                connectionStatus = scanMessage
            }.onFailure { error ->
                scanMessage = when (error) {
                    is SnapHeaterBleScanException -> error.message ?: context.getString(R.string.connect_no_device)
                    else -> error.shortMessage()
                }
                connectionStatus = scanMessage
            }
            isScanning = false
        }
    }

    val blePermissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestMultiplePermissions(),
    ) { grants ->
        if (grants.values.all { it }) {
            startBleScan()
        } else {
            scanMessage = context.getString(R.string.connect_ble_permissions_required)
            connectionStatus = scanMessage
        }
    }

    val notificationPermission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { }
    LaunchedEffect(appSessionName) {
        if (appSession == AppSession.Connected && android.os.Build.VERSION.SDK_INT >= 33 &&
            !preferences.getBoolean("notification_permission_requested", false)) {
            preferences.edit().putBoolean("notification_permission_requested", true).apply()
            notificationPermission.launch(android.Manifest.permission.POST_NOTIFICATIONS)
        }
    }
    val alertId = snapshot.virtualDoorDetectedMs
    val alertDevice = snapshot.deviceId.ifBlank { connectedBaseUrl }
    val alertMessage = stringResource(R.string.vdoor_message, savedDeviceNames[connectedBaseUrl] ?: "SH_${snapshot.deviceId.takeLast(4)}", snapshot.virtualDoorDropC)
    var postedAlert by remember(connectedBaseUrl) { mutableStateOf(0L) }
    var hiddenAlert by remember(connectedBaseUrl) { mutableStateOf(0L) }
    LaunchedEffect(alertDevice, alertId, snapshot.virtualDoorPending) {
        if (appSession == AppSession.Connected && snapshot.virtualDoorPending && alertId > 0 && postedAlert != alertId) {
            postVirtualDoorNotification(context, alertDevice, alertMessage)
            postedAlert = alertId
        }
    }
    if (appSession == AppSession.Connected && snapshot.virtualDoorPending && alertId > 0 && hiddenAlert != alertId) {
        androidx.compose.material3.AlertDialog(
            onDismissRequest = { hiddenAlert = alertId },
            title = { Text(stringResource(R.string.vdoor_title)) },
            text = { Text(alertMessage) },
            confirmButton = { TextButton(enabled = connectionHealthy && !commandPending && !stopPending, onClick = {
                val repository = firmwareRepository ?: return@TextButton
                commandPending = true
                scope.launch {
                    runCatching { withContext(Dispatchers.IO) { repository.acknowledgeVirtualDoor(alertId) } }
                        .onSuccess { if (snapshot.virtualDoorDetectedMs == alertId) snapshot = snapshot.copy(virtualDoorPending = false) }
                        .onFailure { connectionStatus = context.getString(R.string.status_settings_failed, it.shortMessage()) }
                    commandPending = false
                }
            }) { Text(stringResource(R.string.vdoor_ack)) } },
        )
    }

    LaunchedEffect(appSessionName, connectedBaseUrl) {
        val repository = firmwareRepository ?: return@LaunchedEffect
        if (appSession != AppSession.Connected) return@LaunchedEffect
        while (true) {
            if (commandPending || stopPending) { delay(300); continue }
            runCatching {
                withContext(Dispatchers.IO) { repository.snapshot() }
            }.onSuccess { latest ->
                connectionHealthy = true
                snapshot = latest.copy(lastConfirmedSettings = snapshot.lastConfirmedSettings)
                connectionStatus = context.getString(R.string.status_connected_to, connectedBaseUrl)
            }.onFailure { error ->
                connectionHealthy = false
                connectionStatus = context.getString(R.string.status_connection_lost, error.shortMessage())
                snapshot = snapshot.copy(ble = "LAN error")
            }
            delay(3000)
        }
    }

    if (appSession == AppSession.Connect) {
        Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
            ConnectScreen(
                savedDevices = savedDevices,
                savedDeviceNames = savedDeviceNames,
                onRenameDevice = { address, name ->
                    val edit = preferences.edit()
                    if (name.isBlank()) edit.remove(deviceNameKey(address))
                    else edit.putString(deviceNameKey(address), name.trim().take(40))
                    edit.apply()
                    savedNamesRevision++
                },
                onSavedDevice = { connectSavedDevice(it) },
                onPreview = { visualPreview = true },
                deviceAddress = deviceAddress,
                restToken = restToken,
                onRestToken = { restToken = it.take(64) },
                connectionStatus = connectionStatus,
                isConnecting = isConnecting,
                isScanning = isScanning,
                scanMessage = scanMessage,
                onDeviceAddress = {
                    deviceAddress = it
                    preferences.edit().putString("device_address", it).apply()
                    connectionStatus = context.getString(R.string.status_ready)
                    scanMessage = context.getString(R.string.status_ready)
                },
                onConnect = {
                    val baseUrl = normalizeBaseUrl(deviceAddress)
                    isConnecting = true
                    connectionStatus = context.getString(R.string.status_connecting)
                    scope.launch {
                        runCatching {
                            withContext(Dispatchers.IO) {
                                val candidate = restToken.ifBlank { credentials.get(baseUrl) }
                                require(candidate.length in 16..64) { context.getString(R.string.rest_token_note) }
                                val verified = FirmwareSnapHeaterRepository(SnapHeaterApiClient(baseUrl, candidate)).checkHealth()
                                credentials.save(baseUrl, candidate)
                                verified
                            }
                        }.onSuccess { latest ->
                            connectedBaseUrl = baseUrl
                            restToken = ""
                            rememberConnectedDevice(baseUrl, latest.deviceId)
                            preferences.edit().putString("device_address", baseUrl).apply()
                            snapshot = latest
                            connectionHealthy = true
                            connectionStatus = context.getString(R.string.status_connected_to, baseUrl)
                            appSessionName = AppSession.Connected.name
                        }.onFailure { error ->
                            connectionStatus = context.getString(R.string.status_connection_failed, error.shortMessage())
                        }
                        isConnecting = false
                    }
                },
                onBleConnect = {
                    val bleAddress = deviceAddress.removePrefix("ble://")
                    isConnecting = true
                    connectionStatus = context.getString(R.string.status_connecting)
                    scope.launch {
                        runCatching {
                            withContext(Dispatchers.IO) {
                                BleSnapHeaterRepository(context.applicationContext, bleAddress).snapshot()
                            }
                        }.onSuccess { latest ->
                            connectedBaseUrl = "ble://$bleAddress"
                            rememberConnectedDevice("ble://$bleAddress", latest.deviceId)
                            preferences.edit().putString("ble_device_address", bleAddress).apply()
                            snapshot = latest
                            connectionHealthy = true
                            connectionStatus = context.getString(R.string.status_connected_to, "BLE $bleAddress")
                            appSessionName = AppSession.Connected.name
                        }.onFailure { error ->
                            connectionStatus = context.getString(R.string.status_connection_failed, error.shortMessage())
                        }
                        isConnecting = false
                    }
                },
                onBleSearch = {
                    if (hasBlePermissions()) {
                        startBleScan()
                    } else {
                        blePermissionLauncher.launch(SnapHeaterBleScanner.requiredPermissions())
                    }
                },
            )
        }
        return
    }

    val printerAllowed = !printerSkipped && connectionHealthy && snapshot.printerDataReady
    if (printerWizard) {
        if (!wifiStepComplete || !snapshot.wifi.connected || !connectionHealthy) {
            WifiSetupScreen(
                wifi = snapshot.wifi,
                ble = connectedBaseUrl.startsWith("ble://"),
                healthy = connectionHealthy,
                pending = commandPending || stopPending,
                idle = snapshot.mode == AppMode.SafeStop && !snapshot.fanOn,
                error = wifiSetupError,
                onRequest = { action, ssid, password ->
                    val repository = firmwareRepository
                    if (repository != null && connectedBaseUrl.startsWith("ble://") &&
                        connectionHealthy && !commandPending && !stopPending && !snapshot.wifi.busy &&
                        snapshot.mode == AppMode.SafeStop && !snapshot.fanOn) {
                        commandPending = true
                        wifiSetupError = ""
                        scope.launch {
                            runCatching {
                                withContext(Dispatchers.IO) { repository.setupWifi(action, ssid, password) }
                            }.onSuccess { snapshot = it }.onFailure {
                                wifiSetupError = context.getString(R.string.wifi_request_failed)
                            }
                            commandPending = false
                        }
                    }
                },
                onContinue = {
                    if (connectionHealthy && snapshot.wifi.connected && !snapshot.wifi.busy &&
                        wifiSetupError.isBlank()) wifiStepComplete = true
                },
                onSkip = { printerSkipped = true; printerWizard = false },
                onReconnect = {
                    appSessionName = AppSession.Connect.name
                    connectedBaseUrl = ""
                    connectionHealthy = false
                    wifiSetupError = ""
                },
            )
            return
        }
        PrinterSetupScreen(
            pandaIp = snapshot.wifi.ip,
            initialHost = preferences.getString("printer_host_${snapshot.deviceId.ifBlank { connectedBaseUrl }}", "") ?: "",
            initialPort = preferences.getInt("printer_port_${snapshot.deviceId.ifBlank { connectedBaseUrl }}", 7125),
            ready = connectionHealthy && snapshot.printerDataReady,
            busy = commandPending || stopPending,
            idle = connectionHealthy && snapshot.mode == AppMode.SafeStop && !snapshot.fanOn,
            saved = printerConfigSaved,
            status = printerSetupStatus,
            onSave = { host, port, ssid, password ->
                val repository = firmwareRepository
                if (repository != null && !commandPending && !stopPending &&
                    connectionHealthy && snapshot.mode == AppMode.SafeStop && !snapshot.fanOn) {
                    commandPending = true
                    scope.launch {
                        runCatching {
                            withContext(Dispatchers.IO) { repository.configurePrinter(host, port, ssid, password) }
                        }.onSuccess {
                            snapshot = it
                            printerConfigSaved = true
                            preferences.edit()
                                .putString("printer_host_${snapshot.deviceId.ifBlank { connectedBaseUrl }}", host)
                                .putInt("printer_port_${snapshot.deviceId.ifBlank { connectedBaseUrl }}", port)
                                .apply()
                            printerSkipped = true
                            printerSetupStatus = context.getString(R.string.wizard_restart)
                        }.onFailure {
                            printerSetupStatus = context.getString(R.string.wizard_save_failed)
                        }
                        commandPending = false
                    }
                }
            },
            onContinue = {
                if (connectionHealthy && snapshot.printerDataReady && !printerConfigSaved) {
                    printerSkipped = false
                    printerWizard = false
                }
            },
            onSkip = { printerSkipped = true; printerWizard = false },
            onReconnect = {
                appSessionName = AppSession.Connect.name
                connectedBaseUrl = ""
                connectionHealthy = false
                printerConfigSaved = false
            },
        )
        return
    }

    SnapHeaterScaffold(
        selectedTab = selectedTab,
        snapshot = snapshot,
        modeLabel = modeLabel,
        connectionStatus = connectionStatus,
        onSafeStop = { requestSafeStop() },
        onTab = {
            selectedTabName = it.name
            if (it == AppTab.Settings) advancedSettings = false
        },
        onReconnect = {
            printerWizard = true
            printerSkipped = true
            printerConfigSaved = false
            appSessionName = AppSession.Connect.name
            selectedTabName = AppTab.Dashboard.name
            connectedBaseUrl = ""
            connectionStatus = context.getString(R.string.status_ready)
            snapshot = snapshot.copy(ble = context.getString(R.string.status_disconnected))
        },
    ) { tab ->
        when (tab) {
            AppTab.Dashboard -> DashboardScreen(
                stopPending = stopPending,
                snapshot = snapshot,
                telemetryFresh = connectionHealthy,
                onStart = { selectedTabName = AppTab.Modes.name },
                onSafeStop = { requestSafeStop() },
            )
            AppTab.Modes -> ModesScreen(
                printerAllowed = printerAllowed,
                onPrinterSetup = { printerWizard = true },
                snapshot = snapshot,
                heatingAllowed = heatingAllowed,
                safetyWarning = safetyWarning,
                onMode = { mode ->
                    if (mode.requiresPrinter() && !printerAllowed) return@ModesScreen
                    if (mode == AppMode.SafeStop) {
                        requestSafeStop()
                    } else {
                        snapshot = snapshot.copy(
                            mode = mode,
                            lastConfirmedSettings = context.getString(R.string.common_pending),
                        )
                    }
                },
                onConfirmSettings = { confirmed ->
                    if (confirmed.mode.requiresPrinter() && !printerAllowed) return@ModesScreen
                    commandPending = true
                    connectionStatus = context.getString(R.string.common_pending)
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
                            }
                            commandPending = false
                        }
                    } else {
                        commandPending = false
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
                        appSessionName = AppSession.Connect.name
                    }
                },
            )
            AppTab.Diagnostics -> DiagnosticsScreen(snapshot)
            AppTab.Settings -> if (!advancedSettings) {
                ScreenColumn {
                    Text(stringResource(R.string.settings_title), style = MaterialTheme.typography.headlineMedium)
                    Text(stringResource(R.string.daily_settings_intro))
                    LanguagePicker()
                    Button(onClick = { selectedTabName = AppTab.Safety.name }) {
                        ActionLabel(Icons.Outlined.Shield, stringResource(R.string.snapheater_safety))
                    }
                    TextButton(onClick = { selectedTabName = AppTab.Diagnostics.name }) {
                        ActionLabel(Icons.Outlined.MonitorHeart, stringResource(R.string.tab_diag))
                    }
                    TextButton(onClick = { advancedSettings = true }) {
                        ActionLabel(Icons.Outlined.Tune, stringResource(R.string.daily_advanced))
                    }
                }
            } else SettingsScreen(
                snapshot = settingsDraft,
                statusText = connectionStatus,
                busy = commandPending || stopPending,
                onBack = { advancedSettings = false },
                onTarget = { target -> settingsDraft = settingsDraft.copy(targetC = target) },
                onSnapshotChange = { updated -> settingsDraft = updated },
                onVirtualDoorDetectionChange = { enabled ->
                    val repository = firmwareRepository
                    if (repository != null && connectionHealthy && !commandPending && !stopPending) {
                        commandPending = true
                        scope.launch {
                            runCatching { withContext(Dispatchers.IO) { repository.setVirtualDoorDetection(enabled) } }
                                .onSuccess {
                                    snapshot = snapshot.copy(virtualDoorDetectionEnabled = it.virtualDoorDetectionEnabled)
                                    settingsDraft = settingsDraft.copy(virtualDoorDetectionEnabled = it.virtualDoorDetectionEnabled)
                                }
                                .onFailure { connectionStatus = context.getString(R.string.status_settings_failed, it.shortMessage()) }
                            commandPending = false
                        }
                    }
                },
                onApplySettings = { updated ->
                    if (!connectionHealthy || commandPending || stopPending) return@SettingsScreen
                    commandPending = true
                    val repository = firmwareRepository
                    if (repository != null) {
                        scope.launch {
                            runCatching {
                                withContext(Dispatchers.IO) { repository.savePreferences(updated) }
                            }.onSuccess { latest ->
                                if (!stopPending) snapshot = latest.copy(lastConfirmedSettings = context.getString(R.string.status_settings_applied))
                                settingsDraft = latest
                                connectionStatus = context.getString(R.string.status_settings_applied)
                            }.onFailure { error ->
                                connectionStatus = context.getString(R.string.status_settings_failed, error.shortMessage())
                            }
                            commandPending = false
                        }
                    } else {
                        commandPending = false
                        appSessionName = AppSession.Connect.name
                    }
                },
                onSchedule = { planned ->
                    val repository = firmwareRepository
                    if (repository != null && connectionHealthy && !commandPending && !stopPending &&
                        (snapshot.mode == AppMode.SafeStop || !planned.scheduledPreheatEnabled)) {
                        commandPending = true
                        scope.launch {
                            runCatching { withContext(Dispatchers.IO) { repository.schedulePreheat(planned) } }
                                .onSuccess { latest -> if (!stopPending) snapshot = latest; settingsDraft = latest }
                                .onFailure { connectionStatus = context.getString(R.string.status_settings_failed, it.shortMessage()) }
                            commandPending = false
                        }
                    } else connectionStatus = context.getString(R.string.schedule_stop_first)
                },
                restProvisionEnabled = connectedBaseUrl.startsWith("ble://") && connectionHealthy &&
                    snapshot.wifi.connected && snapshot.mode == AppMode.SafeStop && !commandPending && !stopPending,
                onProvisionRest = { token ->
                    val repository = firmwareRepository
                    val ip = snapshot.wifi.ip
                    if (repository != null && !commandPending && !stopPending && ip.isNotBlank()) {
                        commandPending = true
                        scope.launch {
                            runCatching {
                                withContext(Dispatchers.IO) {
                                    repository.provisionRestToken(token)
                                    credentials.save(normalizeBaseUrl(ip), token)
                                    restRepository(normalizeBaseUrl(ip)).checkHealth()
                                }
                            }.onSuccess { connectionStatus = context.getString(R.string.rest_verified) }
                                .onFailure { connectionStatus = context.getString(R.string.status_settings_failed, it.shortMessage()) }
                            commandPending = false
                        }
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
    onSafeStop: () -> Unit,
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
                            connectionStatus,
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 3,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                },
                actions = {
                    TextButton(onClick = onSafeStop) { ActionLabel(Icons.Outlined.PowerSettingsNew, stringResource(R.string.daily_stop)) }
                    IconButton(onClick = onReconnect) {
                        Icon(Icons.Outlined.PhonelinkSetup, contentDescription = stringResource(R.string.action_change_device))
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.background,
                    titleContentColor = MaterialTheme.colorScheme.onBackground,
                    actionIconContentColor = MaterialTheme.colorScheme.primary,
                ),
            )
        },
        bottomBar = {
            NavigationBar(
                containerColor = MaterialTheme.colorScheme.background,
                tonalElevation = 0.dp,
            ) {
                listOf(AppTab.Dashboard, AppTab.Modes, AppTab.Settings).forEach { tab ->
                    NavigationBarItem(
                        selected = selectedTab == tab || (tab == AppTab.Settings &&
                            selectedTab in listOf(AppTab.Safety, AppTab.Diagnostics)),
                        onClick = { onTab(tab) },
                        icon = { Icon(tab.icon, contentDescription = stringResource(tab.labelRes)) },
                        label = { Text(stringResource(tab.labelRes)) },
                        colors = NavigationBarItemDefaults.colors(
                            selectedIconColor = MaterialTheme.colorScheme.onPrimary,
                            selectedTextColor = MaterialTheme.colorScheme.primary,
                            indicatorColor = MaterialTheme.colorScheme.primary,
                            unselectedIconColor = MaterialTheme.colorScheme.onSurfaceVariant,
                            unselectedTextColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        ),
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
            false, // Reserved saved-state slot; retired firmware simulation.
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
            snapshot.zeroCrossSignalPresent,
            snapshot.zeroCrossEdgesPerSec,
            snapshot.zeroCrossLastPeriodUs,
            snapshot.zeroCrossMinPeriodUs,
            snapshot.zeroCrossMaxPeriodUs,
            snapshot.zeroCrossEdges,
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
            zeroCrossSignalPresent = values.getOrNull(60) as? Boolean ?: false,
            zeroCrossEdgesPerSec = values.getOrNull(61) as? Int ?: 0,
            zeroCrossLastPeriodUs = values.getOrNull(62) as? Int ?: 0,
            zeroCrossMinPeriodUs = values.getOrNull(63) as? Int ?: 0,
            zeroCrossMaxPeriodUs = values.getOrNull(64) as? Int ?: 0,
            zeroCrossEdges = values.getOrNull(65) as? Long ?: 0L,
        )
    },
)
