package com.alphastudio.snapheateru1.data

import org.json.JSONObject

fun printerConfiguration(host: String, port: Int, ssid: String, password: String): JSONObject {
    require(host.length in 1..63 && host.matches(Regex("[A-Za-z0-9.-]+")))
    require(port in 1..65535)
    require(ssid.toByteArray(Charsets.UTF_8).size <= 32)
    require(password.isEmpty() || password.toByteArray(Charsets.UTF_8).size in 8..63)
    return JSONObject().put("moonraker_host", host).put("moonraker_port", port)
        .put("takeover", true).apply {
            // An empty SSID preserves the current Wi-Fi network.
            if (ssid.isNotEmpty()) {
                put("wifi_ssid", ssid)
                put("wifi_password", password)
            }
        }
}
