package com.alphastudio.snapheateru1.model

data class WifiNetwork(val ssid: String, val rssi: Int, val secure: Boolean)
data class WifiSetupStatus(
    val supported: Boolean = false,
    val connected: Boolean = false,
    val busy: Boolean = false,
    val phase: String = "idle",
    val ssid: String = "",
    val ip: String = "",
    val operation: Long = 0,
    val networks: List<WifiNetwork> = emptyList(),
)
