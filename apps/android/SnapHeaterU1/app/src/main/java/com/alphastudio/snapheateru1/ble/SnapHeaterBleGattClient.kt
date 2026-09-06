/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import java.nio.charset.StandardCharsets
import java.util.UUID
import kotlin.coroutines.Continuation
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlinx.coroutines.delay
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.currentCoroutineContext

class SnapHeaterBleException(message: String) : Exception(message)

class SnapHeaterBleGattClient(
    private val context: Context,
    private val address: String,
) {
    private val unlockPayload = """{"unlock":"123456"}"""
    companion object {
        private val deviceLocks=java.util.concurrent.ConcurrentHashMap<String,StopPriorityGate>()
        private val eventReads=java.util.concurrent.ConcurrentHashMap<String,kotlinx.coroutines.Job>()
    }
    suspend fun readEvents(): String = advisoryRead { session -> session.readString(SnapHeaterBleContract.EventsCharacteristicUuid) }
    suspend fun readHistory(after: Long): String = advisoryRead { session ->
        require(after in 0..4294967295L)
        val bytes=ByteArray(4) { index -> (after shr (8*index)).toByte() }
        session.writeBytes(SnapHeaterBleContract.HistoryCharacteristicUuid,bytes)
        session.readString(SnapHeaterBleContract.HistoryCharacteristicUuid)
    }
    private suspend fun advisoryRead(block: suspend (GattSession)->String): String = coroutineScope {
        val key=address.uppercase()
        val read=async { withConnectedSession(block=block) }
        eventReads.put(key,read)?.cancel()
        try { read.await() }
        catch(cancelled: kotlinx.coroutines.CancellationException) {
            currentCoroutineContext().ensureActive()
            throw SnapHeaterBleException("Event read yielded to device control")
        } finally { eventReads.remove(key,read) }
    }

    suspend fun readStatus(): String {
        eventReads[address.uppercase()]?.cancel()
        return withConnectedSession { session ->
        delay(350L)
        session.readString(SnapHeaterBleContract.StatusCharacteristicUuid)
        }
    }

    suspend fun writeControl(payload: String): String {
        eventReads[address.uppercase()]?.cancel()
        val json=org.json.JSONObject(payload)
        val stop=json.optBoolean("safe_stop") || json.optBoolean("emergency_stop") ||
            json.optBoolean("disarm_output_safety_latch") ||
            (json.has("work_on") && json.opt("work_on") == false)
        return withConnectedSession(stop) { session ->
        delay(350L)
        session.writeString(SnapHeaterBleContract.ControlCharacteristicUuid, unlockPayload)
        delay(150L)
        session.writeString(SnapHeaterBleContract.ControlCharacteristicUuid, payload)
        delay(150L)
        session.readString(SnapHeaterBleContract.StatusCharacteristicUuid)
        }
    }

    @SuppressLint("MissingPermission")
    private suspend fun <T> withConnectedSession(stop: Boolean = false, block: suspend (GattSession) -> T): T =
        deviceLocks.computeIfAbsent(address.uppercase()) { StopPriorityGate() }.run(stop) { connectedSession(block) }

    @SuppressLint("MissingPermission")
    private suspend fun <T> connectedSession(block: suspend (GattSession) -> T): T {
        val manager = context.getSystemService(BluetoothManager::class.java)
            ?: throw SnapHeaterBleException("Bluetooth manager is not available")
        val adapter = manager.adapter ?: throw SnapHeaterBleException("Bluetooth adapter is not available")
        val device = runCatching { adapter.getRemoteDevice(address) }
            .getOrElse { throw SnapHeaterBleException("Invalid BLE address") }

        removeSystemBondIfPresent(device)
        val session = withTimeout(12000L) { connect(device) }
        return try {
            block(session)
        } finally {
            session.close()
        }
    }

    @SuppressLint("MissingPermission")
    private suspend fun removeSystemBondIfPresent(device: BluetoothDevice) {
        if (device.bondState == BluetoothDevice.BOND_NONE) return
        runCatching {
            device.javaClass.getMethod("removeBond").invoke(device)
        }
        delay(900L)
    }

    @SuppressLint("MissingPermission")
    private suspend fun connect(device: BluetoothDevice): GattSession = suspendCancellableCoroutine { continuation ->
        val handler = Handler(Looper.getMainLooper())
        var completed = false
        var gattRef: BluetoothGatt? = null
        lateinit var callback: SessionCallback

        fun fail(message: String) {
            if (completed) return
            completed = true
            handler.removeCallbacksAndMessages(null)
            runCatching { gattRef?.close() }
            if (continuation.isActive) continuation.resumeWithException(SnapHeaterBleException(message))
        }

        callback = SessionCallback(
            onConnected = { gatt, service ->
                if (completed) return@SessionCallback
                completed = true
                handler.removeCallbacksAndMessages(null)
                if (continuation.isActive) continuation.resume(GattSession(gatt, service, callback))
            },
            onConnectFailed = { reason -> fail(reason) },
        )

        continuation.invokeOnCancellation {
            completed = true
            handler.removeCallbacksAndMessages(null)
            runCatching { gattRef?.close() }
        }

        gattRef = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
        } else {
            device.connectGatt(context, false, callback)
        }

        handler.postDelayed({ fail("BLE connection timed out") }, 12000L)
    }
}

