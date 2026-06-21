package com.example.snapheater

import java.util.UUID

object SnapHeaterBleContract {
    // Verify displayed UUID order with nRF Connect after first firmware build.
    val SERVICE_UUID: UUID = UUID.fromString("01103155-4853-4291-7c4a-5ba100001011")
    val STATUS_UUID: UUID = UUID.fromString("01103155-4853-4291-7c4a-5ba101001011")
    val CONTROL_UUID: UUID = UUID.fromString("01103155-4853-4291-7c4a-5ba102001011")
    val DIAGNOSTICS_UUID: UUID = UUID.fromString("01103155-4853-4291-7c4a-5ba103001011")

    const val DEVICE_NAME = "SnapHeater U1"

    fun unlock(pin: String = "123456") = """{"unlock":"$pin"}"""
    fun off() = """{"work_on":false,"isrunning":false}"""
    fun auto(target: Int = 50, filterTemp: Int = 30, hotbedTemp: Int = 80) =
        """{"work_on":true,"work_mode":1,"set_temp":$target,"filtertemp":$filterTemp,"hotbedtemp":$hotbedTemp}"""
    fun manual(target: Int = 45) =
        """{"work_on":true,"work_mode":2,"set_temp":$target}"""
    fun preheat(target: Int = 60, holdMinutes: Int = 30) =
        """{"preheat_running":true,"preheat_target":$target,"preheat_hold_min":$holdMinutes}"""
    fun profile(name: String) = """{"profile":"$name"}"""
    fun profilePreheat(name: String) = """{"profile":"$name","preheat_running":true}"""
    fun setSessionLimit(minutes: Int) = """{"manual_session_max_min":$minutes}"""
    fun setFanPostrun(minutes: Int) = """{"fan_postrun_min":$minutes}"""
    // Android UI idea: when the Tempering checkbox is enabled, show a duration picker.
    // endTemp=0 means linear ramp to heater-off by the selected duration.
    fun setTempering(enabled: Boolean = true, durationMinutes: Int = 30, endTemp: Int = 0) =
        """{"tempering_enabled":$enabled,"tempering_duration_min":$durationMinutes,"tempering_end_temp":$endTemp}"""
    fun acknowledgeSessionTimeout() = """{"ack_session_timeout":true}"""
    fun disableTempering() = """{"tempering_enabled":false}"""
    fun acknowledgeTemperingComplete() = """{"ack_tempering_complete":true}"""
    fun cancelTempering() = """{"cancel_tempering":true}"""
    fun provisionWifi(ssid: String, password: String, moonrakerHost: String, moonrakerPort: Int = 7125) =
        """{"wifi_ssid":"$ssid","wifi_password":"$password","moonraker_host":"$moonrakerHost","moonraker_port":$moonrakerPort}"""
    fun stopPreheat() = """{"preheat_running":false}"""
    fun acknowledgePreheatComplete() = """{"ack_preheat_complete":true}"""
    fun dryingPla() = """{"filament_drying_mode":1,"isrunning":true}"""
    fun dryingPetg() = """{"filament_drying_mode":2,"isrunning":true}"""
    fun dryingAbs() = """{"filament_drying_mode":3,"isrunning":true}"""
    fun dryingCustom(temp: Int, hours: Int) =
        """{"filament_drying_mode":4,"custom_temp":$temp,"custom_timer":$hours,"isrunning":true}"""
    fun setAutoMaterialProfile(enabled: Boolean = true) =
        """{"auto_material_profile_enabled":$enabled}"""
    fun setAntiWarp(enabled: Boolean = true) =
        """{"anti_warp_enabled":$enabled}"""
    fun setLargePrintProtection(enabled: Boolean = true) =
        """{"large_print_protection_enabled":$enabled}"""
    fun setSafeOvernight(enabled: Boolean = true) =
        """{"safe_overnight_enabled":$enabled}"""
    fun setPauseHold(strategy: Int = 0, holdMin: Int = 60, lowerAfterMin: Int = 30, lowerByC: Int = 5, stopAfterMin: Int = 180) =
        """{"pause_hold_enabled":true,"pause_hold_strategy":$strategy,"pause_hold_min":$holdMin,"pause_lower_after_min":$lowerAfterMin,"pause_lower_by_c":$lowerByC,"pause_stop_after_min":$stopAfterMin}"""
    fun chamberDryOut(target: Int = 45, minutes: Int = 20) =
        """{"dryout_running":true,"dryout_target":$target,"dryout_duration_min":$minutes}"""
    fun stopDryOut() = """{"dryout_running":false}"""
    fun acknowledgeDryOutComplete() = """{"ack_dryout_complete":true}"""
    fun schedulePreheat(delayMinutes: Int, target: Int = 55, holdMinutes: Int = 20) =
        """{"scheduled_preheat_enabled":true,"scheduled_preheat_delay_min":$delayMinutes,"scheduled_preheat_target":$target,"scheduled_preheat_hold_min":$holdMinutes}"""
    fun cancelScheduledPreheat() = """{"scheduled_preheat_enabled":false}"""
    fun setFinishConditioning(mode: Int, keepWarmTemp: Int = 35, keepWarmMaxMin: Int = 60) =
        """{"finish_conditioning_mode":$mode,"keep_warm_temp":$keepWarmTemp,"keep_warm_max_min":$keepWarmMaxMin}"""
    fun heaterHealthTest(target: Int = 45, seconds: Int = 90) =
        """{"health_test_running":true,"health_test_target":$target,"health_test_duration_sec":$seconds}"""
    fun stopHeaterHealthTest() = """{"health_test_running":false}"""
    fun acknowledgeHealthTestComplete() = """{"ack_health_test_complete":true}"""

