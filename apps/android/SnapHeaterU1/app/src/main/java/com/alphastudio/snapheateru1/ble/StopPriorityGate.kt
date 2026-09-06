package com.alphastudio.snapheateru1.ble

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

/** Per-device admission. STOP cancels active/pending sessions, never retries ON. */
class StopPriorityGate {
    private val monitor = Any()
    private val transport = Mutex()
    private val admitted = mutableSetOf<Job>()
    private var generation = 0L
    private var stops = 0

    suspend fun <T> run(stop: Boolean = false, operation: suspend () -> T): T = coroutineScope {
        val job = currentCoroutineContext()[Job]!!
        val epoch: Long
        val cancelled: List<Job>
        synchronized(monitor) {
            if (!stop && stops > 0) throw CancellationException("STOP has priority")
            cancelled = if (stop) admitted.toList() else emptyList()
            if (stop) { generation++; stops++ }
            epoch = generation
            admitted.add(job)
        }
        try {
            cancelled.forEach { it.cancel(CancellationException("Superseded by STOP")) }
            transport.withLock {
                currentCoroutineContext().ensureActive()
                synchronized(monitor) {
                    if (epoch != generation) throw CancellationException("Stale device operation")
                }
                operation()
            }
        } finally {
            synchronized(monitor) {
                admitted.remove(job)
                if (stop) stops--
            }
        }
    }
}
