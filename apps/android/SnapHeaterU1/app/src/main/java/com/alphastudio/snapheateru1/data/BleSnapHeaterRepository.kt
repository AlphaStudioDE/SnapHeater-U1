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
            client.writeControl(JSONObject().put("heartbeat", leaseId).toString())
        } else {
            client.readStatus()
        }
        rememberControl(raw.toBleSnapshot())
    }

    private fun command(payload: JSONObject): HeaterSnapshot = runBlocking {
        if (leaseId.isNotBlank()) payload.put("lease_id", leaseId)
        if (revision >= 0) payload.put("expected_revision", revision)
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
        if (snapshot.mode == AppMode.SafeStop) {
            return command(safeStopPayload())
        }
        val payload = JSONObject()
            .put("work_mode", snapshot.mode.toFirmwareWorkMode())
            .put("takeover", true)
            .put("set_temp", snapshot.targetC)
            .put("work_on", snapshot.mode != AppMode.SafeStop)
            .put("preheat_hold_min", snapshot.preheatHeatSoakMin)
            .put("custom_timer", (snapshot.dryingTimeMin / 60).coerceAtLeast(1))
            .put("tempering_duration_min", snapshot.temperingDurationMin)
        return command(payload)
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
        workMode = optInt("m", 1),
        preheatRunning = optInt("ph", 0) > 0,
        dryingRunning = optBoolean("dry", false),
        temperingPhase = optInt("tmph", 0),
    )
    val material = optString("prof_name", optString("mat", "Custom")).ifBlank { "Custom" }
    val moonrakerConnected = optBoolean("mr", false)
    val progress = (optDouble("p", 0.0) * 100.0).toInt().coerceIn(0, 100)
    val safetyScore = optInt("safe", 0)

    return HeaterSnapshot(
        firmwareVersion = optString("v", "unknown"),
        chamberC = optDouble("tc", 0.0).toInt(),
        ptcC = optDouble("tp", 0.0).toInt(),
        targetC = optInt("set", 45),
        safetyScore = safetyScore,
        setupValidationPassed = optBoolean("setup_ok", false),
        outputSafetyLatchArmed = optBoolean("latch", false),
        outputSafetyLatchReady = optBoolean("latch_ready", false),
        heaterOutputVerified = optBoolean("hv", false),
        fanOutputVerified = optBoolean("fv", false),
        sensorsVerified = optBoolean("sv", false),
        moonrakerVerified = optBoolean("mv", false),
        heaterOutputBuildEnabled = true,
        fanOn = optBoolean("f", false),
        moonraker = if (moonrakerConnected) "Connected" else "Read-only / waiting",
        ble = "BLE connected",
        material = material,
        printerState = optString("ps", "standby").ifBlank { "standby" },
        printProgressPct = progress,
        activeTool = "tool${optInt("tool", 0)}",
        activeMaterial = optString("mat", material).ifBlank { material },
        materialAdvice = material,
        mode = mode,
        manualFanAssist = optBoolean("f", false),
        preheatHeatSoakMin = 15,
        dryingTimeMin = (optLong("rem", 0L) / 60L).toInt().coerceAtLeast(0),
        temperingDurationMin = 45,
        lastConfirmedSettings = "Synced over BLE",
        warmupEtaMin = (optInt("eta", 0) / 60).coerceAtLeast(0),
        heatSoakReady = optInt("soakr", 0) <= 0,
        stabilityScore = optInt("stab", 0),
        printRiskScore = optInt("risk", 0),
        printRiskMessage = if (optBoolean("riskp", false)) "Print risk warning pending" else "No active risk message",
        virtualDoorOpen = optBoolean("vdoor", false),
        virtualDoorPending = optBoolean("vdoor_pending", false),
        mismatchWarningEnabled = true,
        plaProtectionEnabled = !optBoolean("pla", false),
        antiWarpEnabled = optBoolean("aw", true),
        largePrintProtectionEnabled = optBoolean("lp", true),
        safeOvernightEnabled = optBoolean("night", false),
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
    )
}

private fun String.toBleSnapshot(): HeaterSnapshot {
    return JSONObject(this).toBleSnapshot()
}
