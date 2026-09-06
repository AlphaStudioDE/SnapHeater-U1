package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import org.json.JSONObject
import kotlin.math.ceil

// Configuration writes deliberately omit work_on/work_mode and all start flags.
fun HeaterSnapshot.preferencesPayload(): JSONObject = JSONObject()
    .put("expected_revision", controlStateRevision)
    .put("set_temp", targetC.coerceIn(0, 55))
    .put("preheat_target", targetC.coerceIn(30, 55))
    .put("preheat_hold_min", preheatHeatSoakMin.coerceIn(1, 240))
    .put("virtual_door_detection_enabled", virtualDoorDetectionEnabled)
    .put("heat_soak_enabled", heatSoakEnabled)
    .put("auto_material_profile_enabled", autoMaterialProfileEnabled)
    .put("material_mismatch_warning_enabled", mismatchWarningEnabled)
    .put("pla_protection_enabled", plaProtectionEnabled)
    .put("pause_hold_enabled", pauseHoldEnabled)
    .put("start_print_warning_enabled", startPrintWarningEnabled)
    .put("airflow_detection_enabled", airflowDetectionEnabled)
    .put("temp_history_enabled", tempHistoryEnabled)
    .put("incident_report_enabled", incidentReportEnabled)
    .put("local_only_mode", localOnlyMode)
    .put("symbiont_mode_enabled", symbiontModeEnabled)
    .put("symbiont_ventilation_allowed", symbiontVentilationAllowed)
    .put("symbiont_safe_control_enabled", symbiontModeEnabled && symbiontVentilationAllowed)
    .put("symbiont_policy", if (symbiontModeEnabled && symbiontVentilationAllowed) 1 else 0)

fun HeaterSnapshot.jobPayload(): JSONObject {
    if (mode == AppMode.SafeStop) return JSONObject().put("safe_stop", true)
    val result = JSONObject().put("work_on", true).put("work_mode", mode.toFirmwareWorkMode())
        .put("expected_revision", controlStateRevision)
        .put("takeover", true).put("set_temp", targetC.coerceIn(0, 55))
        .put("tempering_duration_min", temperingDurationMin.coerceIn(1, 240))
        .withAutoFinishPolicy(mode)
    when (mode) {
        AppMode.Preheat -> result.put("preheat_running", true).put("preheat_target", targetC.coerceIn(30, 55))
            .put("preheat_hold_min", preheatHeatSoakMin.coerceIn(1, 240))
        AppMode.Drying -> result.put("isrunning", true).put("filament_drying_mode", 4)
            .put("custom_temp", targetC.coerceIn(40, 55))
            .put("custom_timer", ceil(dryingTimeMin.coerceIn(1, sessionLimitMin.coerceIn(1,720)) / 60.0).toInt())
            .put("drying_duration_min", dryingTimeMin.coerceIn(1, sessionLimitMin.coerceIn(1,720)))
        AppMode.Tempering -> result.put("tempering_start_now", true).put("tempering_enabled", true)
            .put("tempering_end_temp", 35)
        else -> Unit
    }
    return result
}

fun HeaterSnapshot.schedulePayload(): JSONObject = JSONObject()
    .put("expected_revision", controlStateRevision)
    .put("scheduled_preheat_enabled", scheduledPreheatEnabled)
    .put("scheduled_preheat_delay_min", scheduledPreheatDelayMin.coerceIn(1, 1440))
    .put("scheduled_preheat_target", scheduledPreheatTargetC.coerceIn(30, 55))
    .put("scheduled_preheat_hold_min", scheduledPreheatHoldMin.coerceIn(1, 240))
    .put("takeover", true)
