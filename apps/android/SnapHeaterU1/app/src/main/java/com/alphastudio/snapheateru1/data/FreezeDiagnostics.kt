package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.R
import org.json.JSONObject

/** Missing/unknown data from older firmware must never name a sensor by guess. */
fun JSONObject.freezeMask(): Int = when (val value = opt("sensor_freeze_mask")) {
    is Number -> if (value.toDouble() in listOf(1.0, 2.0, 3.0)) value.toInt() else 0
    else -> 0
}

fun freezeSensorLabel(mask: Int): Int = when (mask) {
    1 -> R.string.freeze_sensor_chamber
    2 -> R.string.freeze_sensor_ptc
    3 -> R.string.freeze_sensor_both
    else -> R.string.freeze_sensor_unknown
}

fun freezeEventMask(code: String): Int = when (code) {
    "sensor_freeze_warning_chamber" -> 1
    "sensor_freeze_warning_ptc" -> 2
    "sensor_freeze_warning_both" -> 3
    else -> 0
}
