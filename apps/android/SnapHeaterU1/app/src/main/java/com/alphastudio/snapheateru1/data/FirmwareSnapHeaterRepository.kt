/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import kotlin.math.ceil
import org.json.JSONObject

class FirmwareSnapHeaterRepository(
    private val client: SnapHeaterApiClient,
) : SnapHeaterRepository {
    override fun snapshot(): HeaterSnapshot = client.status().toHeaterSnapshot()

    override fun setMode(mode: AppMode): HeaterSnapshot {
        val payload = JSONObject()
            .put("work_mode", mode.toFirmwareWorkMode())
            .put("work_on", mode != AppMode.SafeStop)
        return client.postSettings(payload).toHeaterSnapshot()
    }

    override fun setTarget(targetC: Int): HeaterSnapshot {
        return client.postSettings(JSONObject().put("set_temp", targetC)).toHeaterSnapshot()
    }

    override fun applySettings(snapshot: HeaterSnapshot): HeaterSnapshot {
        return client.postSettings(snapshot.toSettingsPayload()).toHeaterSnapshot()
    }

    fun checkHealth(): HeaterSnapshot {
        client.health()
        return snapshot()
    }
}

private fun HeaterSnapshot.toSettingsPayload(): JSONObject {
    val payload = JSONObject()
        .put("work_mode", mode.toFirmwareWorkMode())
        .put("set_temp", targetC)
        .put("auto_material_profile_enabled", autoMaterialProfileEnabled)
        .put("material_mismatch_warning_enabled", mismatchWarningEnabled)
        .put("pla_protection_enabled", plaProtectionEnabled)
        .put("anti_warp_enabled", antiWarpEnabled)
        .put("large_print_protection_enabled", largePrintProtectionEnabled)
        .put("safe_overnight_enabled", safeOvernightEnabled)
        .put("pause_hold_enabled", pauseHoldEnabled)
        .put("smart_resume_enabled", smartResumeEnabled)
        .put("start_print_warning_enabled", startPrintWarningEnabled)
        .put("airflow_detection_enabled", airflowDetectionEnabled)
        .put("temp_history_enabled", tempHistoryEnabled)
        .put("incident_report_enabled", incidentReportEnabled)
        .put("local_recipes_enabled", localRecipesEnabled)
        .put("scheduled_preheat_enabled", scheduledPreheatEnabled)
        .put("local_only_mode", localOnlyMode)
        .put("demo_mode_enabled", demoModeEnabled)
        .put("contest_showcase_mode_enabled", showcaseModeEnabled)
        .put("symbiont_mode_enabled", symbiontModeEnabled)
        .put("symbiont_ventilation_allowed", symbiontVentilationAllowed)

    when (mode) {
        AppMode.AutoStandby -> payload.put("work_on", true)
        AppMode.ManualHold -> payload.put("work_on", true)
        AppMode.Preheat -> {
            payload
                .put("work_on", true)
                .put("preheat_running", true)
                .put("preheat_target", targetC)
                .put("preheat_hold_min", preheatHeatSoakMin)
        }
        AppMode.Drying -> {
            payload
                .put("work_on", true)
                .put("isrunning", true)
                .put("custom_temp", targetC)
                .put("custom_timer", ceil(dryingTimeMin / 60.0).toInt().coerceAtLeast(1))
        }
        AppMode.Tempering -> {
            payload
                .put("work_on", true)
                .put("tempering_enabled", true)
                .put("tempering_end_temp", targetC)
                .put("tempering_duration_min", temperingDurationMin)
        }
        AppMode.SafeStop -> {
            payload
                .put("work_on", false)
                .put("preheat_running", false)
                .put("isrunning", false)
                .put("dryout_running", false)
                .put("health_test_running", false)
                .put("disarm_output_safety_latch", true)
        }
    }

    return payload
}

