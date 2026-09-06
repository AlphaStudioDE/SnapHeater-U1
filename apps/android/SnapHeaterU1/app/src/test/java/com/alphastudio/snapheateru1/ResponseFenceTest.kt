package com.alphastudio.snapheateru1

import com.alphastudio.snapheateru1.data.*
import com.alphastudio.snapheateru1.model.*
import org.junit.Assert.*
import org.junit.Test

class ResponseFenceTest {
    @Test fun stopAndDeviceChangeRejectEarlierResponsesEvenAfterStopCompletes() {
        val fence=ResponseFence()
        val poll=fence.ticket()
        val start=fence.invalidate()
        assertFalse(fence.accepts(poll))
        val stop=fence.invalidate()
        assertFalse(fence.accepts(start))
        assertTrue(fence.accepts(stop))
        // Completion does not roll the epoch back to a pre-OFF command.
        assertFalse(fence.accepts(start))
        fence.invalidate() // Select another heater.
        assertFalse(fence.accepts(stop))
    }

    @Test fun userIntentKeepsItsObservedRevisionAndStopNeedsNone() {
        val intent=HeaterSnapshot(mode=AppMode.ManualHold,controlStateRevision=42)
        assertEquals(42L,intent.jobPayload().getLong("expected_revision"))
        assertEquals(42L,intent.preferencesPayload().getLong("expected_revision"))
        assertEquals(42L,intent.schedulePayload().getLong("expected_revision"))
        assertFalse(intent.copy(mode=AppMode.SafeStop).jobPayload().has("expected_revision"))
    }

    @Test fun dryingDurationNeverExceedsDeviceLimit() {
        for(limit in 1..720) {
            val p=HeaterSnapshot(mode=AppMode.Drying,dryingTimeMin=1440,sessionLimitMin=limit).jobPayload()
            assertEquals(limit,p.getInt("drying_duration_min"))
        }
    }
}
