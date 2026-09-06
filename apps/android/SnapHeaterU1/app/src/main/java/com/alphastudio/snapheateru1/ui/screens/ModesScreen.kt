/*
 * SnapHeater U1 — Copyright (c) 2026 Damian Borkowski — SPDX-License-Identifier: MIT
 */
package com.alphastudio.snapheateru1.ui.screens

import androidx.compose.material.icons.outlined.*
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import com.alphastudio.snapheateru1.ui.components.ActionLabel
import com.alphastudio.snapheateru1.ui.components.ModeIcon
import androidx.compose.material.icons.Icons

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.requiresPrinter
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.labelRes
import com.alphastudio.snapheateru1.ui.detailRes

@Composable
fun ModesScreen(
    snapshot: HeaterSnapshot,
    heatingAllowed: Boolean,
    safetyWarning: String,
    onMode: (AppMode) -> Unit,
    onConfirmSettings: (HeaterSnapshot) -> Unit,
    printerAllowed: Boolean = true,
    onPrinterSetup: () -> Unit = {},
) {
    // Drafts are local: telemetry polling must not overwrite edits or display
    // an unsubmitted selection as the device's actual mode.
    var selectedName by rememberSaveable { mutableStateOf<String?>(null) }
    var target by rememberSaveable { mutableStateOf(snapshot.targetC.coerceIn(30, 55)) }
    var drying by rememberSaveable { mutableStateOf(snapshot.dryingTimeMin.coerceIn(30, 360)) }
    var soak by rememberSaveable { mutableStateOf(snapshot.preheatHeatSoakMin.coerceIn(5, 45)) }
    var temper by rememberSaveable { mutableStateOf(snapshot.temperingDurationMin.coerceIn(10, 180)) }
    val selected = selectedName?.let { AppMode.valueOf(it) }
    val dryingLimit=snapshot.sessionLimitMin.coerceIn(1,720)
    LaunchedEffect(dryingLimit) { drying=drying.coerceIn(1,dryingLimit) }
    ScreenColumn {
        Text(stringResource(R.string.daily_choose), style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold)
        Text(stringResource(R.string.daily_draft_note), color = MaterialTheme.colorScheme.onSurfaceVariant)
        if (!printerAllowed) {
            Text(stringResource(R.string.wizard_modes_locked))
            TextButton(onClick = onPrinterSetup) { Text(stringResource(R.string.wizard_connect_printer)) }
        }
        if (selected == null) {
            listOf(AppMode.Preheat, AppMode.Drying, AppMode.AutoStandby,
                AppMode.AutoStandbyTempering, AppMode.ManualHold, AppMode.Tempering).forEach { mode ->
                Card(onClick = { selectedName = mode.name }, enabled = !mode.requiresPrinter() || printerAllowed,
                    shape = RoundedCornerShape(20.dp),
                    modifier = Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        ModeIcon(mode, tint = MaterialTheme.colorScheme.primary)
                        Text(stringResource(mode.labelRes()), style = MaterialTheme.typography.titleLarge)
                        Text(stringResource(mode.detailRes()), color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
            }
        } else {
            TextButton(onClick = { selectedName = null }) { ActionLabel(Icons.AutoMirrored.Outlined.ArrowBack, stringResource(R.string.daily_other_mode)) }
            ModeIcon(selected, tint = MaterialTheme.colorScheme.primary)
            Text(stringResource(selected.labelRes()), style = MaterialTheme.typography.headlineSmall)
            Text(stringResource(selected.detailRes()), color = MaterialTheme.colorScheme.onSurfaceVariant)
            Card(shape = RoundedCornerShape(20.dp)) {
                Column(Modifier.fillMaxWidth().padding(20.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
                    ValueStepper(stringResource(R.string.dashboard_target), "$target °C",
                        target > 30, target < 55, { target-- }, { target++ })
                    when (selected) {
                        AppMode.Drying -> Column {
                            Text(stringResource(R.string.drying_session_limit, dryingLimit))
                            ValueStepper(stringResource(R.string.label_drying_time), "$drying min",
                            drying > 1, drying < dryingLimit, { drying = (drying - 15).coerceAtLeast(1) },
                            { drying = (drying + 15).coerceAtMost(dryingLimit) })
                        }
                        AppMode.Preheat -> ValueStepper(stringResource(R.string.label_heat_soak), "$soak min",
                            soak > 5, soak < 45, { soak = (soak - 5).coerceAtLeast(5) },
                            { soak = (soak + 5).coerceAtMost(45) })
                        AppMode.Tempering, AppMode.AutoStandbyTempering -> ValueStepper(stringResource(R.string.label_cooldown_time), "$temper min",
                            temper > 10, temper < 180, { temper = (temper - 5).coerceAtLeast(10) },
                            { temper = (temper + 5).coerceAtMost(180) })
                        else -> Unit
                    }
                }
            }
            if (!heatingAllowed) Text(safetyWarning, color = MaterialTheme.colorScheme.error)
            val modeLabel = stringResource(selected.labelRes())
            Button(
                enabled = heatingAllowed && (!selected.requiresPrinter() || printerAllowed),
                onClick = {
                    onConfirmSettings(snapshot.copy(mode = selected, targetC = target,
                        dryingTimeMin = drying.coerceIn(1,dryingLimit), preheatHeatSoakMin = soak,
                        temperingDurationMin = temper, lastConfirmedSettings = modeLabel))
                },
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp),
            ) { ActionLabel(selected, stringResource(R.string.daily_apply_mode, modeLabel)) }
        }
        OutlinedButton(onClick = { onMode(AppMode.SafeStop) },
            modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) {
            ActionLabel(Icons.Outlined.PowerSettingsNew, stringResource(R.string.daily_stop))
        }
    }
}

@Composable
private fun ValueStepper(label: String, value: String, canDecrease: Boolean, canIncrease: Boolean,
    decrease: () -> Unit, increase: () -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(label, style = MaterialTheme.typography.labelLarge)
        Text(value, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
            OutlinedButton(onClick = decrease, enabled = canDecrease,
                modifier = Modifier.weight(1f).sizeIn(minWidth = 64.dp, minHeight = 48.dp)) {
                ActionLabel(Icons.Outlined.Remove, stringResource(R.string.daily_less))
            }
            Spacer(Modifier.width(12.dp))
            OutlinedButton(onClick = increase, enabled = canIncrease,
                modifier = Modifier.weight(1f).sizeIn(minWidth = 64.dp, minHeight = 48.dp)) {
                ActionLabel(Icons.Outlined.Add, stringResource(R.string.daily_more))
            }
        }
    }
}