private fun JSONObject.toHeaterSnapshot(): HeaterSnapshot {
    val settings = optJSONObject("settings") ?: JSONObject()
    val runtime = optJSONObject("runtime") ?: JSONObject()
    val printer = optJSONObject("printer") ?: JSONObject()
    val pins = optJSONObject("hardware_pins") ?: JSONObject()

    val mode = firmwareModeToAppMode(
        workMode = settings.optInt("work_mode", 1),
        preheatRunning = settings.optBoolean("preheat_running", false),
        dryingRunning = settings.optBoolean("isrunning", false),
        temperingPhase = settings.optInt("tempering_phase", 0),
    )
    val material = settings.optString("material_profile_name", "Custom").ifBlank { "Custom" }
    val connected = printer.optBoolean("moonraker_connected", false)
    val klippyReady = printer.optBoolean("klippy_ready", false)
    val progress = (printer.optDouble("progress", 0.0) * 100.0).toInt().coerceIn(0, 100)

    return HeaterSnapshot(
        firmwareVersion = optString("fw_version", "unknown"),
        chamberC = runtime.optDouble("warehouse_temper", 0.0).toInt(),
        ptcC = runtime.optDouble("ptc_temp", 0.0).toInt(),
        targetC = settings.optInt("set_temp", 45),
        safetyScore = runtime.optInt("safety_score", settings.optInt("safety_score", 0)),
        setupValidationPassed = runtime.optBoolean("setup_validation_passed", settings.optBoolean("setup_validation_passed", false)),
        outputSafetyLatchArmed = settings.optBoolean("output_safety_latch_armed", false),
        heaterOutputBuildEnabled = optBoolean("heater_output_build_enabled", false),
        heaterLocked = !optBoolean("heater_output_build_enabled", false),
        gpioProbeLocked = !optBoolean("gpio_probe_build_enabled", false),
        fanOn = runtime.optBoolean("fan_output_on", false),
        moonraker = if (connected) "Connected / ${if (klippyReady) "ready" else "waiting"}" else "Read-only / waiting",
        ble = "LAN connected",
        material = material,
        printerState = printer.optString("normalized_state", "standby").ifBlank { "standby" },
        printProgressPct = progress,
        activeTool = printer.optString("active_tool_object", "extruder0").ifBlank { "extruder0" },
        activeMaterial = printer.optString("active_material", material).ifBlank { material },
        materialAdvice = runtime.optString("material_advice", "$material profile selected").ifBlank { "$material profile selected" },
        mode = mode,
        manualFanAssist = runtime.optBoolean("fan_output_on", false),
        preheatHeatSoakMin = settings.optInt("preheat_hold_min", 15),
        dryingTimeMin = settings.optInt("custom_timer", 2).coerceAtLeast(1) * 60,
        temperingDurationMin = settings.optInt("tempering_duration_min", 45),
        lastConfirmedSettings = "Synced from firmware",
        warmupEtaMin = (runtime.optInt("warmup_eta_sec", 0) / 60).coerceAtLeast(0),
        heatSoakEnabled = settings.optBoolean("heat_soak_enabled", true),
        heatSoakReady = runtime.optBoolean("heat_soak_ready", false),
        stabilityScore = runtime.optInt("stability_score_pct", 0),
        printRiskScore = runtime.optInt("print_risk_score", 0),
        printRiskMessage = runtime.optString("print_risk_message", "No active risk message").ifBlank { "No active risk message" },
        virtualDoorDetectionEnabled = settings.optBoolean("virtual_door_detection_enabled", true),
        virtualDoorOpen = settings.optBoolean("virtual_door_open", false),
        virtualDoorPending = settings.optBoolean("virtual_door_open_pending", false),
        autoMaterialProfileEnabled = settings.optBoolean("auto_material_profile_enabled", true),
        mismatchWarningEnabled = settings.optBoolean("material_mismatch_warning_enabled", true),
        plaProtectionEnabled = settings.optBoolean("pla_protection_enabled", true),
        antiWarpEnabled = settings.optBoolean("anti_warp_enabled", true),
        largePrintProtectionEnabled = settings.optBoolean("large_print_protection_enabled", true),
        safeOvernightEnabled = settings.optBoolean("safe_overnight_enabled", false),
        pauseHoldEnabled = settings.optBoolean("pause_hold_enabled", true),
        smartResumeEnabled = settings.optBoolean("smart_resume_enabled", true),
        startPrintWarningEnabled = settings.optBoolean("start_print_warning_enabled", true),
        airflowDetectionEnabled = settings.optBoolean("airflow_detection_enabled", true),
        airflowWarningPending = runtime.optBoolean("airflow_warning_pending", false),
        filterLifePct = runtime.optInt("filter_life_pct", 0),
        filterWarningPending = settings.optBoolean("filter_life_warning_pending", false),
        heaterWearPct = runtime.optInt("heater_wear_pct", 0),
        estimatedEnergyWh = runtime.optDouble("estimated_energy_wh", 0.0).toInt(),
        sessionEnergyWh = runtime.optDouble("session_energy_wh", 0.0).toInt(),
        tempHistoryEnabled = settings.optBoolean("temp_history_enabled", true),
        incidentReportEnabled = settings.optBoolean("incident_report_enabled", true),
        localRecipesEnabled = settings.optBoolean("local_recipes_enabled", true),
        activeRecipeName = settings.optString("active_recipe_name", "Recipe").ifBlank { "Recipe" },
        scheduledPreheatEnabled = settings.optBoolean("scheduled_preheat_enabled", false),
        localOnlyMode = settings.optBoolean("local_only_mode", true),
        demoModeEnabled = settings.optBoolean("demo_mode_enabled", false),
        showcaseModeEnabled = settings.optBoolean("contest_showcase_mode_enabled", false),
        symbiontModeEnabled = settings.optBoolean("symbiont_mode_enabled", false),
        symbiontVentilationAllowed = settings.optBoolean("symbiont_ventilation_allowed", false),
        otaRollbackReady = settings.optBoolean("ota_rollback_placeholder_enabled", false),
        hardwareMapName = pins.optString("map_name", "panda_breath_accepted"),
        hardwareSafetyState = pins.optString("safety_state", "heater_output_build_enabled_runtime_latch_required"),
        heaterGpio = pins.optInt("heater_gpio", 18),
        fanGpio = pins.optInt("fan_gpio", 3),
        zeroCrossGpio = pins.optInt("zero_cross_gpio", 7),
        chamberAdcChannel = pins.optInt("chamber_adc_channel", 0),
        ptcAdcChannel = pins.optInt("ptc_adc_channel", 1),
        ledAutoGpio = pins.optInt("led_auto_gpio", 6),
        ledOnGpio = pins.optInt("led_on_gpio", 5),
        ledOffGpio = pins.optInt("led_off_gpio", 4),
        fanTriacControl = pins.optBoolean("fan_triac_control", true),
        acMainsHz = pins.optInt("ac_mains_hz", 50),
        fanTriacRunPercent = pins.optInt("fan_triac_run_percent", 100),
        fanTriacMinDelayUs = pins.optInt("fan_triac_min_delay_us", 200),
        fanTriacGatePulseUs = pins.optInt("fan_triac_gate_pulse_us", 100),
    )
}

private fun AppMode.toFirmwareWorkMode(): Int = when (this) {
    AppMode.AutoStandby -> 1
    AppMode.ManualHold -> 2
    AppMode.Drying -> 3
    AppMode.Preheat -> 4
    AppMode.Tempering -> 2
    AppMode.SafeStop -> 2
}

private fun firmwareModeToAppMode(
    workMode: Int,
    preheatRunning: Boolean,
    dryingRunning: Boolean,
    temperingPhase: Int,
): AppMode = when {
    temperingPhase > 0 -> AppMode.Tempering
    preheatRunning || workMode == 4 -> AppMode.Preheat
    dryingRunning || workMode == 3 -> AppMode.Drying
    workMode == 2 -> AppMode.ManualHold
    else -> AppMode.AutoStandby
}
