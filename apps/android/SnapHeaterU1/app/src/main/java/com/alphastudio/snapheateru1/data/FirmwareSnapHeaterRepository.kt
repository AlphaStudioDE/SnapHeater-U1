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
    private var leaseId: String = ""
    private var revision: Long = -1

    override fun snapshot(): HeaterSnapshot {
        if (leaseId.isNotBlank()) client.heartbeat(leaseId)
        return rememberControl(client.status().toHeaterSnapshot())
    }

    private fun command(payload: JSONObject): HeaterSnapshot {
        if (leaseId.isNotBlank()) payload.put("lease_id", leaseId)
        if (revision >= 0) payload.put("expected_revision", revision)
        return rememberControl(client.postSettings(payload).toHeaterSnapshot())
    }

    private fun rememberControl(value: HeaterSnapshot): HeaterSnapshot {
        revision = value.controlStateRevision
        if (value.controlOwner == "rest") {
            if (value.controlLeaseId.isNotBlank()) leaseId = value.controlLeaseId
        } else {
            leaseId = ""
        }
        return value
    }

    override fun setMode(mode: AppMode): HeaterSnapshot {
        val payload = if (mode == AppMode.SafeStop) {
            safeStopPayload()
        } else {
            JSONObject()
                .put("work_mode", mode.toFirmwareWorkMode())
                .put("work_on", true)
                .put("takeover", true)
        }
        return command(payload)
    }

    override fun setTarget(targetC: Int): HeaterSnapshot {
        return command(JSONObject().put("set_temp", targetC).put("takeover", true))
    }

    override fun applySettings(snapshot: HeaterSnapshot): HeaterSnapshot {
        return command(snapshot.toSettingsPayload().put("takeover", true))
    }

    override fun applySafety(snapshot: HeaterSnapshot, armLatch: Boolean, disarmLatch: Boolean): HeaterSnapshot {
        val payload = JSONObject()
            .put("output_safety_latch_enabled", true)
            .put("heater_output_verified", snapshot.heaterOutputVerified)
            .put("fan_output_verified", snapshot.fanOutputVerified)
            .put("sensors_verified", snapshot.sensorsVerified)
            .put("moonraker_verified", snapshot.moonrakerVerified)
        if (armLatch) payload.put("arm_output_safety_latch", true)
        if (disarmLatch) payload.put("disarm_output_safety_latch", true)
        return command(payload)
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
            return safeStopPayload()
        }
    }

    return payload
}

private fun safeStopPayload(): JSONObject =
    JSONObject()
        .put("safe_stop", true)
        .put("work_on", false)
        .put("preheat_running", false)
        .put("isrunning", false)
        .put("dryout_running", false)
        .put("health_test_running", false)
        .put("tempering_enabled", false)
        .put("cancel_tempering", true)
        .put("disarm_output_safety_latch", true)

private fun JSONObject.toHeaterSnapshot(): HeaterSnapshot {
    val settings = optJSONObject("settings") ?: JSONObject()
    val runtime = optJSONObject("runtime") ?: JSONObject()
    val printer = optJSONObject("printer") ?: JSONObject()
    val pins = optJSONObject("hardware_pins") ?: JSONObject()
    val control = optJSONObject("control") ?: JSONObject()

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
        outputSafetyLatchReady = runtime.optBoolean("output_safety_latch_ready", false),
        heaterOutputVerified = settings.optBoolean("heater_output_verified", false),
        fanOutputVerified = settings.optBoolean("fan_output_verified", false),
        sensorsVerified = settings.optBoolean("sensors_verified", false),
        moonrakerVerified = settings.optBoolean("moonraker_verified", false),
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
        showcaseModeEnabled = settings.optBoolean("contest_showcase_mode_enabled", false),
        symbiontModeEnabled = settings.optBoolean("symbiont_mode_enabled", false),
        symbiontVentilationAllowed = settings.optBoolean("symbiont_ventilation_allowed", false),
        otaRollbackReady = optJSONObject("ota")?.optJSONObject("inactive_slot")
            ?.optBoolean("accepted_identity", false) == true,
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
        zeroCrossSignalPresent = runtime.optBoolean("zero_cross_signal_present", false),
        zeroCrossEdgesPerSec = runtime.optInt("zero_cross_edges_per_sec", 0),
        zeroCrossLastPeriodUs = runtime.optInt("zero_cross_last_period_us", 0),
        zeroCrossMinPeriodUs = runtime.optInt("zero_cross_min_period_us", 0),
        zeroCrossMaxPeriodUs = runtime.optInt("zero_cross_max_period_us", 0),
        zeroCrossEdges = runtime.optLong("zero_cross_edges", 0L),
        controlOwner = control.optString("owner", "none"),
        controlStateRevision = control.optLong("state_revision", 0L),
        controlLeaseId = optString("lease_id", ""),
        controlLeaseRemainingMs = control.optLong("lease_remaining_ms", 0L),
    )
}
