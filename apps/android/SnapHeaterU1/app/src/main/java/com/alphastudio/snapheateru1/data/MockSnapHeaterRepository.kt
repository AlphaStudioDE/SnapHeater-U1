/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot

class MockSnapHeaterRepository : SnapHeaterRepository {
    private var state = HeaterSnapshot(ble = "Demo mode")

    override fun snapshot(): HeaterSnapshot = state

    override fun setMode(mode: AppMode): HeaterSnapshot {
        state = state.copy(mode = mode)
        return state
    }

    override fun setTarget(targetC: Int): HeaterSnapshot {
        state = state.copy(targetC = targetC)
        return state
    }

    override fun applySettings(snapshot: HeaterSnapshot): HeaterSnapshot {
        state = snapshot
        return state
    }

    override fun applySafety(snapshot: HeaterSnapshot, armLatch: Boolean, disarmLatch: Boolean): HeaterSnapshot {
        state = snapshot.copy(
            outputSafetyLatchArmed = when {
                armLatch -> true
                disarmLatch -> false
                else -> snapshot.outputSafetyLatchArmed
            },
            outputSafetyLatchReady = armLatch || (
                snapshot.outputSafetyLatchArmed &&
                    snapshot.heaterOutputVerified &&
                    snapshot.fanOutputVerified &&
                    snapshot.sensorsVerified
                ),
        )
        return state
    }
}
