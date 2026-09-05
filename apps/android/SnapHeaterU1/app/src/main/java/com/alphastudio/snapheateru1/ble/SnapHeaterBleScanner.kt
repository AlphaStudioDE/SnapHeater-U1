/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.location.LocationManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import androidx.core.content.ContextCompat
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlinx.coroutines.suspendCancellableCoroutine

data class SnapHeaterBleDevice(
    val name: String,
    val address: String,
    val rssi: Int,
)

class SnapHeaterBleScanException(
    message: String,
    val seenDevices: List<SnapHeaterBleDevice> = emptyList(),
) : Exception(message)

class SnapHeaterBleScanner(
    private val context: Context,
) {
    @SuppressLint("MissingPermission")
    suspend fun findFirst(timeoutMs: Long = 15000L): SnapHeaterBleDevice = suspendCancellableCoroutine { continuation ->
        if (!hasRequiredPermissions(context)) {
            continuation.resumeWithException(SnapHeaterBleScanException("Bluetooth/location permissions are missing"))
            return@suspendCancellableCoroutine
        }
        if (!isLocationEnabled(context)) {
            continuation.resumeWithException(SnapHeaterBleScanException("Location is disabled; enable Location for BLE scanning on this phone"))
            return@suspendCancellableCoroutine
        }

        val manager = context.getSystemService(BluetoothManager::class.java)
        val adapter = manager?.adapter
        if (adapter == null) {
            continuation.resumeWithException(SnapHeaterBleScanException("Bluetooth is not available"))
            return@suspendCancellableCoroutine
        }
        if (!adapter.isEnabled) {
            continuation.resumeWithException(SnapHeaterBleScanException("Bluetooth is disabled"))
            return@suspendCancellableCoroutine
        }

        val scanner = adapter.bluetoothLeScanner
        if (scanner == null) {
            continuation.resumeWithException(SnapHeaterBleScanException("Bluetooth LE scanner is not available"))
            return@suspendCancellableCoroutine
        }

        val handler = Handler(Looper.getMainLooper())
        val seenDevices = linkedMapOf<String, SnapHeaterBleDevice>()
        var completed = false

        lateinit var callback: ScanCallback

        fun stop() {
            runCatching { scanner.stopScan(callback) }
        }

        fun complete(result: Result<SnapHeaterBleDevice>) {
            if (completed) return
            completed = true
            handler.removeCallbacksAndMessages(null)
            stop()
            if (!continuation.isActive) return
            result
                .onSuccess { continuation.resume(it) }
                .onFailure { continuation.resumeWithException(it) }
        }

        callback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val advertisedName = result.scanRecord?.deviceName ?: runCatching { result.device.name }.getOrNull()
                val displayName = advertisedName?.ifBlank { null } ?: "(no name)"
                seenDevices[result.device.address] = SnapHeaterBleDevice(displayName, result.device.address, result.rssi)
                val serviceMatches = result.scanRecord
                    ?.serviceUuids
                    ?.any { it.uuid == SnapHeaterBleContract.ServiceUuid } == true
                val nameMatches = advertisedName
                    ?.trim()
                    ?.contains("SnapHeater", ignoreCase = true) == true ||
                    advertisedName?.trim()?.equals(SnapHeaterBleContract.ShortDeviceName, ignoreCase = true) == true
                if (
                    nameMatches ||
                    serviceMatches
                ) {
                    complete(Result.success(SnapHeaterBleDevice(advertisedName ?: SnapHeaterBleContract.DeviceName, result.device.address, result.rssi)))
                }
            }

            override fun onBatchScanResults(results: MutableList<ScanResult>) {
                results.forEach { onScanResult(0, it) }
            }

            override fun onScanFailed(errorCode: Int) {
                complete(Result.failure(SnapHeaterBleScanException("Bluetooth scan failed: $errorCode")))
            }
        }

        continuation.invokeOnCancellation {
            completed = true
            handler.removeCallbacksAndMessages(null)
            stop()
        }

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
            .setMatchMode(ScanSettings.MATCH_MODE_AGGRESSIVE)
            .setReportDelay(0L)
            .build()
        scanner.startScan(emptyList<ScanFilter>(), settings, callback)
        handler.postDelayed({
            val seen = seenDevices.values
                .sortedByDescending { it.rssi }
                .take(8)
            val details = if (seen.isEmpty()) {
                "No BLE advertisements were visible to the app"
            } else {
                seen.joinToString(separator = "\n") { "${it.name} ${it.address} ${it.rssi} dBm" }
            }
            complete(Result.failure(SnapHeaterBleScanException("No validated device found\n$details", seen)))
        }, timeoutMs)
    }

    companion object {
        fun requiredPermissions(): Array<String> {
            return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                arrayOf(
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.ACCESS_FINE_LOCATION,
                )
            } else {
                arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
            }
        }

        fun hasRequiredPermissions(context: Context): Boolean {
            return requiredPermissions().all { permission ->
                ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
            }
        }

        fun isLocationEnabled(context: Context): Boolean {
            val manager = context.getSystemService(LocationManager::class.java) ?: return true
            return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                manager.isLocationEnabled
            } else {
                runCatching { manager.isProviderEnabled(LocationManager.GPS_PROVIDER) }.getOrDefault(false) ||
                    runCatching { manager.isProviderEnabled(LocationManager.NETWORK_PROVIDER) }.getOrDefault(false)
            }
        }
    }
}
