package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.PasswordVisualTransformation
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.data.printerConfiguration
import com.alphastudio.snapheateru1.data.PrinterDiscovery
import com.alphastudio.snapheateru1.data.DiscoveredPrinter
import com.alphastudio.snapheateru1.data.DiscoveryStage
import kotlinx.coroutines.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Search
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.components.ScreenColumn

@Composable
fun PrinterSetupScreen(ready: Boolean, busy: Boolean, idle: Boolean, saved: Boolean,
    status: String, onSave: (String, Int, String) -> Unit,
    onContinue: () -> Unit, onSkip: () -> Unit, onReconnect: () -> Unit,
    initialHost: String = "", initialPort: Int = 7125, pandaIp: String = "") {
    var host by rememberSaveable { mutableStateOf(initialHost) }
    var port by rememberSaveable { mutableStateOf(initialPort.toString()) }
    var apiKey by remember { mutableStateOf("") } // No preferences or saved-instance state.
    val context = LocalContext.current
    val scanner = remember(context.applicationContext) { PrinterDiscovery(context.applicationContext) }
    val scope = rememberCoroutineScope()
    var scanJob by remember { mutableStateOf<Job?>(null) }
    var scanning by remember { mutableStateOf(false) }
    var results by remember { mutableStateOf(emptyList<DiscoveredPrinter>()) }
    var scanStatus by remember { mutableStateOf("") }
    fun startDiscovery() {
        if (scanning || busy) return
        scanning = true
        results = emptyList()
        scanJob = scope.launch {
            try {
                results = scanner.discover(host.trim(), port.toIntOrNull() ?: 7125, pandaIp) { stage, percent ->
                    val key = when (stage) {
                        DiscoveryStage.Saved -> R.string.discovery_saved
                        DiscoveryStage.Mdns -> R.string.discovery_mdns
                        DiscoveryStage.Quick -> R.string.discovery_quick
                        DiscoveryStage.Full -> R.string.discovery_full
                    }
                    withContext(Dispatchers.Main.immediate) { scanStatus = context.getString(key) + " · $percent%" }
                }
                scanStatus = context.getString(if (results.isEmpty()) R.string.discovery_empty else R.string.discovery_found)
            } catch (cancelled: CancellationException) {
                scanStatus = context.getString(R.string.discovery_cancelled)
                throw cancelled
            } catch (_: Exception) {
                scanStatus = context.getString(R.string.discovery_failed)
            } finally { scanning = false }
        }
    }
    val valid = runCatching { printerConfiguration(host.trim(), port.toInt(), apiKey) }.isSuccess
    ScreenColumn {
        Text(stringResource(R.string.wizard_printer), style = MaterialTheme.typography.headlineMedium)
        Text(stringResource(R.string.wizard_help))
        Text(stringResource(R.string.discovery_help))
        if(busy) {LinearProgressIndicator(Modifier.fillMaxWidth());Text(stringResource(R.string.printer_setup_testing))}
        Button(enabled = !busy && !scanning, onClick = { startDiscovery() }) {
            ActionLabel(Icons.Outlined.Search, stringResource(R.string.discovery_search))
        }
        if (scanning) {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            TextButton(onClick = { scanJob?.cancel() }) { Text(stringResource(R.string.discovery_cancel)) }
        }
        if (scanStatus.isNotBlank()) Text(scanStatus)
        results.forEach { printer ->
            OutlinedButton(enabled = !busy && !scanning, onClick = {
                host = printer.host
                port = printer.port.toString()
            }, modifier = Modifier.fillMaxWidth()) {
                Column {
                    Text(printer.name)
                    Text("${printer.host}:${printer.port}")
                    Text(stringResource(if (printer.controlData) R.string.discovery_data_ok else R.string.discovery_data_missing))
                }
            }
        }
        Text(stringResource(if (ready) R.string.wizard_ready else R.string.wizard_waiting))
        OutlinedTextField(host, { host = it }, label = { Text(stringResource(R.string.wizard_host)) },
            singleLine = true, enabled = !busy, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(port, { port = it }, label = { Text(stringResource(R.string.wizard_port)) },
            singleLine = true, enabled = !busy, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(apiKey, {apiKey=it}, label={Text(stringResource(R.string.printer_api_key))},
            visualTransformation=PasswordVisualTransformation(),singleLine=true,enabled=!busy,
            modifier=Modifier.fillMaxWidth())
        Text(stringResource(R.string.printer_key_help))
        if (!idle) Text(stringResource(R.string.wizard_idle))
        Button(enabled = valid && idle && !busy && !scanning, onClick = {
            onSave(host.trim(), port.toInt(), apiKey)
            apiKey=""
        }) { Text(stringResource(R.string.printer_test_save)) }
        if (saved) Text(stringResource(R.string.printer_setup_success))
        Text(status)
        Button(enabled = ready && !busy, onClick = onContinue) {
            Text(stringResource(R.string.wizard_continue))
        }
        TextButton(enabled = !busy, onClick = onReconnect) { Text(stringResource(R.string.wizard_reconnect)) }
        TextButton(enabled = !busy, onClick = onSkip) { Text(stringResource(R.string.wizard_skip)) }
    }
}
