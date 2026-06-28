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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CheckboxDefaults
import androidx.compose.material3.OutlinedButton
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

@Composable
fun SafetyScreen(
    snapshot: HeaterSnapshot,
    onApplySafety: (HeaterSnapshot, Boolean, Boolean) -> Unit,
) {
    val verificationReady = snapshot.heaterOutputVerified && snapshot.fanOutputVerified && snapshot.sensorsVerified
    val latchPrerequisitesReady = verificationReady && (!snapshot.moonrakerVerified || snapshot.setupValidationPassed)
    val confirmSensors = stringResource(R.string.safety_confirm_sensors)
    val confirmFan = stringResource(R.string.safety_confirm_fan)
    val confirmHeater = stringResource(R.string.safety_confirm_heater)
    val confirmMoonraker = stringResource(R.string.safety_confirm_moonraker)

    ScreenColumn {
        SectionTitle(stringResource(R.string.safety_title), stringResource(R.string.safety_subtitle))

        SafetyStep(stringResource(R.string.safety_step_gpio), snapshot.hardwareMapName == "panda_breath_accepted")
        SafetyStep(stringResource(R.string.safety_step_heater_build), snapshot.heaterOutputBuildEnabled)
        SafetyStep(stringResource(R.string.safety_step_triac), snapshot.fanTriacControl && snapshot.zeroCrossGpio == 7)
        SafetyStep(stringResource(R.string.safety_step_latch_ready), snapshot.outputSafetyLatchReady)
        SafetyStep(stringResource(R.string.safety_step_moonraker), snapshot.moonrakerVerified)

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
                StatusRow(stringResource(R.string.dashboard_output_latch), if (snapshot.outputSafetyLatchArmed) stringResource(R.string.common_armed) else stringResource(R.string.common_not_armed), valueColor = if (snapshot.outputSafetyLatchArmed) StatusColors.Warning else StatusColors.Good)
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
                StatusRow(stringResource(R.string.safety_moonraker_verified), yesNo(snapshot.moonrakerVerified), valueColor = verifyColor(snapshot.moonrakerVerified))
                StatusRow(stringResource(R.string.safety_arm_prerequisites), if (latchPrerequisitesReady) stringResource(R.string.common_ready) else stringResource(R.string.common_incomplete), valueColor = if (latchPrerequisitesReady) StatusColors.Good else StatusColors.Warning)
                StatusRow(stringResource(R.string.safety_gpio_probe_api), if (snapshot.gpioProbeLocked) stringResource(R.string.value_disabled) else stringResource(R.string.value_enabled), valueColor = if (snapshot.gpioProbeLocked) StatusColors.Good else StatusColors.Warning)
                Text(
                    stringResource(R.string.safety_workflow_note),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )

                SafetyAction(confirmSensors, snapshot.sensorsVerified) {
                    onApplySafety(snapshot.copy(sensorsVerified = true), false, false)
                }
                SafetyAction(confirmFan, snapshot.fanOutputVerified) {
                    onApplySafety(snapshot.copy(fanOutputVerified = true), false, false)
                }
                SafetyAction(confirmHeater, snapshot.heaterOutputVerified) {
                    onApplySafety(snapshot.copy(heaterOutputVerified = true), false, false)
                }
                SafetyAction(confirmMoonraker, snapshot.moonrakerVerified) {
                    onApplySafety(snapshot.copy(moonrakerVerified = true), false, false)
                }

                Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
                    Button(
                        onClick = { onApplySafety(snapshot, true, false) },
                        enabled = latchPrerequisitesReady && !snapshot.outputSafetyLatchArmed,
                        modifier = Modifier.weight(1f),
                    ) {
                        Text(stringResource(R.string.safety_arm_latch))
                    }
                    OutlinedButton(
                        onClick = { onApplySafety(snapshot, false, true) },
                        enabled = snapshot.outputSafetyLatchArmed,
                        modifier = Modifier.weight(1f),
                    ) {
                        Text(stringResource(R.string.safety_disarm))
                    }
                }
            }
        }
    }
}

@Composable
private fun SafetyAction(label: String, done: Boolean, onClick: () -> Unit) {
    OutlinedButton(
        onClick = onClick,
        enabled = !done,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Text(if (done) stringResource(R.string.safety_done, label) else label)
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
