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
import androidx.compose.material.icons.outlined.Wifi
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material.icons.outlined.Visibility
import androidx.compose.material.icons.outlined.BluetoothSearching
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.LanguagePicker
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Security
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.ui.components.StatusPill
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun ConnectScreen(
    savedDevices: List<String>,
    savedDeviceNames: Map<String, String>,
    onRenameDevice: (String, String) -> Unit,
    onSavedDevice: (String) -> Unit,
    onPreview: () -> Unit,
    deviceAddress: String,
    restToken: String,
    onRestToken: (String) -> Unit,
    connectionStatus: String,
    isConnecting: Boolean,
    isScanning: Boolean,
    scanMessage: String,
    onDeviceAddress: (String) -> Unit,
    onConnect: () -> Unit,
    onBleConnect: () -> Unit,
    onBleSearch: () -> Unit,
) {
    var renamingAddress by remember { mutableStateOf<String?>(null) }
    var newName by remember { mutableStateOf("") }
    if (renamingAddress != null) {
        androidx.compose.material3.AlertDialog(
            onDismissRequest = { renamingAddress = null },
            title = { Text(stringResource(R.string.device_rename)) },
            text = {
                OutlinedTextField(newName, { if (it.length <= 40) newName = it },
                    label = { Text(stringResource(R.string.device_name)) },
                    supportingText = { Text(stringResource(R.string.device_name_help)) },
                    singleLine = true)
            },
            confirmButton = {
                androidx.compose.material3.TextButton(onClick = {
                    renamingAddress?.let { onRenameDevice(it, newName) }
                    renamingAddress = null
                }) { Text(stringResource(R.string.device_name_save)) }
            },
            dismissButton = {
                androidx.compose.material3.TextButton(onClick = { renamingAddress = null }) {
                    Text(stringResource(R.string.device_name_cancel))
                }
            },
        )
    }
    val ready = stringResource(R.string.status_ready)
    val canConnectOverBle = deviceAddress.startsWith("ble://", ignoreCase = true)
    val canConnectOverLan = deviceAddress.isNotBlank() &&
        !deviceAddress.contains(":") &&
        !canConnectOverBle

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Spacer(modifier = Modifier.height(8.dp))
        if (savedDevices.isNotEmpty()) {
            Text(stringResource(R.string.connect_saved_devices), style = MaterialTheme.typography.titleLarge)
            savedDevices.forEach { address ->
                OutlinedButton(
                    onClick = { onSavedDevice(address) },
                    enabled = !isConnecting && !isScanning,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Column(modifier = Modifier.fillMaxWidth()) {
                        ActionLabel(
                            if (address.startsWith("ble://")) Icons.Filled.Bluetooth else Icons.Outlined.Wifi,
                            savedDeviceNames[address] ?: "SH_?",
                        )
                        Text(address.removePrefix("ble://").removePrefix("http://").removePrefix("https://"),
                            style = MaterialTheme.typography.bodySmall)
                    }
                }
                androidx.compose.material3.TextButton(enabled = !isConnecting && !isScanning, onClick = {
                    newName = savedDeviceNames[address].orEmpty()
                    renamingAddress = address
                }) { ActionLabel(Icons.Outlined.Edit, stringResource(R.string.device_rename)) }
            }
            Text(stringResource(R.string.connect_saved_hint), style = MaterialTheme.typography.bodySmall)
        }
        Text(stringResource(R.string.wizard_snapheater), style = MaterialTheme.typography.titleLarge)
        LanguagePicker()
        OutlinedButton(
            onClick = onPreview,
            enabled = !isConnecting && !isScanning,
            modifier = Modifier.fillMaxWidth(),
        ) {
            ActionLabel(Icons.Outlined.Visibility, stringResource(R.string.visual_preview_open))
        }
        StatusPill(stringResource(R.string.status_local_only), StatusColors.Normal)

        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(stringResource(R.string.app_name), fontSize = 34.sp, fontWeight = FontWeight.Bold)
            Text(
                stringResource(R.string.connect_subtitle),
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
                        Text(stringResource(R.string.connect_device_connection), fontWeight = FontWeight.Bold)
                        Text(connectionStatus, color = if (connectionStatus == ready) StatusColors.Normal else StatusColors.Warning)
                    }
                }

                OutlinedTextField(
                    value = deviceAddress,
                    onValueChange = onDeviceAddress,
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                    label = { Text(stringResource(R.string.connect_device_address)) },
                placeholder = { Text("192.168.1.80") },
                )

                OutlinedTextField(value = restToken, onValueChange = onRestToken,
                    label = { Text(stringResource(R.string.rest_token)) }, singleLine = true,
                    visualTransformation = androidx.compose.ui.text.input.PasswordVisualTransformation(),
                    modifier = Modifier.fillMaxWidth())
                Text(stringResource(R.string.rest_token_note), style = MaterialTheme.typography.bodySmall)
                Button(
                    onClick = onConnect,
                    enabled = !isConnecting && canConnectOverLan,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    ActionLabel(Icons.Outlined.Wifi,
                        if (isConnecting) stringResource(R.string.status_connecting) else stringResource(R.string.connect_over_lan))
                }

                if (canConnectOverBle) {
                    Button(
                        onClick = onBleConnect,
                        enabled = !isConnecting,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Icon(Icons.Filled.Bluetooth, contentDescription = null)
                        Spacer(modifier = Modifier.size(8.dp))
                        Text(if (isConnecting) stringResource(R.string.status_connecting) else stringResource(R.string.connect_over_ble))
                    }
                }

                Button(
                    onClick = onBleSearch,
                    enabled = !isScanning,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Icon(Icons.Outlined.BluetoothSearching, contentDescription = null)
                    Spacer(modifier = Modifier.size(8.dp))
                    Text(if (isScanning) stringResource(R.string.status_scanning) else stringResource(R.string.connect_search))
                }


                if (scanMessage != ready) {
                    Text(scanMessage, color = StatusColors.Warning, style = MaterialTheme.typography.bodySmall)
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
                    Text(stringResource(R.string.connect_runtime_safety_title), fontWeight = FontWeight.Bold)
                    Text(
                        stringResource(R.string.connect_runtime_safety_body),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }

    }
}
