package com.alphastudio.snapheateru1.ui.screens

import com.alphastudio.snapheateru1.data.TemperaturePoint
import kotlin.math.sin

/** Pure synthetic series. Never insert these points into the real history store. */
internal fun previewTemperaturePoints(end: Long, hours: Int): List<TemperaturePoint> {
    require(hours == 2 || hours == 24)
    val step = if (hours == 2) 10_000L else 30_000L
    val duration = hours * 3_600_000L
    return (0..(duration / step).toInt()).map { index ->
        val time = end - duration + index * step
        val elapsed = (time - (end - 7_200_000L)) / 60_000.0
        val minute = ((elapsed % 120) + 120) % 120
        // Include the endpoint of the last ramp rather than starting a new cycle.
        val m = if (time == end) 120.0 else minute
        val target = if (m < 90) 45.0 else 45.0 - (m - 90) / 3
        val chamber = when {
            m < 25 -> 24.0 + 21.0 * sin(m / 25 * Math.PI / 2)
            m in 75.0..77.0 -> 45.0
            m < 90 -> 45.0 + 0.35 * sin(m * 0.8)
            else -> target + 0.8 * sin((m - 90) / 30 * Math.PI)
        }
        val ptc = when {
            m < 10 -> 24.0 + m * 5
            m in 75.0..77.0 -> 70.0
            m < 90 -> 70.0 + 3 * sin(m * 0.5)
            else -> 70.0 - (m - 90) + 0.7 * sin(m)
        }
        TemperaturePoint(time, chamber, ptc, target, false, "preview", index.toLong())
    }
}
