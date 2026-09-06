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
    override fun clearFault(): HeaterSnapshot = clearFaultAndVerify({snapshot()},{command(it)})

    override fun snapshot(): HeaterSnapshot {
        if (leaseId.isNotBlank()) {
            // A locally running job can outlive this phone's command lease.
            // Recover via read-only status; never retry an energy command.
            runCatching { client.heartbeat(leaseId) }.onFailure { leaseId = "" }
        }
        return rememberControl(client.status().toHeaterSnapshot())
    }

    override fun acknowledgeVirtualDoor(detectedMs: Long): HeaterSnapshot =
        rememberControl(client.postSettings(JSONObject().put("virtual_door_ack", detectedMs)).toHeaterSnapshot())

    override fun setVirtualDoorDetection(enabled: Boolean): HeaterSnapshot =
        command(JSONObject().put("virtual_door_detection_enabled", enabled).put("takeover", true))

    private fun command(payload: JSONObject): HeaterSnapshot {
        if (leaseId.isNotBlank()) payload.put("lease_id", leaseId)
        if (!payload.has("expected_revision") && revision >= 0) payload.put("expected_revision", revision)
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

    override fun setMode(mode: AppMode): HeaterSnapshot = applySettings(snapshot().copy(mode = mode))

    override fun setTarget(targetC: Int): HeaterSnapshot {
        return command(JSONObject().put("set_temp", targetC).put("takeover", true))
    }
    override fun pauseJob(paused: Boolean, expectedRevision: Long): HeaterSnapshot =
        command(JSONObject().put("pause_job", paused).put("takeover", true).put("expected_revision", expectedRevision))

    override fun configurePrinter(host: String, port: Int, apiKey: String): HeaterSnapshot {
        snapshot()
        val payload=printerConfiguration(host,port,apiKey)
        val id=payload.getJSONObject("printer_setup").getString("id")
        return awaitPrinterSetup(id,command(payload),{snapshot()})
    }

    override fun applySettings(snapshot: HeaterSnapshot): HeaterSnapshot = command(snapshot.jobPayload())

    override fun savePreferences(snapshot: HeaterSnapshot): HeaterSnapshot = command(snapshot.preferencesPayload().put("takeover", true))
    override fun schedulePreheat(snapshot: HeaterSnapshot): HeaterSnapshot = command(snapshot.schedulePayload())

    override fun applySafety(snapshot: HeaterSnapshot, armLatch: Boolean, disarmLatch: Boolean): HeaterSnapshot {
        val payload = JSONObject()
            .put("expected_revision", snapshot.controlStateRevision)
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
        client.verifyAccess() // Authenticated, read-only: status alone does not prove control access.
        return snapshot()
    }
}

private fun JSONObject.toHeaterSnapshot(): HeaterSnapshot {
    val settings = optJSONObject("settings") ?: JSONObject()
    val runtime = optJSONObject("runtime") ?: JSONObject()
    val printer = optJSONObject("printer") ?: JSONObject()
    val pins = optJSONObject("hardware_pins") ?: JSONObject()
    val control = optJSONObject("control") ?: JSONObject()

    val mode = firmwareModeToAppMode(
        workOn = if (settings.has("work_on")) settings.optBoolean("work_on") else null,
        workMode = settings.optInt("work_mode", 1),
        preheatRunning = settings.optBoolean("preheat_running", false),
        dryingRunning = settings.optBoolean("isrunning", false),
        temperingPhase = settings.optInt("tempering_phase", 0),
        temperingEnabled = settings.optBoolean("tempering_enabled", false),
        finishConditioningMode = settings.optInt("finish_conditioning_mode", 1),
    )
    val material = settings.optString("material_profile_name", "Custom").ifBlank { "Custom" }
    val connected = printer.optBoolean("moonraker_connected", false)
    val klippyReady = printer.optBoolean("klippy_ready", false)
    val progress = (printer.optDouble("progress", 0.0) * 100.0).toInt().coerceIn(0, 100)

    return HeaterSnapshot(
        settingsPersistenceKnown=has("settings_pending") && has("settings_persist_ok"),
        settingsPending=optBoolean("settings_pending",false),
        settingsPersistOk=optBoolean("settings_persist_ok",false),
        sessionLimitMin=settings.optInt("manual_session_max_min",720).coerceIn(1,720),
        faultClearSupported=optBoolean("fault_clear_supported",false),
        faultLatched=optBoolean("fault_latched",false),
        faultInhibited=optBoolean("fault_inhibited",false),
        faultReason=optString("fault_reason",""),
        faultClearBlockReason=optString("fault_clear_block_reason",""),
        firmwareVersion = optString("fw_version", "unknown"),
        chamberC = runtime.optDouble("warehouse_temper", 0.0).toInt(),
        ptcC = runtime.optDouble("ptc_temp", 0.0).toInt(),
        historyReadingValid = runtime.optDouble("warehouse_temper", Double.NaN).isFinite() &&
            runtime.optDouble("ptc_temp", Double.NaN).isFinite() &&
            runtime.optString("warehouse_sensor_status")=="ok" && runtime.optString("ptc_sensor_status")=="ok",
        effectiveTargetC = runtime.optInt("heater_effective_target_c", 0),
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
        printerDataReady = printer.optBoolean("data_ready", false),
        deviceId = optString("device_id", ""),
        wifi = wifiSetupStatus(),
        ble = "LAN connected",
        material = material,
        printerState = printer.optString("normalized_state", "standby").ifBlank { "standby" },
        printProgressPct = progress,
        activeTool = printer.optString("active_tool_object", "extruder0").ifBlank { "extruder0" },
        activeMaterial = printer.optString("active_material", material).ifBlank { material },
        materialAdvice = runtime.optString("material_advice", "$material profile selected").ifBlank { "$material profile selected" },
        mode = mode,
        paused = settings.optBoolean("paused", false),
        printerSetupId = optJSONObject("printer_setup")?.optString("id").orEmpty(),
        printerSetupPhase = optJSONObject("printer_setup")?.optString("phase").orEmpty(),
        ventilationStatus = optString("ventilation_status", ""),
        printerApiKeySet = optJSONObject("printer_setup")?.optBoolean("key_set",false) ?: false,
        sensorFreezeWarningMs = runtime.optLong("sensor_freeze_warning_ms", 0),
        sensorFreezeRemainingS = runtime.optInt("sensor_freeze_remaining_s", 0),
        usageAvailable = optBoolean("usage_available", false),
        heaterUsageMs = optLong("usage_heater_ms", 0),
        filterUsageMs = optLong("usage_filter_ms", 0),
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
        virtualDoorDetectedMs = settings.optLong("virtual_door_detected_ms", 0),
        virtualDoorDropC = settings.optDouble("virtual_door_last_drop_c", 0.0),
        autoMaterialProfileEnabled = settings.optBoolean("auto_material_profile_enabled", true),
        mismatchWarningEnabled = settings.optBoolean("material_mismatch_warning_enabled", true),
        plaProtectionEnabled = settings.optBoolean("pla_protection_enabled", true),
        pauseHoldEnabled = settings.optBoolean("pause_hold_enabled", true),
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
        scheduledPreheatEnabled = settings.optBoolean("scheduled_preheat_enabled", false),
        localOnlyMode = settings.optBoolean("local_only_mode", true),
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
    ).withPreferences(settings)
}
