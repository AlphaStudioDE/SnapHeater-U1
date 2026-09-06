package com.alphastudio.snapheateru1
import com.alphastudio.snapheateru1.data.unreadEvents
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test

class DeviceEventsTest {
    private val page=JSONObject("""{"boot":"new","events":[{"seq":2,"level":"warn","code":"fault"},{"seq":3,"level":"info","code":"cycle_complete"}]}""")
    @Test fun receiptDeduplicatesSameBoot() {
        assertEquals(listOf(3L),unreadEvents(page,"new",2).map { it.sequence })
        assertTrue(unreadEvents(page,"new",3).isEmpty())
    }
    @Test fun rebootDoesNotSuppressNewEvents() {
        assertEquals(listOf(2L,3L),unreadEvents(page,"old",99).map { it.sequence })
    }
}
