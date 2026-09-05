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
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CheckboxDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.ui.components.ScreenColumn
import com.alphastudio.snapheateru1.ui.components.SectionTitle
import com.alphastudio.snapheateru1.ui.components.StatusRow
import com.alphastudio.snapheateru1.ui.theme.StatusColors

@Suppress("UNUSED_PARAMETER")
@Composable
fun SafetyScreen(
    snapshot: HeaterSnapshot,
    onApplySafety: (HeaterSnapshot, Boolean, Boolean) -> Unit,
) {
    val moonrakerGateReady = snapshot.localOnlyMode || snapshot.moonrakerVerified

    ScreenColumn {
        SectionTitle(stringResource(R.string.safety_title), stringResource(R.string.safety_subtitle))

        SafetyStep(stringResource(R.string.safety_step_gpio), snapshot.hardwareMapName == "panda_breath_accepted")
        SafetyStep(stringResource(R.string.safety_step_heater_build), snapshot.heaterOutputBuildEnabled)
        SafetyStep(stringResource(R.string.safety_step_triac), snapshot.fanTriacControl && snapshot.zeroCrossGpio == 7)
        SafetyStep(stringResource(R.string.safety_step_latch_ready), snapshot.outputSafetyLatchReady)
        SafetyStep(stringResource(R.string.safety_step_moonraker), moonrakerGateReady)

        Card(
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier.fillMaxWidth().padding(14.dp),
            ) {
                Text(stringResource(R.string.safety_current_gate), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow(
                    stringResource(R.string.label_heater),
                    if (snapshot.heaterOutputBuildEnabled) stringResource(R.string.value_enabled) else stringResource(R.string.value_disabled),
                    strong = true,
                    valueColor = if (snapshot.heaterOutputBuildEnabled) StatusColors.Warning else StatusColors.Good,
                )
                StatusRow(stringResource(R.string.safety_hardware_map), snapshot.hardwareMapName, valueColor = StatusColors.Good)
                StatusRow(stringResource(R.string.safety_readiness), "${snapshot.safetyScore}%", valueColor = safetyColor(snapshot.safetyScore))
                StatusRow(stringResource(R.string.safety_setup_validation), if (snapshot.setupValidationPassed) stringResource(R.string.common_ready) else stringResource(R.string.common_pending), valueColor = if (snapshot.setupValidationPassed) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.dashboard_output_latch), if (snapshot.outputSafetyLatchReady) stringResource(R.string.common_ready) else stringResource(R.string.common_blocked), valueColor = if (snapshot.outputSafetyLatchReady) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.safety_latch_ready_label), if (snapshot.outputSafetyLatchReady) stringResource(R.string.common_ready) else stringResource(R.string.common_blocked), valueColor = if (snapshot.outputSafetyLatchReady) StatusColors.Good else StatusColors.Warning)
                Text(
                    stringResource(R.string.safety_body),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
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
                Text(stringResource(R.string.safety_latch_workflow), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                StatusRow(stringResource(R.string.safety_sensors_verified), yesNo(snapshot.sensorsVerified), valueColor = verifyColor(snapshot.sensorsVerified))
                StatusRow(stringResource(R.string.safety_fan_driver), if (snapshot.fanTriacControl) "GPIO${snapshot.fanGpio} + ZC GPIO${snapshot.zeroCrossGpio}" else "Plain GPIO${snapshot.fanGpio}", valueColor = if (snapshot.fanTriacControl) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.safety_fan_output_verified), yesNo(snapshot.fanOutputVerified), valueColor = verifyColor(snapshot.fanOutputVerified))
                StatusRow(stringResource(R.string.safety_heater_output), "GPIO${snapshot.heaterGpio}", valueColor = if (snapshot.heaterOutputVerified) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.safety_heater_output_verified), yesNo(snapshot.heaterOutputVerified), valueColor = verifyColor(snapshot.heaterOutputVerified))
                StatusRow(
                    stringResource(R.string.safety_moonraker_verified),
                    when {
                        snapshot.moonrakerVerified -> stringResource(R.string.common_yes)
                        snapshot.localOnlyMode -> stringResource(R.string.common_not_required)
                        else -> stringResource(R.string.common_no)
                    },
                    valueColor = if (moonrakerGateReady) StatusColors.Good else StatusColors.Warning,
                )
                StatusRow(stringResource(R.string.safety_arm_prerequisites), if (snapshot.outputSafetyLatchReady) stringResource(R.string.common_ready) else stringResource(R.string.common_incomplete), valueColor = if (snapshot.outputSafetyLatchReady) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.safety_gpio_probe_api), if (snapshot.gpioProbeLocked) stringResource(R.string.value_disabled) else stringResource(R.string.value_enabled), valueColor = if (snapshot.gpioProbeLocked) StatusColors.Good else StatusColors.Warning)
                Text(
                    stringResource(R.string.safety_workflow_note),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun SafetyStep(label: String, checked: Boolean) {
    Card(
        shape = RoundedCornerShape(8.dp),
        border = androidx.compose.foundation.BorderStroke(
            1.dp,
            if (checked) StatusColors.Good.copy(alpha = 0.5f) else StatusColors.Warning.copy(alpha = 0.5f),
        ),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
    ) {
        androidx.compose.foundation.layout.Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            modifier = Modifier.fillMaxWidth().padding(10.dp),
        ) {
            Checkbox(
                checked = checked,
                onCheckedChange = null,
                colors = CheckboxDefaults.colors(
                    checkedColor = StatusColors.Good,
                    uncheckedColor = StatusColors.Warning,
                    checkmarkColor = MaterialTheme.colorScheme.background,
                ),
            )
            Text(label, modifier = Modifier.weight(1f), color = if (checked) StatusColors.Good else StatusColors.Warning)
        }
    }
}

private fun safetyColor(score: Int) = when {
    score >= 80 -> StatusColors.Good
    score >= 50 -> StatusColors.Warning
    else -> StatusColors.Danger
}

@Composable
private fun yesNo(value: Boolean) = if (value) stringResource(R.string.common_yes) else stringResource(R.string.common_no)

private fun verifyColor(value: Boolean) = if (value) StatusColors.Good else StatusColors.Warning
