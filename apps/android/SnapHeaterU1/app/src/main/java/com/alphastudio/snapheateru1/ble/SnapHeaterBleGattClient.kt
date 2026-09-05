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

class SnapHeaterBleException(message: String) : Exception(message)

class SnapHeaterBleGattClient(
    private val context: Context,
    private val address: String,
) {
    private val unlockPayload = """{"unlock":"123456"}"""

    suspend fun readStatus(): String = withConnectedSession { session ->
        delay(350L)
        session.readString(SnapHeaterBleContract.StatusCharacteristicUuid)
    }

    suspend fun writeControl(payload: String): String = withConnectedSession { session ->
        delay(350L)
        session.writeString(SnapHeaterBleContract.ControlCharacteristicUuid, unlockPayload)
        delay(150L)
        session.writeString(SnapHeaterBleContract.ControlCharacteristicUuid, payload)
        delay(150L)
        session.readString(SnapHeaterBleContract.StatusCharacteristicUuid)
    }

    @SuppressLint("MissingPermission")
    private suspend fun <T> withConnectedSession(block: suspend (GattSession) -> T): T {
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
    suspend fun writeString(uuid: UUID, payload: String): Unit = withTimeout(8000L) {
        suspendCancellableCoroutine { continuation ->
            val characteristic = service.getCharacteristic(uuid)
                ?: run {
                    continuation.resumeWithException(SnapHeaterBleException("BLE characteristic not found"))
                    return@suspendCancellableCoroutine
                }
            val bytes = payload.toByteArray(StandardCharsets.UTF_8)
            callback.pendingWrite = PendingWrite(uuid, continuation)
            characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            val started = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeCharacteristic(characteristic, bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothGatt.GATT_SUCCESS
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
        runCatching { gatt.disconnect() }
        runCatching { gatt.close() }
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
                val started = gatt.discoverServices()
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
