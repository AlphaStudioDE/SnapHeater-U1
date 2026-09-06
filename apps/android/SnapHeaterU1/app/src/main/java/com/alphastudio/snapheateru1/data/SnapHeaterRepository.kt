/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot

interface SnapHeaterRepository {
    fun clearFault(): HeaterSnapshot = throw UnsupportedOperationException("Fault clear unavailable")
    fun pauseJob(paused: Boolean, expectedRevision: Long): HeaterSnapshot
    fun savePreferences(snapshot: HeaterSnapshot): HeaterSnapshot
    fun schedulePreheat(snapshot: HeaterSnapshot): HeaterSnapshot
    fun provisionRestToken(token: String): Unit = throw UnsupportedOperationException("Use BLE to provision a REST token")
    fun acknowledgeVirtualDoor(detectedMs: Long): HeaterSnapshot
    fun setVirtualDoorDetection(enabled: Boolean): HeaterSnapshot
    fun setupWifi(action: String, ssid: String = "", password: String = ""): HeaterSnapshot =
        throw UnsupportedOperationException("Wi-Fi setup requires BLE")
    fun configurePrinter(host: String, port: Int, apiKey: String): HeaterSnapshot
    fun snapshot(): HeaterSnapshot
    fun setMode(mode: AppMode): HeaterSnapshot
    fun setTarget(targetC: Int): HeaterSnapshot
    fun applySettings(snapshot: HeaterSnapshot): HeaterSnapshot
    fun applySafety(snapshot: HeaterSnapshot, armLatch: Boolean, disarmLatch: Boolean): HeaterSnapshot
}
