package com.alphastudio.snapheateru1

import com.alphastudio.snapheateru1.ble.StopPriorityGate
import com.alphastudio.snapheateru1.data.readBoundedResponse
import kotlinx.coroutines.*
import org.junit.Assert.*
import org.junit.Test
import java.io.StringReader

class StopPriorityTest {
    @Test fun stopCancelsActiveReadAndQueuedOn() = runBlocking {
        withTimeout(3000) {
            val gate=StopPriorityGate()
            val entered=CompletableDeferred<Unit>()
            var closed=false
            var onSent=false
            val read=launch {
                gate.run {
                    try { entered.complete(Unit); awaitCancellation() }
                    finally { closed=true }
                }
            }
            entered.await()
            val on=launch(start=CoroutineStart.UNDISPATCHED) { gate.run { onSent=true } }
            gate.run(stop=true) { assertTrue(closed); assertFalse(onSent) }
            read.join();on.join()
            assertTrue(read.isCancelled);assertTrue(on.isCancelled)
            gate.run { assertFalse(onSent) } // New explicit operation remains possible.
        }
    }

    @Test fun responseLimitIsEnforcedDuringRead() {
        assertEquals("abcd",readBoundedResponse(StringReader("abcd"),4))
        try {
            readBoundedResponse(StringReader("abcdef"),4)
            fail("Oversized response accepted")
        } catch (_: IllegalArgumentException) { }
    }
}
