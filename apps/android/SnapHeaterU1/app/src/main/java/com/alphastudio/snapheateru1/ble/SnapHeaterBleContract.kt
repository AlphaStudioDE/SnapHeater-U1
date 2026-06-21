/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ble

import java.util.UUID

object SnapHeaterBleContract {
    val ServiceUuid: UUID = UUID.fromString("7b2f1000-4a6f-4c2d-9a1e-2f4f53485531")
    val StatusCharacteristicUuid: UUID = UUID.fromString("7b2f1001-4a6f-4c2d-9a1e-2f4f53485531")
    val ControlCharacteristicUuid: UUID = UUID.fromString("7b2f1002-4a6f-4c2d-9a1e-2f4f53485531")
    val DiagnosticsCharacteristicUuid: UUID = UUID.fromString("7b2f1003-4a6f-4c2d-9a1e-2f4f53485531")

    const val DeviceName = "SnapHeater U1"
}
