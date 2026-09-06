/*
 * SnapHeater U1 — Copyright (c) 2026 Damian Borkowski — SPDX-License-Identifier: MIT
 * Discovery order, target priorities and timeouts adapted from coroNET OS 2
 * src/printer/PrinterService.cpp (same copyright, MIT). No printer writes.
 */
package com.alphastudio.snapheateru1.data

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import org.json.JSONObject
import java.net.Inet4Address
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.HttpURLConnection
import java.net.URL
import java.io.ByteArrayOutputStream
import kotlin.coroutines.resume

data class DiscoveredPrinter(val host: String, val port: Int, val name: String, val controlData: Boolean)
enum class DiscoveryStage { Saved, Mdns, Quick, Full }

// Keep coroNET ordering; restrict fallback to the local /24 intersected with
// the real subnet. No sweeping a large routed network or mobile connection.
internal fun quickPrinterTargets(local: Int, gateway: Int?, saved: Int?): List<Int> {
    val targets = linkedSetOf<Int>()
    fun add(value: Int?) { if (value != null && value in 1..254) targets.add(value) }
    add(saved); add(gateway); add(local)
    for (distance in 1..12) { add(local - distance); add(local + distance) }
    for (host in listOf(2,3,4,5,6,8,10,11,12,15,20,25,30,40,50,60,75,80,100,120,150,200)) add(host)
    for (host in 2..60) { if (targets.size >= 60) break; add(host) }
    return targets.toList()
}

class PrinterDiscovery(context: Context) {
    private val connectivity = context.getSystemService(ConnectivityManager::class.java)
    private val nsd = context.getSystemService(NsdManager::class.java)

    suspend fun discover(savedHost: String, savedPort: Int, pandaIp: String,
        progress: suspend (DiscoveryStage, Int) -> Unit): List<DiscoveredPrinter> = withContext(Dispatchers.IO) {
        val network = connectivity.allNetworks.firstOrNull {
            connectivity.getNetworkCapabilities(it)?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
        } ?: error("Wi-Fi required")
        val properties = connectivity.getLinkProperties(network) ?: error("Wi-Fi unavailable")
        val link = properties.linkAddresses.firstOrNull { it.address is Inet4Address }
            ?: error("IPv4 Wi-Fi required")
        val local = link.address.address
        val deadline = System.nanoTime() + 65_000_000_000L
        fun allowed(address: InetAddress): Boolean {
            if (address !is Inet4Address || address.isLoopbackAddress || address.isMulticastAddress) return false
            val bytes = address.address
            return (0 until link.prefixLength).all { bit ->
                val mask = 1 shl (7 - bit % 8)
                (local[bit / 8].toInt() and mask) == (bytes[bit / 8].toInt() and mask)
            }
        }
        fun checkNetwork() {
            check(System.nanoTime() < deadline) { "Discovery timeout" }
            check(connectivity.getNetworkCapabilities(network)?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true)
        }
        check(pandaIp.matches(Regex("(\\d{1,3}\\.){3}\\d{1,3}")) &&
            allowed(InetAddress.getByName(pandaIp))) { "Phone and Panda must share the Wi-Fi subnet" }
        val found = linkedMapOf<String, DiscoveredPrinter>()
        suspend fun probe(host: String, port: Int, name: String) {
            currentCoroutineContext().ensureActive()
            checkNetwork()
            if (port !in 1..65535) return
            // Only IPv4 literals discovered on the Wi-Fi subnet reach HTTP.
            val address = runCatching { InetAddress.getByName(host) }.getOrNull() ?: return
            if (!allowed(address)) return
            val result = probeMoonraker(network, host, port, name) ?: return
            found["$host:$port"] = result
        }
        progress(DiscoveryStage.Saved, 2)
        // Avoid unbounded DNS resolution for a manually entered hostname.
        val saved = savedHost.takeIf { it.matches(Regex("(\\d{1,3}\\.){3}\\d{1,3}")) }
        if (saved != null) probe(saved, savedPort, "Moonraker")
        if (found.isNotEmpty()) return@withContext found.values.toList()

        progress(DiscoveryStage.Mdns, 6)
        for (type in listOf("_snapmaker._tcp.", "_moonraker._tcp.")) {
            for (service in discoverServices(type)) {
                currentCoroutineContext().ensureActive()
                checkNetwork()
                val resolved = resolve(service) ?: continue
                val txtIp = resolved.attributes["ip"]?.toString(Charsets.UTF_8)
                val host = txtIp?.takeIf { it.matches(Regex("(\\d{1,3}\\.){3}\\d{1,3}")) && it != "0.0.0.0" }
                    ?: resolved.host?.hostAddress ?: continue
                val device = resolved.attributes["device_name"]?.toString(Charsets.UTF_8)
                val machine = resolved.attributes["machine_type"]?.toString(Charsets.UTF_8)
                val name = listOfNotNull(device, machine).filter { it.isNotBlank() }.distinct().joinToString(" · ")
                    .ifBlank { resolved.serviceName ?: "Moonraker" }.take(100)
                probe(host, if (type.startsWith("_snapmaker")) 7125 else resolved.port, name)
            }
            if (found.isNotEmpty()) return@withContext found.values.toList()
        }
        val prefix = local.take(3).joinToString(".") { (it.toInt() and 255).toString() } + "."
        val gateway = properties.routes.firstOrNull { it.isDefaultRoute }?.gateway?.address
            ?.takeIf { it.size == 4 }?.last()?.toInt()?.and(255)
        val quick = quickPrinterTargets(local.last().toInt() and 255, gateway,
            saved?.takeIf { it.startsWith(prefix) }?.substringAfterLast('.')?.toIntOrNull())
        suspend fun scan(targets: List<Int>, stage: DiscoveryStage) {
            targets.forEachIndexed { index, host ->
                currentCoroutineContext().ensureActive()
                progress(stage, (index + 1) * 100 / targets.size)
                probe("$prefix$host", 7125, "Moonraker")
            }
        }
        scan(quick, DiscoveryStage.Quick)
        if (found.isEmpty()) scan((1..254).filterNot { it in quick }, DiscoveryStage.Full)
        found.values.toList()
    }

