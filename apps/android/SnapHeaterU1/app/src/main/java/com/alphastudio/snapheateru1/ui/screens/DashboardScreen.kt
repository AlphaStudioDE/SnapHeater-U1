/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.shape.RoundedCornerShape
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.HomeLayoutStyle
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusPill
import com.alphastudio.snapheateru1.ui.components.StatusRow
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Composable
fun DashboardScreen(
    snapshot: HeaterSnapshot,
    homeLayoutStyle: HomeLayoutStyle = HomeLayoutStyle.Default,
    onStart: () -> Unit = {},
    onSafeStop: () -> Unit = {},
) {
    when (homeLayoutStyle) {
        HomeLayoutStyle.SimpleDaily -> SimpleDailyDashboard(snapshot, onStart, onSafeStop)
        HomeLayoutStyle.BentoDashboard -> BentoDashboard(snapshot)
        HomeLayoutStyle.FocusDial -> FocusDialDashboard(snapshot, onStart)
    }
}

@Composable
private fun SimpleDailyDashboard(snapshot: HeaterSnapshot, onStart: () -> Unit, onSafeStop: () -> Unit) {
    ScreenColumn {
        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.fillMaxWidth().padding(16.dp),
            ) {
                SectionTitle(stringResource(R.string.dashboard_live_state), stringResource(R.string.dashboard_live_state_subtitle))
                Text("${snapshot.chamberC} C", fontSize = 58.sp, fontWeight = FontWeight.Bold, color = chamberColor(snapshot))
                StatusRow(stringResource(R.string.dashboard_target), "${snapshot.targetC} C", valueColor = StatusColors.Warning)
                StatusRow(stringResource(R.string.dashboard_safety), "${snapshot.safetyScore}%", valueColor = safetyColor(snapshot.safetyScore))
                LinearProgressIndicator(
                    progress = { snapshot.safetyScore / 100f },
                    modifier = Modifier.fillMaxWidth(),
                    color = safetyColor(snapshot.safetyScore),
                    trackColor = MaterialTheme.colorScheme.outlineVariant,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
                    Button(onClick = onStart, modifier = Modifier.weight(1f)) {
                        Text(stringResource(R.string.dashboard_action_start))
                    }
                    OutlinedButton(onClick = onSafeStop, modifier = Modifier.weight(1f)) {
                        Text(stringResource(R.string.dashboard_action_safe_stop))
                    }
                }
            }
        }

        LiveStateCard(snapshot)
        PrinterChamberCard(snapshot)
        FooterPills(snapshot)
    }
}

@Composable
private fun BentoDashboard(snapshot: HeaterSnapshot) {
    ScreenColumn {
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
            LargeBentoTile(
                label = stringResource(R.string.dashboard_chamber),
                value = "${snapshot.chamberC} C",
                detail = stringResource(R.string.dashboard_chamber_detail),
                accent = chamberColor(snapshot),
                modifier = Modifier.weight(1.25f),
            )
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                SmallBentoTile(stringResource(R.string.dashboard_target), "${snapshot.targetC} C", StatusColors.Warning)
                SmallBentoTile(stringResource(R.string.dashboard_safety), "${snapshot.safetyScore}%", safetyColor(snapshot.safetyScore))
            }
        }

        Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
            SmallBentoTile(stringResource(R.string.dashboard_ptc), "${snapshot.ptcC} C", ptcColor(snapshot.ptcC), Modifier.weight(1f))
            SmallBentoTile(stringResource(R.string.dashboard_warmup_eta), "${snapshot.warmupEtaMin} min", StatusColors.Warning, Modifier.weight(1f))
        }

        LiveStateCard(snapshot)
        PrinterChamberCard(snapshot)
        PostServiceCard(snapshot)
        FooterPills(snapshot)
    }
}

@Composable
private fun FocusDialDashboard(snapshot: HeaterSnapshot, onStart: () -> Unit) {
    ScreenColumn {
        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(14.dp),
                modifier = Modifier.fillMaxWidth().padding(16.dp),
            ) {
                SectionTitle(stringResource(R.string.dashboard_chamber), stringResource(R.string.dashboard_chamber_detail))
                TemperatureDial(
                    currentC = snapshot.chamberC,
                    targetC = snapshot.targetC,
                    accent = chamberColor(snapshot),
                    modifier = Modifier.fillMaxWidth(0.82f).aspectRatio(1f),
                )
                SmallBentoTile(
                    label = stringResource(R.string.dashboard_target),
                    value = "${snapshot.targetC} C",
                    accent = StatusColors.Warning,
                    modifier = Modifier.fillMaxWidth(),
                )
                Button(onClick = onStart, modifier = Modifier.fillMaxWidth().height(52.dp)) {
                    Text(stringResource(R.string.dashboard_action_start))
                }
            }
        }

        Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
            SmallBentoTile(stringResource(R.string.dashboard_safety), "${snapshot.safetyScore}%", safetyColor(snapshot.safetyScore), Modifier.weight(1f))
            SmallBentoTile(stringResource(R.string.dashboard_ptc), "${snapshot.ptcC} C", ptcColor(snapshot.ptcC), Modifier.weight(1f))
        }

        LiveStateCard(snapshot)
        FooterPills(snapshot)
    }
}