    // Software-only chamber/top-cover opening inference from sudden temperature drop.
    // action: 0=notify only, 1=stop post-print conditioning, 2=stop heater/conditioning.
    fun setVirtualDoorDetection(
        enabled: Boolean = true,
        windowSec: Int = 60,
        dropC: Int = 4,
        rateCPerMin: Int = 4,
        minBaseTemp: Int = 35,
        action: Int = 1
    ) = """{"virtual_door_detection_enabled":$enabled,"virtual_door_window_sec":$windowSec,"virtual_door_drop_c":$dropC,"virtual_door_rate_c_per_min":$rateCPerMin,"virtual_door_min_base_temp":$minBaseTemp,"virtual_door_action":$action}"""
    fun acknowledgeVirtualDoorOpen() = """{"ack_virtual_door_open":true}"""
    fun clearVirtualDoorOpen() = """{"clear_virtual_door_open":true}"""

}

    // v1.2 Material/Profile Mismatch Warning
    const val KEY_MATERIAL_MISMATCH_WARNING_ENABLED = "material_mismatch_warning_enabled"
    const val KEY_ACK_MATERIAL_MISMATCH = "ack_material_mismatch"
    const val KEY_CLEAR_MATERIAL_MISMATCH = "clear_material_mismatch"

    // Compact BLE status fields:
    // mm=true => Android should show mismatch notification.
    // mmu=user profile id, mmp=printer detected profile id, mmmsg=human-readable message.


// v1.3 extended intelligence keys
const val CMD_HEAT_SOAK_ENABLED = "heat_soak_enabled"
const val CMD_HEAT_SOAK_MIN = "heat_soak_min"
const val CMD_FILTER_LIFE_COUNTER_ENABLED = "filter_life_counter_enabled"
const val CMD_PLA_PROTECTION_ENABLED = "pla_protection_enabled"
const val CMD_DEMO_MODE_ENABLED = "demo_mode_enabled"
const val CMD_ACTIVE_RECIPE_SLOT = "active_recipe_slot"
const val CMD_ACTIVE_RECIPE_NAME = "active_recipe_name"
const val STATUS_WARMUP_ETA_SEC = "eta"
const val STATUS_HEAT_SOAK_PHASE = "soak"
const val STATUS_FILTER_LIFE_PERCENT = "flt"
const val STATUS_HEATER_WEAR_PERCENT = "wear"
const val STATUS_PRINT_RISK_SCORE = "risk"
const val STATUS_SAFETY_SCORE = "safe"


// v1.4 Productization & Safety keys
object SnapHeaterProductizationKeys {
    const val SYMBIONT_MODE_ENABLED = "symbiont_mode_enabled"
    const val SYMBIONT_VENTILATION_ALLOWED = "symbiont_ventilation_allowed"
    const val OUTPUT_SAFETY_LATCH_ENABLED = "output_safety_latch_enabled"
    const val ARM_OUTPUT_SAFETY_LATCH = "arm_output_safety_latch"
    const val FIRST_SETUP_STEP = "first_setup_step"
    const val INCIDENT_REPORT_PENDING = "incident_report_pending"
    const val TEMP_HISTORY_ENABLED = "temp_history_enabled"
    const val LOCAL_ONLY_MODE = "local_only_mode"
}
