package com.example.snapheater

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import java.nio.charset.StandardCharsets

/**
 * Minimal Android BLE client skeleton for SnapHeater U1.
 * Add runtime permissions in the real app:
 * - Android 12+: BLUETOOTH_SCAN, BLUETOOTH_CONNECT
 * - older Android: ACCESS_FINE_LOCATION for BLE scanning
 */
@SuppressLint("MissingPermission")
class SnapHeaterBleClient(
    private val context: Context,
    private val onStatus: (String) -> Unit,
    private val onLog: (String) -> Unit = {}
) {
    private val adapter: BluetoothAdapter by lazy {
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    }
    private var gatt: BluetoothGatt? = null
    private var controlChar: BluetoothGattCharacteristic? = null
    private var statusChar: BluetoothGattCharacteristic? = null
    private var diagChar: BluetoothGattCharacteristic? = null

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val name = result.device.name ?: result.scanRecord?.deviceName
            if (name == SnapHeaterBleContract.DEVICE_NAME) {
                adapter.bluetoothLeScanner.stopScan(this)
                onLog("Found SnapHeater U1: ${result.device.address}")
                gatt = result.device.connectGatt(context, false, gattCallback)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            onLog("BLE scan failed: $errorCode")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                onLog("BLE connected")
                gatt.requestMtu(256)
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                onLog("BLE disconnected")
                controlChar = null
                statusChar = null
                diagChar = null
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            val service = gatt.getService(SnapHeaterBleContract.SERVICE_UUID)
            if (service == null) {
                onLog("SnapHeater service not found; verify UUID display order")
                return
            }
            statusChar = service.getCharacteristic(SnapHeaterBleContract.STATUS_UUID)
            controlChar = service.getCharacteristic(SnapHeaterBleContract.CONTROL_UUID)
            diagChar = service.getCharacteristic(SnapHeaterBleContract.DIAGNOSTICS_UUID)

            statusChar?.let { ch ->
                gatt.setCharacteristicNotification(ch, true)
                val cccd = ch.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                cccd?.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                cccd?.let { gatt.writeDescriptor(it) }
                gatt.readCharacteristic(ch)
            }
            diagChar?.let { gatt.readCharacteristic(it) }
        }

        @Deprecated("Deprecated in API 33 but kept for compatibility")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid == SnapHeaterBleContract.STATUS_UUID) {
                onStatus(characteristic.value.toString(StandardCharsets.UTF_8))
            }
        }

        @Deprecated("Deprecated in API 33 but kept for compatibility")
        override fun onCharacteristicRead(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val text = characteristic.value.toString(StandardCharsets.UTF_8)
                if (characteristic.uuid == SnapHeaterBleContract.STATUS_UUID) onStatus(text)
                else onLog("Read ${characteristic.uuid}: $text")
            }
        }
    }

    fun scanAndConnect() {
        onLog("Scanning for ${SnapHeaterBleContract.DEVICE_NAME}")
        adapter.bluetoothLeScanner.startScan(scanCallback)
    }

    fun disconnect() {
        adapter.bluetoothLeScanner.stopScan(scanCallback)
        gatt?.close()
        gatt = null
    }

    fun writeControl(json: String) {
        val ch = controlChar ?: run {
            onLog("Control characteristic not ready")
            return
        }
        ch.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        ch.value = json.toByteArray(StandardCharsets.UTF_8)
        val ok = gatt?.writeCharacteristic(ch) ?: false
        onLog("Write control queued=$ok payload=$json")
    }

    fun unlock(pin: String = "123456") = writeControl(SnapHeaterBleContract.unlock(pin))
    fun off() = writeControl(SnapHeaterBleContract.off())
    fun auto(target: Int = 50) = writeControl(SnapHeaterBleContract.auto(target))
    fun manual(target: Int = 45) = writeControl(SnapHeaterBleContract.manual(target))
    fun preheat(target: Int = 60, holdMinutes: Int = 30) = writeControl(SnapHeaterBleContract.preheat(target, holdMinutes))
    fun stopPreheat() = writeControl(SnapHeaterBleContract.stopPreheat())
    fun acknowledgePreheatComplete() = writeControl(SnapHeaterBleContract.acknowledgePreheatComplete())
    fun dryingPla() = writeControl(SnapHeaterBleContract.dryingPla())
    fun setTempering(enabled: Boolean = true, durationMinutes: Int = 30, endTemp: Int = 0) =
        writeControl(SnapHeaterBleContract.setTempering(enabled, durationMinutes, endTemp))
}

