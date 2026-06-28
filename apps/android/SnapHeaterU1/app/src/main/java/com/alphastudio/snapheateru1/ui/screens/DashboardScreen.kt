/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.shape.RoundedCornerShape
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.MetricTile
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusPill
import com.alphastudio.snapheateru1.ui.components.StatusRow
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun DashboardScreen(snapshot: HeaterSnapshot) {
    ScreenColumn {
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                MetricTile(stringResource(R.string.dashboard_chamber), "${snapshot.chamberC} C", stringResource(R.string.dashboard_chamber_detail), chamberColor(snapshot))
                MetricTile(stringResource(R.string.dashboard_target), "${snapshot.targetC} C", stringResource(R.string.dashboard_target_detail), StatusColors.Warning)
            }
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                MetricTile(stringResource(R.string.dashboard_ptc), "${snapshot.ptcC} C", stringResource(R.string.dashboard_ptc_detail), ptcColor(snapshot.ptcC))
                MetricTile(stringResource(R.string.dashboard_safety), "${snapshot.safetyScore}%", stringResource(R.string.dashboard_safety_detail), safetyColor(snapshot.safetyScore))
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                SectionTitle(stringResource(R.string.dashboard_live_state), stringResource(R.string.dashboard_live_state_subtitle))
                StatusRow(stringResource(R.string.dashboard_ble), snapshot.ble, strong = true)
                StatusRow(stringResource(R.string.dashboard_moonraker), snapshot.moonraker, valueColor = StatusColors.Warning)
                StatusRow(stringResource(R.string.label_fan), if (snapshot.fanOn) stringResource(R.string.common_on) else stringResource(R.string.common_off), valueColor = if (snapshot.fanOn) StatusColors.Good else StatusColors.Normal)
                StatusRow(
                    stringResource(R.string.dashboard_heater_build),
                    if (snapshot.heaterOutputBuildEnabled) stringResource(R.string.value_enabled) else stringResource(R.string.value_disabled),
                    strong = true,
                    valueColor = if (snapshot.heaterOutputBuildEnabled) StatusColors.Warning else StatusColors.Good,
                )
                StatusRow(stringResource(R.string.dashboard_output_latch), if (snapshot.outputSafetyLatchArmed) stringResource(R.string.common_armed) else stringResource(R.string.common_not_armed), valueColor = if (snapshot.outputSafetyLatchArmed) StatusColors.Warning else StatusColors.Good)
                StatusRow(stringResource(R.string.dashboard_fan_driver), if (snapshot.fanTriacControl) "TRIAC / ZC" else "GPIO", valueColor = if (snapshot.fanTriacControl) StatusColors.Good else StatusColors.Warning)
                LinearProgressIndicator(
                    progress = { snapshot.safetyScore / 100f },
                    modifier = Modifier.fillMaxWidth(),
                    color = safetyColor(snapshot.safetyScore),
                    trackColor = MaterialTheme.colorScheme.outlineVariant,
                )
            }
        }

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                SectionTitle(stringResource(R.string.dashboard_printer_chamber), stringResource(R.string.dashboard_printer_chamber_subtitle))
                StatusRow(stringResource(R.string.dashboard_printer), snapshot.printerState, valueColor = StatusColors.Normal)
                StatusRow(stringResource(R.string.dashboard_progress), "${snapshot.printProgressPct}%")
                StatusRow(stringResource(R.string.dashboard_active_material), snapshot.activeMaterial)
                StatusRow(stringResource(R.string.dashboard_warmup_eta), "${snapshot.warmupEtaMin} min", valueColor = StatusColors.Warning)
                StatusRow(stringResource(R.string.label_heat_soak), if (snapshot.heatSoakReady) stringResource(R.string.common_ready) else stringResource(R.string.common_waiting), valueColor = if (snapshot.heatSoakReady) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.dashboard_stability), "${snapshot.stabilityScore}%", valueColor = safetyColor(snapshot.stabilityScore))
                StatusRow(stringResource(R.string.dashboard_print_risk), "${snapshot.printRiskScore}% / ${snapshot.printRiskMessage}", valueColor = riskColor(snapshot.printRiskScore))
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
                SectionTitle(stringResource(R.string.dashboard_post_service), stringResource(R.string.dashboard_post_service_subtitle))
                StatusRow(stringResource(R.string.dashboard_virtual_door), if (snapshot.virtualDoorOpen) stringResource(R.string.dashboard_open_detected) else stringResource(R.string.dashboard_no_event), valueColor = if (snapshot.virtualDoorOpen) StatusColors.Warning else StatusColors.Good)
                StatusRow(stringResource(R.string.dashboard_filter_life), "${snapshot.filterLifePct}%", valueColor = serviceColor(snapshot.filterLifePct))
                StatusRow(stringResource(R.string.dashboard_heater_wear), "${snapshot.heaterWearPct}%", valueColor = serviceColor(snapshot.heaterWearPct))
                StatusRow(stringResource(R.string.dashboard_session_energy), "${snapshot.sessionEnergyWh} Wh")
                StatusRow(stringResource(R.string.dashboard_estimated_total), "${snapshot.estimatedEnergyWh} Wh")
            }
        }

        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            StatusPill(snapshot.ble, StatusColors.Normal)
            StatusPill(
                "${snapshot.hardwareMapName}: H${snapshot.heaterGpio} F${snapshot.fanGpio} ZC${snapshot.zeroCrossGpio}",
                StatusColors.Good,
            )
            StatusPill(stringResource(R.string.status_local_control_only), StatusColors.Normal)
        }

        Text(
            stringResource(R.string.dashboard_footer),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontWeight = FontWeight.Medium,
        )
    }
}

private fun chamberColor(snapshot: HeaterSnapshot) = when {
    snapshot.chamberC >= snapshot.targetC - 2 && snapshot.chamberC <= snapshot.targetC + 2 -> StatusColors.Good
    snapshot.chamberC > snapshot.targetC + 5 -> StatusColors.Warning
    else -> StatusColors.Normal
}

private fun ptcColor(ptcC: Int) = when {
    ptcC >= 90 -> StatusColors.Danger
    ptcC >= 75 -> StatusColors.Warning
    else -> StatusColors.Normal
}

private fun safetyColor(score: Int) = when {
    score >= 80 -> StatusColors.Good
    score >= 50 -> StatusColors.Warning
    else -> StatusColors.Danger
}

private fun riskColor(score: Int) = when {
    score >= 70 -> StatusColors.Danger
    score >= 40 -> StatusColors.Warning
    else -> StatusColors.Good
}

private fun serviceColor(percent: Int) = when {
    percent >= 90 -> StatusColors.Danger
    percent >= 70 -> StatusColors.Warning
    else -> StatusColors.Good
}
