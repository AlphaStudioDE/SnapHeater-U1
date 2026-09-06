/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import android.content.Context
import com.alphastudio.snapheateru1.ble.SnapHeaterBleGattClient
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import kotlinx.coroutines.runBlocking
import org.json.JSONObject

class BleSnapHeaterRepository(
    context: Context,
    address: String,
) : SnapHeaterRepository {
    private val client = SnapHeaterBleGattClient(context.applicationContext, address)
    private var leaseId: String = ""
    private var revision: Long = -1

    override fun snapshot(): HeaterSnapshot = runBlocking {
        val raw = if (leaseId.isNotBlank()) {
            try {
                client.writeControl(JSONObject().put("heartbeat", leaseId).toString())
            } catch (cancelled: kotlinx.coroutines.CancellationException) {
                throw cancelled
            } catch (_: Exception) {
                leaseId = ""
                client.readStatus()
            }
        } else {
            client.readStatus()
        }
        rememberControl(raw.toBleSnapshot())
    }

    private fun command(payload: JSONObject): HeaterSnapshot = runBlocking {
        if (leaseId.isNotBlank()) payload.put("lease_id", leaseId)
        if (!payload.has("expected_revision") && revision >= 0) payload.put("expected_revision", revision)
        rememberControl(client.writeControl(payload.toString()).toBleSnapshot())
    }

    private fun rememberControl(value: HeaterSnapshot): HeaterSnapshot {
        revision = value.controlStateRevision
        if (value.controlOwner == "ble") {
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
    override fun clearFault(): HeaterSnapshot = clearFaultAndVerify({snapshot()},{command(it)})

    override fun acknowledgeVirtualDoor(detectedMs: Long): HeaterSnapshot = runBlocking {
        rememberControl(client.writeControl(JSONObject().put("virtual_door_ack", detectedMs).toString()).toBleSnapshot())
    }
    override fun setVirtualDoorDetection(enabled: Boolean): HeaterSnapshot =
        command(JSONObject().put("virtual_door_detection_enabled", enabled).put("takeover", true))

    override fun provisionRestToken(token: String) {
        require(token.length in 16..64)
        runBlocking { client.writeControl(JSONObject().put("rest_token", token).toString()) }
    }

    override fun configurePrinter(host: String, port: Int, apiKey: String): HeaterSnapshot {
        snapshot()
        val payload=printerConfiguration(host,port,apiKey)
        val id=payload.getJSONObject("printer_setup").getString("id")
        return awaitPrinterSetup(id,command(payload),{snapshot()})
    }

    override fun setupWifi(action: String, ssid: String, password: String): HeaterSnapshot {
        require(action == "scan" || action == "connect")
        val request = JSONObject().put("action", action)
        if (action == "connect") {
            require(ssid.toByteArray(Charsets.UTF_8).size in 1..32)
            require(password.isEmpty() || password.toByteArray(Charsets.UTF_8).size in 8..63)
            request.put("ssid", ssid).put("password", password)
        }
        return command(JSONObject().put("wifi_setup", request))
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

private fun JSONObject.toBleSnapshot(): HeaterSnapshot {
    val control = optJSONObject("control") ?: JSONObject()
    val mode = firmwareModeToAppMode(
        workOn = if (has("on")) optBoolean("on") else null,
        workMode = optInt("m", 1),
        preheatRunning = optInt("ph", 0) > 0,
        dryingRunning = optBoolean("dry", false),
        temperingPhase = optInt("tmph", 0),
        temperingEnabled = optBoolean("tmpen", false),
        finishConditioningMode = optInt("finish", 1),
    )
    val material = optString("prof_name", optString("mat", "Custom")).ifBlank { "Custom" }
    val moonrakerConnected = optBoolean("mr", false)
    val progress = (optDouble("p", 0.0) * 100.0).toInt().coerceIn(0, 100)
    val safetyScore = optInt("safe", 0)

    return HeaterSnapshot(
        settingsPersistenceKnown=has("settings_pending") && has("settings_persist_ok"),
        settingsPending=optBoolean("settings_pending",false),
        settingsPersistOk=optBoolean("settings_persist_ok",false),
        sessionLimitMin=optInt("session_limit_min",720).coerceIn(1,720),
        faultClearSupported=optBoolean("fault_clear_supported",false),
        faultLatched=optBoolean("fault_latched",false),
        faultInhibited=optBoolean("fault_inhibited",false),
        faultReason=optString("fault_reason",""),
        faultClearBlockReason=optString("fault_clear_block_reason",""),
        firmwareVersion = optString("v", "unknown"),
        chamberC = optDouble("tc", 0.0).toInt(),
        ptcC = optDouble("tp", 0.0).toInt(),
        historyReadingValid = optDouble("tc", Double.NaN).isFinite() && optDouble("tp", Double.NaN).isFinite() &&
            optString("chamber_sensor_status")=="ok" && optString("ptc_sensor_status")=="ok",
        effectiveTargetC = optInt("target_now", 0),
        targetC = optInt("set", 45),
        safetyScore = safetyScore,
        setupValidationPassed = optBoolean("setup_ok", false),
        outputSafetyLatchArmed = optBoolean("latch", false),
        outputSafetyLatchReady = optBoolean("latch_ready", false),
        heaterOutputVerified = optBoolean("hv", false),
        fanOutputVerified = optBoolean("fv", false),
        sensorsVerified = optBoolean("sv", false),
        moonrakerVerified = optBoolean("mv", false),
        heaterOutputBuildEnabled = optBoolean("heater_build", false),
        fanOn = optBoolean("f", false),
        moonraker = if (moonrakerConnected) "Connected" else "Read-only / waiting",
        printerDataReady = optBoolean("pr_ready", false),
        deviceId = optString("device_id", ""),
        wifi = wifiSetupStatus(),
        ble = "BLE connected",
        material = material,
        printerState = optString("ps", "standby").ifBlank { "standby" },
        printProgressPct = progress,
        activeTool = "tool${optInt("tool", 0)}",
        activeMaterial = optString("mat", material).ifBlank { material },
        materialAdvice = material,
        mode = mode,
        paused = optBoolean("paused", false),
        printerSetupId = optJSONObject("printer_setup")?.optString("id").orEmpty(),
        printerSetupPhase = optJSONObject("printer_setup")?.optString("phase").orEmpty(),
        ventilationStatus = optString("ventilation_status", ""),
        printerApiKeySet = optJSONObject("printer_setup")?.optBoolean("key_set",false) ?: false,
        sensorFreezeWarningMs = optLong("sensor_freeze_warning_ms", 0),
        sensorFreezeRemainingS = optInt("sensor_freeze_remaining_s", 0),
        usageAvailable = optBoolean("usage_available", false),
        heaterUsageMs = optLong("usage_heater_ms", 0),
        filterUsageMs = optLong("usage_filter_ms", 0),
        manualFanAssist = optBoolean("f", false),
        preheatHeatSoakMin = optInt("phhold", 15),
        dryingTimeMin = (optLong("rem", 0L) / 60L).toInt().coerceAtLeast(0),
        temperingDurationMin = optInt("tmpmin", 45),
        lastConfirmedSettings = "Synced over BLE",
        warmupEtaMin = (optInt("eta", 0) / 60).coerceAtLeast(0),
        heatSoakReady = optInt("soakr", 0) <= 0,
        stabilityScore = optInt("stab", 0),
        printRiskScore = optInt("risk", 0),
        printRiskMessage = if (optBoolean("riskp", false)) "Print risk warning pending" else "No active risk message",
        virtualDoorOpen = optBoolean("vdoor", false),
        virtualDoorDetectionEnabled = optBoolean("vdoor_enabled", true),
        virtualDoorPending = optBoolean("vdoor_pending", false),
        virtualDoorDetectedMs = optLong("vdoor_ms", 0),
        virtualDoorDropC = optDouble("vdoor_drop", 0.0),
        mismatchWarningEnabled = true,
        plaProtectionEnabled = !optBoolean("pla", false),
        airflowWarningPending = optBoolean("air", false),
        filterLifePct = optInt("flt", 0),
        heaterWearPct = optInt("wear", 0),
        estimatedEnergyWh = optDouble("wh", 0.0).toInt(),
        hardwareMapName = "panda_breath_accepted",
        zeroCrossSignalPresent = optBoolean("zc", false),
        zeroCrossEdgesPerSec = optInt("zcr", 0),
        zeroCrossLastPeriodUs = optInt("zcp", 0),
        zeroCrossEdges = optLong("zce", 0L),
        controlOwner = control.optString("owner", "none"),
        controlStateRevision = control.optLong("state_revision", 0L),
        controlLeaseId = control.optString("lease_id", ""),
        controlLeaseRemainingMs = control.optLong("lease_remaining_ms", 0L),
    ).withPreferences(optJSONObject("prefs") ?: JSONObject())
}

private fun String.toBleSnapshot(): HeaterSnapshot {
    return JSONObject(this).toBleSnapshot()
}
