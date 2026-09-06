package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.HeaterSnapshot
import org.json.JSONObject

fun HeaterSnapshot.withPreferences(prefs: JSONObject): HeaterSnapshot = copy(
    virtualDoorDetectionEnabled = prefs.optBoolean("virtual_door_detection_enabled", virtualDoorDetectionEnabled),
    heatSoakEnabled = prefs.optBoolean("heat_soak_enabled", heatSoakEnabled),
    autoMaterialProfileEnabled = prefs.optBoolean("auto_material_profile_enabled", autoMaterialProfileEnabled),
    mismatchWarningEnabled = prefs.optBoolean("material_mismatch_warning_enabled", mismatchWarningEnabled),
    plaProtectionEnabled = prefs.optBoolean("pla_protection_enabled", plaProtectionEnabled),
    antiWarpEnabled = prefs.optBoolean("anti_warp_enabled", antiWarpEnabled),
    largePrintProtectionEnabled = prefs.optBoolean("large_print_protection_enabled", largePrintProtectionEnabled),
    safeOvernightEnabled = prefs.optBoolean("safe_overnight_enabled", safeOvernightEnabled),
    pauseHoldEnabled = prefs.optBoolean("pause_hold_enabled", pauseHoldEnabled),
    smartResumeEnabled = prefs.optBoolean("smart_resume_enabled", smartResumeEnabled),
    startPrintWarningEnabled = prefs.optBoolean("start_print_warning_enabled", startPrintWarningEnabled),
    airflowDetectionEnabled = prefs.optBoolean("airflow_detection_enabled", airflowDetectionEnabled),
    tempHistoryEnabled = prefs.optBoolean("temp_history_enabled", tempHistoryEnabled),
    incidentReportEnabled = prefs.optBoolean("incident_report_enabled", incidentReportEnabled),
    localRecipesEnabled = prefs.optBoolean("local_recipes_enabled", localRecipesEnabled),
    localOnlyMode = prefs.optBoolean("local_only_mode", localOnlyMode),
    showcaseModeEnabled = prefs.optBoolean("contest_showcase_mode_enabled", showcaseModeEnabled),
    symbiontModeEnabled = prefs.optBoolean("symbiont_mode_enabled", symbiontModeEnabled),
    symbiontVentilationAllowed = prefs.optBoolean("symbiont_ventilation_allowed", symbiontVentilationAllowed),
    scheduledPreheatEnabled = prefs.optBoolean("scheduled_preheat_enabled", false),
    scheduledPreheatDelayMin = prefs.optInt("scheduled_preheat_delay_min", 30),
    scheduledPreheatTargetC = prefs.optInt("scheduled_preheat_target", 45),
    scheduledPreheatHoldMin = prefs.optInt("scheduled_preheat_hold_min", 15),
)