    private suspend fun discoverServices(type: String): List<NsdServiceInfo> {
        val services = Channel<NsdServiceInfo>(32)
        val listener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(t: String) {}
            override fun onDiscoveryStopped(t: String) {}
            override fun onStartDiscoveryFailed(t: String, code: Int) { services.close() }
            override fun onStopDiscoveryFailed(t: String, code: Int) {}
            override fun onServiceLost(info: NsdServiceInfo) {}
            override fun onServiceFound(info: NsdServiceInfo) { services.trySend(info) }
        }
        val found = linkedMapOf<String, NsdServiceInfo>()
        try {
            nsd.discoverServices(type, NsdManager.PROTOCOL_DNS_SD, listener)
            withTimeoutOrNull(1200) {
                for (service in services) {
                    found[service.serviceName] = service
                    if (found.size >= 16) break
                }
            }
        } finally {
            runCatching { nsd.stopServiceDiscovery(listener) }
            services.close()
        }
        return found.values.toList()
    }

    @Suppress("DEPRECATION")
    private suspend fun resolve(service: NsdServiceInfo): NsdServiceInfo? =
        withTimeoutOrNull(1800) {
            suspendCancellableCoroutine { continuation ->
                nsd.resolveService(service, object : NsdManager.ResolveListener {
                    override fun onResolveFailed(info: NsdServiceInfo, error: Int) {
                        if (continuation.isActive) continuation.resume(null)
                    }
                    override fun onServiceResolved(info: NsdServiceInfo) {
                        if (continuation.isActive) continuation.resume(info)
                    }
                })
            }
        }

    private suspend fun probeMoonraker(network: Network, host: String, port: Int, name: String): DiscoveredPrinter? {
        return try {
            network.socketFactory.createSocket().use { it.connect(InetSocketAddress(host, port), 140) }
            currentCoroutineContext().ensureActive()
            val info = getJson(network, host, port, "/printer/info")?.optJSONObject("result") ?: return null
            if (info.optString("state").isBlank()) return null
            currentCoroutineContext().ensureActive()
            val status = getJson(network, host, port,
                "/printer/objects/query?print_stats=state&heater_bed=temperature,target&webhooks=state")
                ?.optJSONObject("result")?.optJSONObject("status")
            val bed = status?.optJSONObject("heater_bed")
            val valid = status?.optJSONObject("print_stats")?.opt("state") is String &&
                status.optJSONObject("webhooks")?.optString("state") == "ready" &&
                bed?.opt("temperature") is Number && bed.optDouble("temperature").isFinite() &&
                bed.opt("target") is Number && bed.optDouble("target").isFinite()
            DiscoveredPrinter(host, port, name, valid)
        } catch (cancelled: CancellationException) {
            throw cancelled
        } catch (_: Exception) { null }
    }

    private fun getJson(network: Network, host: String, port: Int, path: String): JSONObject? {
        val connection = network.openConnection(URL("http://$host:$port$path")) as HttpURLConnection
        try {
            connection.connectTimeout = 900
            connection.readTimeout = 900
            connection.instanceFollowRedirects = false
            connection.requestMethod = "GET"
            if (connection.responseCode != 200) return null
            val deadline = System.nanoTime() + 1_800_000_000L
            val body = connection.inputStream.use { stream ->
                val output = ByteArrayOutputStream()
                val buffer = ByteArray(2048)
                while (output.size() <= 65536) {
                    check(System.nanoTime() < deadline)
                    val count = stream.read(buffer)
                    if (count < 0) break
                    output.write(buffer, 0, count)
                }
                output.toByteArray()
            }
            if (body.size > 65536) return null
            return JSONObject(body.toString(Charsets.UTF_8))
        } finally { connection.disconnect() }
    }
}
