package com.alphastudio.snapheateru1

import com.alphastudio.snapheateru1.data.clearFaultAndVerify
import com.alphastudio.snapheateru1.model.*
import org.junit.Assert.*
import org.junit.Test

class FaultClearTest {
    private val locked=HeaterSnapshot(deviceId="AABBCCDD1234",faultClearSupported=true,
        faultLatched=true,mode=AppMode.SafeStop)

    @Test fun sendsOnceAndWaitsForFreshClearWithoutStartingHeat() {
        var reads=0;var writes=0
        val result=clearFaultAndVerify(
            {reads++;if(reads<3) locked else locked.copy(faultLatched=false)},
            {payload -> writes++;assertEquals(1,payload.length());
                assertTrue(payload.getBoolean("clear_heater_fault"));locked}, {})
        assertEquals(1,writes);assertFalse(result.faultLatched)
        assertEquals(AppMode.SafeStop,result.mode)
    }

    @Test fun unresolvedFaultDoesNotReportSuccessOrRetryClear() {
        var writes=0
        try {
            clearFaultAndVerify({locked},{writes++;locked},{})
            fail("Unconfirmed clear accepted")
        } catch (_: IllegalStateException) {assertEquals(1,writes)}
    }

    @Test fun inhibitNeverSendsClear() {
        try {
            clearFaultAndVerify({locked.copy(faultInhibited=true)},{fail("Must not send");locked},{})
            fail("Inhibit ignored")
        } catch (_: IllegalStateException) { }
    }

    @Test fun differentDeviceCannotConfirmClear() {
        try {
            clearFaultAndVerify({locked},{locked.copy(deviceId="OTHER",faultLatched=false)},{})
            fail("Wrong device accepted")
        } catch (_: IllegalStateException) { }
    }
}
