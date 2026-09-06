package com.alphastudio.snapheateru1

import com.alphastudio.snapheateru1.data.*
import com.alphastudio.snapheateru1.model.*
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import java.io.File

class ControlPayloadTest {
    @Test fun exportRealAppCommandsAndCheckContracts() {
        val base = HeaterSnapshot(targetC = 50, dryingTimeMin = 90, preheatHeatSoakMin = 12,
            temperingDurationMin = 30, scheduledPreheatEnabled = true,
            scheduledPreheatDelayMin = 17, scheduledPreheatTargetC = 43, scheduledPreheatHoldMin = 11)
        val commands = linkedMapOf(
            "auto" to base.copy(mode = AppMode.AutoStandby).jobPayload(),
            "auto_temper" to base.copy(mode = AppMode.AutoStandbyTempering).jobPayload(),
            "preheat" to base.copy(mode = AppMode.Preheat).jobPayload(),
            "drying" to base.copy(mode = AppMode.Drying).jobPayload(),
            "tempering" to base.copy(mode = AppMode.Tempering).jobPayload(),
            "stop" to base.copy(mode = AppMode.SafeStop).jobPayload(),
            "preferences" to base.copy(mode = AppMode.SafeStop, heatSoakEnabled = false,
                plaProtectionEnabled = false,
                tempHistoryEnabled = false).preferencesPayload(),
            "schedule" to base.schedulePayload(),
            "cancel_schedule" to base.copy(scheduledPreheatEnabled = false).schedulePayload())
        assertTrue(commands.getValue("preheat").getBoolean("preheat_running"))
        assertEquals(90, commands.getValue("drying").getInt("drying_duration_min"))
        assertEquals(4, commands.getValue("drying").getInt("filament_drying_mode"))
        assertTrue(commands.getValue("tempering").getBoolean("tempering_start_now"))
        assertFalse(AppMode.Tempering.requiresPrinter())
        val prefs = commands.getValue("preferences")
        for (field in listOf("work_on", "work_mode", "preheat_running", "isrunning", "scheduled_preheat_enabled"))
            assertFalse(field, prefs.has(field))
        assertFalse(prefs.getBoolean("heat_soak_enabled"))
        assertEquals(1, commands.getValue("stop").length())
        val mapped = base.withPreferences(JSONObject().put("heat_soak_enabled", false).put("scheduled_preheat_delay_min", 17))
        assertFalse(mapped.heatSoakEnabled)
        assertEquals(17, mapped.scheduledPreheatDelayMin)
        val out = File("build/control-fixtures").apply { mkdirs() }
        commands.forEach { (name, json) -> File(out, "$name.json").writeText(json.toString()) }
    }
}
