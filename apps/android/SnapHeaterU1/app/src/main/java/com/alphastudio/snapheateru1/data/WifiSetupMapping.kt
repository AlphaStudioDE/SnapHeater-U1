package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.WifiSetupStatus
import com.alphastudio.snapheateru1.model.WifiNetwork
import org.json.JSONObject

fun JSONObject.wifiSetupStatus(): WifiSetupStatus {
    val wifi = optJSONObject("wifi") ?: return WifiSetupStatus()
    val aps = wifi.optJSONArray("networks")
    val networks = (0 until (aps?.length() ?: 0)).mapNotNull { index ->
        aps?.optJSONObject(index)?.let {
            WifiNetwork(it.optString("ssid"), it.optInt("rssi", -100), it.optBoolean("secure", true))
        }
    }.filter { it.ssid.isNotEmpty() }.distinctBy { it.ssid }
    return WifiSetupStatus(wifi.optBoolean("supported"), wifi.optBoolean("connected"),
        wifi.optBoolean("busy"), wifi.optString("phase", "idle"), wifi.optString("ssid"),
        wifi.optString("ip"), wifi.optLong("operation"), networks)
}
