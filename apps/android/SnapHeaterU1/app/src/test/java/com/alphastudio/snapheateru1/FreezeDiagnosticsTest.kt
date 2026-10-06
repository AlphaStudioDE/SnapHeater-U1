package com.alphastudio.snapheateru1

import com.alphastudio.snapheateru1.data.freezeMask
import com.alphastudio.snapheateru1.data.freezeEventMask
import com.alphastudio.snapheateru1.data.freezeSensorLabel
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test

class FreezeDiagnosticsTest {
    @Test fun validMasksIdentifyExactlyTheReportedSensors() {
        for(mask in 1..3) assertEquals(mask,JSONObject().put("sensor_freeze_mask",mask).freezeMask())
        assertEquals(R.string.freeze_sensor_chamber,freezeSensorLabel(1))
        assertEquals(R.string.freeze_sensor_ptc,freezeSensorLabel(2))
        assertEquals(R.string.freeze_sensor_both,freezeSensorLabel(3))
    }
    @Test fun legacyAndInvalidMasksNeverGuessASensor() {
        assertEquals(0,JSONObject().freezeMask())
        for(value in listOf(0,4,-1,1.5,"1",JSONObject.NULL))
            assertEquals(0,JSONObject().put("sensor_freeze_mask",value).freezeMask())
        assertEquals(R.string.freeze_sensor_unknown,freezeSensorLabel(4))
    }
    @Test fun historyAndNotificationCodesPreserveChannelIdentity() {
        assertEquals(1,freezeEventMask("sensor_freeze_warning_chamber"))
        assertEquals(2,freezeEventMask("sensor_freeze_warning_ptc"))
        assertEquals(3,freezeEventMask("sensor_freeze_warning_both"))
        assertEquals(0,freezeEventMask("sensor_freeze_warning"))
        assertEquals(0,freezeEventMask("sensor_freeze_ended"))
    }
}