@Composable
private fun LiveStateCard(snapshot: HeaterSnapshot) {
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
            StatusRow(stringResource(R.string.dashboard_output_latch), if (snapshot.outputSafetyLatchReady) stringResource(R.string.common_ready) else stringResource(R.string.common_blocked), valueColor = if (snapshot.outputSafetyLatchReady) StatusColors.Good else StatusColors.Warning)
            StatusRow(stringResource(R.string.dashboard_fan_driver), if (snapshot.fanTriacControl) "TRIAC / ZC" else "GPIO", valueColor = if (snapshot.fanTriacControl) StatusColors.Good else StatusColors.Warning)
            LinearProgressIndicator(
                progress = { snapshot.safetyScore / 100f },
                modifier = Modifier.fillMaxWidth(),
                color = safetyColor(snapshot.safetyScore),
                trackColor = MaterialTheme.colorScheme.outlineVariant,
            )
        }
    }
}

@Composable
private fun PrinterChamberCard(snapshot: HeaterSnapshot) {
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
}

@Composable
private fun PostServiceCard(snapshot: HeaterSnapshot) {
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
}

@Composable
private fun FooterPills(snapshot: HeaterSnapshot) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        StatusPill(snapshot.ble, StatusColors.Normal)
        StatusPill(
            "${snapshot.hardwareMapName}: H${snapshot.heaterGpio} F${snapshot.fanGpio} ZC${snapshot.zeroCrossGpio}",
            StatusColors.Good,
        )
        StatusPill(stringResource(R.string.status_local_control_only), StatusColors.Normal)
    }
}

@Composable
private fun LargeBentoTile(label: String, value: String, detail: String, accent: androidx.compose.ui.graphics.Color, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier.defaultMinSize(minHeight = 176.dp),
        shape = RoundedCornerShape(8.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, accent.copy(alpha = 0.35f)),
    ) {
        Column(
            verticalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.fillMaxSize().padding(14.dp),
        ) {
            Text(label, style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Column {
                Text(value, fontSize = 42.sp, fontWeight = FontWeight.Bold, color = accent)
                Text(detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun SmallBentoTile(label: String, value: String, accent: androidx.compose.ui.graphics.Color, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier.defaultMinSize(minHeight = 82.dp),
        shape = RoundedCornerShape(8.dp),
        color = MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(1.dp, accent.copy(alpha = 0.24f)),
    ) {
        Column(
            verticalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(12.dp),
        ) {
            Text(label, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(value, fontSize = 24.sp, fontWeight = FontWeight.Bold, color = accent)
        }
    }
}

@Composable
private fun TemperatureDial(currentC: Int, targetC: Int, accent: androidx.compose.ui.graphics.Color, modifier: Modifier = Modifier) {
    val trackColor = MaterialTheme.colorScheme.outlineVariant
    val textColor = MaterialTheme.colorScheme.onSurface
    val targetColor = StatusColors.Warning
    Box(modifier = modifier, contentAlignment = Alignment.Center) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val stroke = Stroke(width = 18.dp.toPx(), cap = StrokeCap.Round)
            val diameter = size.minDimension - stroke.width
            val topLeft = Offset((size.width - diameter) / 2f, (size.height - diameter) / 2f)
            val arcSize = Size(diameter, diameter)
            drawArc(
                color = trackColor,
                startAngle = 140f,
                sweepAngle = 260f,
                useCenter = false,
                topLeft = topLeft,
                size = arcSize,
                style = stroke,
            )
            drawArc(
                color = accent,
                startAngle = 140f,
                sweepAngle = ((currentC.coerceIn(20, 80) - 20) / 60f) * 260f,
                useCenter = false,
                topLeft = topLeft,
                size = arcSize,
                style = stroke,
            )
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text("$currentC C", fontSize = 54.sp, fontWeight = FontWeight.Bold, color = textColor)
            Text("Target $targetC C", style = MaterialTheme.typography.titleSmall, color = targetColor, fontWeight = FontWeight.Bold)
        }
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
