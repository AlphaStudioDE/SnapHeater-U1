/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot

class MockSnapHeaterRepository : SnapHeaterRepository {
    private var state = HeaterSnapshot()

    override fun snapshot(): HeaterSnapshot = state

    override fun setMode(mode: AppMode): HeaterSnapshot {
        state = state.copy(mode = mode)
        return state
    }

    override fun setTarget(targetC: Int): HeaterSnapshot {
        state = state.copy(targetC = targetC)
        return state
    }
}
