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

class SnapHeaterApiClient(baseUrl: String, private val controlToken: String = "") {
    private val rootUrl = normalizeBaseUrl(baseUrl)

    fun health(): JSONObject = request("GET", "/api/health")

    fun status(): JSONObject = request("GET", "/api/status")
    fun verifyAccess(): JSONObject = request("GET", "/api/v2/auth")
    fun otaInfo(): JSONObject = request("GET", "/api/v2/ota")
    fun history(after: Long): JSONObject {
        require(after in 0..4294967295L)
        return request("GET","/api/history?after=$after",responseLimit=8192)
    }
    fun notifications(): JSONObject = request("GET", "/api/events").getJSONObject("notifications")

    /** Never retry an upload automatically: a lost reply may mean the device already rebooted. */
    fun uploadFirmware(image: ByteArray, expectedDevice: String, progress: (Float) -> Unit): JSONObject {
        require(controlToken.length in 16..64) { "REST authentication required" }
        verifyAccess()
        val state = status()
        require(state.optString("device_id").equals(expectedDevice, ignoreCase=true)) { "Device identity changed" }
        val info=otaInfo().getJSONObject("ota")
        require(info.optBoolean("enabled")) { "OTA disabled by firmware" }
        require(image.size>32 && image.size<=info.optInt("inactive_capacity") && image[0].toInt() and 255 == 0xE9) { "Invalid image size or header" }
        val hash=java.security.MessageDigest.getInstance("SHA-256").digest(image).joinToString("") { "%02x".format(it) }
        val connection=(URL("$rootUrl/api/v2/update").openConnection() as HttpURLConnection).apply {
            requestMethod="POST"; connectTimeout=5000; readTimeout=70000
            instanceFollowRedirects=false
            doOutput=true
            setFixedLengthStreamingMode(image.size)
            setRequestProperty("Content-Type","application/octet-stream")
            setRequestProperty("X-DragonBreath-Auth",controlToken)
            setRequestProperty("X-SnapHeater-SHA256",hash)
        }
        try {
            connection.outputStream.use { output ->
                var offset=0
                while(offset<image.size) {
                    val count=minOf(4096,image.size-offset)
                    output.write(image,offset,count); offset+=count
                    progress(offset.toFloat()/image.size)
                }
            }
            val code=connection.responseCode
            val stream=if(code in 200..299) connection.inputStream else connection.errorStream
            val body=stream?.bufferedReader()?.use { readBoundedResponse(it,8192) }.orEmpty()
            if(code !in 200..299) throw SnapHeaterApiException("OTA HTTP $code: "+runCatching { JSONObject(body).optString("error") }.getOrDefault(""))
            val reply=JSONObject(body)
            check(reply.optBoolean("ok") && reply.optString("sha256").equals(hash,true)) { "Upload confirmation missing or hash mismatch; check device before retry" }
            return reply
        } finally { connection.disconnect() }
    }

    fun postSettings(payload: JSONObject): JSONObject = request("POST", "/api/settings", payload)

    fun heartbeat(leaseId: String): JSONObject =
        request("POST", "/api/v2/heartbeat", JSONObject().put("lease_id", leaseId))

    private fun request(method: String, path: String, body: JSONObject? = null, responseLimit: Int=131072): JSONObject {
        val connection = try {
            (URL("$rootUrl$path").openConnection() as HttpURLConnection).apply {
                requestMethod = method
                instanceFollowRedirects = false
                connectTimeout = 3000
                readTimeout = 5000
                setRequestProperty("Accept", "application/json")
                if (controlToken.isNotBlank()) setRequestProperty("X-DragonBreath-Auth", controlToken)
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
            val text = stream?.bufferedReader(Charsets.UTF_8)?.use { reader ->
                val result=StringBuilder()
                val buffer=CharArray(1024)
                while(true) {
                    val count=reader.read(buffer)
                    if(count<0) break
                    if(result.length+count>responseLimit) throw SnapHeaterApiException("Response exceeds limit")
                    result.append(buffer,0,count)
                }
                result.toString()
            }.orEmpty()
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
