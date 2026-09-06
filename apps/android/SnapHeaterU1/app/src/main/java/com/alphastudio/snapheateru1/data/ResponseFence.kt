package com.alphastudio.snapheateru1.data

/** UI-thread fence: an old response must not restore state after OFF or a new selection. */
class ResponseFence {
    private var generation = 0L
    fun invalidate(): Long = ++generation
    fun ticket(): Long = generation
    fun accepts(ticket: Long): Boolean = ticket == generation
}