class GattSession(
    private val gatt: BluetoothGatt,
    private val service: BluetoothGattService,
    private val callback: SessionCallback,
) {
    @SuppressLint("MissingPermission")
    suspend fun readString(uuid: UUID): String = withTimeout(8000L) {
        suspendCancellableCoroutine { continuation ->
            val characteristic = service.getCharacteristic(uuid)
                ?: run {
                    continuation.resumeWithException(SnapHeaterBleException("BLE characteristic not found"))
                    return@suspendCancellableCoroutine
                }
            callback.pendingRead = PendingRead(uuid, continuation)
            if (!gatt.readCharacteristic(characteristic)) {
                callback.pendingRead = null
                continuation.resumeWithException(SnapHeaterBleException("BLE read could not start"))
            }
        }
    }

    @SuppressLint("MissingPermission")
    suspend fun writeString(uuid: UUID, payload: String): Unit = writeBytes(uuid,payload.toByteArray(StandardCharsets.UTF_8))

    @SuppressLint("MissingPermission")
    suspend fun writeBytes(uuid: UUID, bytes: ByteArray): Unit = withTimeout(8000L) {
        suspendCancellableCoroutine { continuation ->
            val characteristic = service.getCharacteristic(uuid)
                ?: run {
                    continuation.resumeWithException(SnapHeaterBleException("BLE characteristic not found"))
                    return@suspendCancellableCoroutine
                }
            callback.pendingWrite = PendingWrite(uuid, continuation)
            characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            val started = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeCharacteristic(characteristic, bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == android.bluetooth.BluetoothStatusCodes.SUCCESS
            } else {
                characteristic.value = bytes
                gatt.writeCharacteristic(characteristic)
            }
            if (!started) {
                callback.pendingWrite = null
                continuation.resumeWithException(SnapHeaterBleException("BLE write could not start"))
            }
        }
    }

    fun close() {
        try { gatt.disconnect() } catch (_: SecurityException) { /* Permission revoked during session. */ }
        try { gatt.close() } catch (_: SecurityException) { /* Permission revoked during session. */ }
    }
}

data class PendingRead(
    val uuid: UUID,
    val continuation: Continuation<String>,
)

data class PendingWrite(
    val uuid: UUID,
    val continuation: Continuation<Unit>,
)

class SessionCallback(
    private val onConnected: (BluetoothGatt, BluetoothGattService) -> Unit,
    private val onConnectFailed: (String) -> Unit,
) : BluetoothGattCallback() {
    var pendingRead: PendingRead? = null
    var pendingWrite: PendingWrite? = null

    override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
        if (status != BluetoothGatt.GATT_SUCCESS) {
            onConnectFailed("BLE connection failed: $status")
            return
        }
        if (newState == BluetoothProfile.STATE_CONNECTED) {
            @SuppressLint("MissingPermission")
            val mtuStarted = gatt.requestMtu(247)
            if (!mtuStarted) {
                val started = try { gatt.discoverServices() } catch (_: SecurityException) { false }
                if (!started) onConnectFailed("BLE service discovery could not start")
            }
        } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
            onConnectFailed("BLE device disconnected")
        }
    }

    override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
        @SuppressLint("MissingPermission")
        val started = gatt.discoverServices()
        if (!started) onConnectFailed("BLE service discovery could not start after MTU")
    }

    override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
        if (status != BluetoothGatt.GATT_SUCCESS) {
            onConnectFailed("BLE service discovery failed: $status")
            return
        }
        val service = gatt.getService(SnapHeaterBleContract.ServiceUuid)
        if (service == null) {
            onConnectFailed("SnapHeater BLE service not found")
            return
        }
        onConnected(gatt, service)
    }

    @Deprecated("Used on Android 12 and below")
    override fun onCharacteristicRead(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
        handleRead(characteristic.uuid, characteristic.value, status)
    }

    override fun onCharacteristicRead(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
        value: ByteArray,
        status: Int,
    ) {
        handleRead(characteristic.uuid, value, status)
    }

    override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
        val pending = pendingWrite ?: return
        if (pending.uuid != characteristic.uuid) return
        pendingWrite = null
        if (status == BluetoothGatt.GATT_SUCCESS) {
            pending.continuation.resume(Unit)
        } else {
            pending.continuation.resumeWithException(SnapHeaterBleException("BLE write failed: $status"))
        }
    }

    private fun handleRead(uuid: UUID, value: ByteArray?, status: Int) {
        val pending = pendingRead ?: return
        if (pending.uuid != uuid) return
        pendingRead = null
        if (status == BluetoothGatt.GATT_SUCCESS && value != null) {
            pending.continuation.resume(String(value, StandardCharsets.UTF_8))
        } else {
            pending.continuation.resumeWithException(SnapHeaterBleException("BLE read failed: $status"))
        }
    }
}
