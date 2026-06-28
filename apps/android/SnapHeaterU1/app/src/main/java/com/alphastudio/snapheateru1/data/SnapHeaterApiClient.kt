/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import java.net.HttpURLConnection
import java.net.ConnectException
import java.net.SocketTimeoutException
import java.net.UnknownHostException
import java.net.URL
import org.json.JSONObject
import org.json.JSONException

class SnapHeaterApiClient(baseUrl: String) {
    private val rootUrl = normalizeBaseUrl(baseUrl)

    fun health(): JSONObject = request("GET", "/api/health")

    fun status(): JSONObject = request("GET", "/api/status")

    fun postSettings(payload: JSONObject): JSONObject = request("POST", "/api/settings", payload)

    private fun request(method: String, path: String, body: JSONObject? = null): JSONObject {
        val connection = try {
            (URL("$rootUrl$path").openConnection() as HttpURLConnection).apply {
                requestMethod = method
                connectTimeout = 3000
                readTimeout = 5000
                setRequestProperty("Accept", "application/json")
                if (body != null) {
                    doOutput = true
                    setRequestProperty("Content-Type", "application/json")
                }
            }
        } catch (error: Exception) {
            throw SnapHeaterApiException("Invalid device address", error)
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
                throw SnapHeaterApiException("Firmware returned HTTP $code")
            }
            return if (text.isBlank()) JSONObject() else JSONObject(text)
        } catch (error: SocketTimeoutException) {
            throw SnapHeaterApiException("Timeout: device did not answer", error)
        } catch (error: UnknownHostException) {
            throw SnapHeaterApiException("Device address not found", error)
        } catch (error: ConnectException) {
            throw SnapHeaterApiException("Cannot connect to device", error)
        } catch (error: JSONException) {
            throw SnapHeaterApiException("Firmware returned invalid JSON", error)
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

class SnapHeaterApiException(message: String, cause: Throwable? = null) : Exception(message, cause)
