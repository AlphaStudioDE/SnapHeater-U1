/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot

interface SnapHeaterRepository {
    fun snapshot(): HeaterSnapshot
    fun setMode(mode: AppMode): HeaterSnapshot
    fun setTarget(targetC: Int): HeaterSnapshot
    fun applySettings(snapshot: HeaterSnapshot): HeaterSnapshot
    fun applySafety(snapshot: HeaterSnapshot, armLatch: Boolean, disarmLatch: Boolean): HeaterSnapshot
}
