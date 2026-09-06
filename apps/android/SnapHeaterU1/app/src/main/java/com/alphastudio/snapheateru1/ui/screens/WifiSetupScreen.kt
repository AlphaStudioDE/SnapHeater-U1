package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Wifi
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.PasswordVisualTransformation
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.WifiSetupStatus
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import kotlinx.coroutines.delay

@Composable
fun WifiSetupScreen(wifi: WifiSetupStatus, ble: Boolean, healthy: Boolean,
    pending: Boolean, idle: Boolean, error: String,
    onRequest: (String, String, String) -> Unit,
    onContinue: () -> Unit, onSkip: () -> Unit, onReconnect: () -> Unit) {
    var ssid by rememberSaveable { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    var seconds by remember { mutableStateOf(0) }
    val waiting = pending || (wifi.busy && wifi.phase in listOf("scanning", "connecting"))
    LaunchedEffect(wifi.operation, waiting) {
        seconds = 0
        while (waiting && seconds < 35) { delay(1000); seconds++ }
    }
    val timedOut = waiting && seconds >= 35
    val available = ble && healthy && wifi.supported && idle && !pending && !wifi.busy
    val knownSecure = wifi.networks.firstOrNull { it.ssid == ssid }?.secure == true
    val valid = ssid.toByteArray(Charsets.UTF_8).size in 1..32 &&
        ((password.isEmpty() && !knownSecure) || password.toByteArray(Charsets.UTF_8).size in 8..63)
    val successful = healthy && error.isBlank() && wifi.connected && !wifi.busy && !pending &&
        wifi.phase in listOf("idle", "connected", "scan_complete")
    val phaseText = when {
        timedOut -> R.string.wifi_result_unavailable
        !healthy -> R.string.wifi_link_lost
        !wifi.supported -> R.string.wifi_update_needed
        wifi.phase == "auth_failed" -> R.string.wifi_auth_failed
        wifi.phase == "not_found" -> R.string.wifi_not_found
        wifi.phase == "timeout" -> R.string.wifi_timeout
        wifi.phase == "save_failed" -> R.string.wifi_save_failed
        wifi.phase in listOf("connection_failed", "scan_failed") -> R.string.wifi_failed
        wifi.phase == "connecting" -> R.string.wifi_connecting
        wifi.phase == "scanning" -> R.string.wifi_scanning
        successful -> R.string.wifi_connected
        wifi.phase == "scan_complete" && wifi.networks.isEmpty() -> R.string.wifi_empty
        else -> R.string.wifi_choose
    }
    ScreenColumn {
        Text(stringResource(R.string.wifi_step), style = MaterialTheme.typography.headlineMedium)
        Text(stringResource(R.string.wifi_help))
        if (!ble) Text(stringResource(R.string.wifi_ble_required))
        Text(stringResource(phaseText), color = if (phaseText in listOf(
            R.string.wifi_auth_failed, R.string.wifi_not_found, R.string.wifi_timeout,
            R.string.wifi_failed, R.string.wifi_save_failed, R.string.wifi_result_unavailable))
            MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface)
        if (waiting && !timedOut) {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            Text("$seconds s")
        }
        if (wifi.connected) Text("${wifi.ssid} · ${wifi.ip}")
        if (error.isNotBlank()) Text(error, color = MaterialTheme.colorScheme.error)
        if (!idle) Text(stringResource(R.string.wizard_idle))
        OutlinedButton(enabled = available, onClick = { onRequest("scan", "", "") }) {
            ActionLabel(Icons.Outlined.Wifi, stringResource(R.string.wifi_scan))
        }
        wifi.networks.forEach { network ->
            OutlinedButton(enabled = available, onClick = {
                ssid = network.ssid
                password = ""
            }, modifier = Modifier.fillMaxWidth()) {
                Text("${network.ssid} · ${network.rssi} dBm")
            }
        }
        OutlinedTextField(ssid, { ssid = it }, enabled = !pending && !wifi.busy,
            label = { Text(stringResource(R.string.wifi_ssid)) }, singleLine = true)
        OutlinedTextField(password, { password = it }, enabled = !pending && !wifi.busy,
            label = { Text(stringResource(R.string.wizard_password)) }, singleLine = true,
            visualTransformation = PasswordVisualTransformation())
        Button(enabled = available && valid, onClick = {
            onRequest("connect", ssid, password)
            password = ""
        }) { Text(stringResource(R.string.wifi_try)) }
        Button(enabled = successful, onClick = onContinue) {
            Text(stringResource(R.string.wifi_next))
        }
        TextButton(enabled = !pending, onClick = onReconnect) { Text(stringResource(R.string.wizard_reconnect)) }
        TextButton(enabled = !pending, onClick = onSkip) { Text(stringResource(R.string.wizard_skip)) }
    }
}
