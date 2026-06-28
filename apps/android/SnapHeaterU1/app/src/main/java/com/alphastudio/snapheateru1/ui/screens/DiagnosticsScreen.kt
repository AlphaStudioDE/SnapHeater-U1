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
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.ble.SnapHeaterBleContract
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusRow
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun DiagnosticsScreen(snapshot: HeaterSnapshot) {
    ScreenColumn {
        SectionTitle(stringResource(R.string.diagnostics_title), stringResource(R.string.diagnostics_subtitle))

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                StatusRow(stringResource(R.string.diagnostics_device_name), SnapHeaterBleContract.DeviceName)
                StatusRow(stringResource(R.string.diagnostics_ble_state), snapshot.ble)
                StatusRow(stringResource(R.string.diagnostics_service), SnapHeaterBleContract.ServiceUuid.toString().take(13) + "...")
                StatusRow(stringResource(R.string.diagnostics_firmware), snapshot.firmwareVersion)
                StatusRow(stringResource(R.string.dashboard_heater_build), if (snapshot.heaterOutputBuildEnabled) stringResource(R.string.value_enabled) else stringResource(R.string.value_disabled), valueColor = if (snapshot.heaterOutputBuildEnabled) StatusColors.Warning else StatusColors.Good)
                StatusRow(stringResource(R.string.dashboard_moonraker), snapshot.moonraker, valueColor = StatusColors.Warning)
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text(stringResource(R.string.diagnostics_hardware), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow(stringResource(R.string.diagnostics_map), snapshot.hardwareMapName, valueColor = StatusColors.Good)
                StatusRow(stringResource(R.string.diagnostics_heater_relay), "GPIO${snapshot.heaterGpio}", valueColor = StatusColors.Warning)
                StatusRow(stringResource(R.string.diagnostics_fan_triac_gate), "GPIO${snapshot.fanGpio}", valueColor = StatusColors.Good)
                StatusRow(stringResource(R.string.diagnostics_zero_cross), "GPIO${snapshot.zeroCrossGpio}", valueColor = StatusColors.Good)
                StatusRow(stringResource(R.string.diagnostics_adc), "CH${snapshot.chamberAdcChannel} / CH${snapshot.ptcAdcChannel}")
                StatusRow(stringResource(R.string.diagnostics_leds), "GPIO${snapshot.ledAutoGpio} / GPIO${snapshot.ledOnGpio} / GPIO${snapshot.ledOffGpio}")
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text(stringResource(R.string.diagnostics_fan_timing), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow(stringResource(R.string.diagnostics_driver), if (snapshot.fanTriacControl) "Phase-angle / zero-cross" else "Plain GPIO", valueColor = if (snapshot.fanTriacControl) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.diagnostics_mains), "${snapshot.acMainsHz} Hz")
                StatusRow(stringResource(R.string.diagnostics_run_power), "${snapshot.fanTriacRunPercent}%")
                StatusRow(stringResource(R.string.diagnostics_min_delay), "${snapshot.fanTriacMinDelayUs} us")
                StatusRow(stringResource(R.string.diagnostics_gate_pulse), "${snapshot.fanTriacGatePulseUs} us")
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text(stringResource(R.string.diagnostics_event_log), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                LogLine("00:01", "App started in mock repository mode")
                LogLine("00:03", "Hardware map loaded: ${snapshot.hardwareMapName}")
                LogLine("00:05", "TRIAC fan: GPIO${snapshot.fanGpio} gate, GPIO${snapshot.zeroCrossGpio} zero-cross")
                LogLine("00:08", "Heater build enabled: ${snapshot.heaterOutputBuildEnabled}")
                LogLine("00:11", "Temperature history enabled: ${snapshot.tempHistoryEnabled}")
                LogLine("00:13", "Incident reports enabled: ${snapshot.incidentReportEnabled}")
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text(stringResource(R.string.diagnostics_reports), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow(stringResource(R.string.settings_temperature_history), if (snapshot.tempHistoryEnabled) stringResource(R.string.value_enabled) else stringResource(R.string.value_disabled), valueColor = if (snapshot.tempHistoryEnabled) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.diagnostics_incident_report), if (snapshot.incidentReportEnabled) stringResource(R.string.value_enabled) else stringResource(R.string.value_disabled), valueColor = if (snapshot.incidentReportEnabled) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.diagnostics_airflow_warning), if (snapshot.airflowWarningPending) stringResource(R.string.common_pending) else stringResource(R.string.common_clear), valueColor = if (snapshot.airflowWarningPending) StatusColors.Warning else StatusColors.Good)
                StatusRow(stringResource(R.string.diagnostics_filter_warning), if (snapshot.filterWarningPending) stringResource(R.string.common_pending) else stringResource(R.string.common_clear), valueColor = if (snapshot.filterWarningPending) StatusColors.Warning else StatusColors.Good)
                StatusRow(stringResource(R.string.diagnostics_ota_rollback), if (snapshot.otaRollbackReady) stringResource(R.string.common_available) else stringResource(R.string.common_placeholder))
            }
        }
    }
}

@Composable
private fun LogLine(time: String, message: String) {
    Text(
        "$time  $message",
        style = MaterialTheme.typography.bodySmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}
