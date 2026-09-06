package com.alphastudio.snapheateru1

import com.alphastudio.snapheateru1.ui.screens.previewTemperaturePoints
import org.junit.Assert.*
import org.junit.Test

class PreviewHistoryTest {
    @Test fun rangesAreBoundedContinuousAndSynthetic() {
        val end = 1_800_000_000_000L
        for (hours in listOf(2, 24)) {
            val points = previewTemperaturePoints(end, hours)
            assertEquals(end, points.last().time)
            assertEquals(hours * 3_600_000L, end - points.first().time)
            assertTrue(points.size in 721..2881)
            assertTrue(points.all { it.segment == "preview" && it.chamber!!.isFinite() && it.ptc!!.isFinite() })
            assertTrue(points.zipWithNext().all { (a, b) -> b.time - a.time in 1..30_000 && b.sequence == a.sequence + 1 })
            assertEquals(35.0, points.last().target!!, 0.001)
            assertEquals(points, previewTemperaturePoints(end, hours))
        }
    }
}
