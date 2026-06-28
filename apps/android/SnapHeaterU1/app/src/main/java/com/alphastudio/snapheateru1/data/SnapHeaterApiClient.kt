/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import java.net.HttpURLConnection
import java.net.URL
import org.json.JSONObject

class SnapHeaterApiClient(baseUrl: String) {
    private val rootUrl = normalizeBaseUrl(baseUrl)

    fun health(): JSONObject = request("GET", "/api/health")

    fun status(): JSONObject = request("GET", "/api/status")

    fun postSettings(payload: JSONObject): JSONObject = request("POST", "/api/settings", payload)

    private fun request(method: String, path: String, body: JSONObject? = null): JSONObject {
        val connection = (URL("$rootUrl$path").openConnection() as HttpURLConnection).apply {
            requestMethod = method
            connectTimeout = 3000
            readTimeout = 5000
            setRequestProperty("Accept", "application/json")
            if (body != null) {
                doOutput = true
                setRequestProperty("Content-Type", "application/json")
            }
        }

        try {
            if (body != null) {
                connection.outputStream.use { stream ->
                    stream.write(body.toString().toByteArray(Charsets.UTF_8))
                }
            }

            val code = connection.responseCode
            val stream = if (code in 200..299) connection.inputStream else connection.errorStream
            val text = stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }.orEmpty()
            if (code !in 200..299) {
                throw IllegalStateException("HTTP $code: ${text.ifBlank { "empty response" }}")
            }
            return if (text.isBlank()) JSONObject() else JSONObject(text)
        } finally {
            connection.disconnect()
        }
    }
}

fun normalizeBaseUrl(value: String): String {
    val trimmed = value.trim().trimEnd('/')
    val withScheme = if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) trimmed else "http://$trimmed"
    return withScheme.trimEnd('/')
}
