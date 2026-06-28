/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui

import androidx.annotation.StringRes
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.model.AppMode

@StringRes
fun AppMode.labelRes(): Int = when (this) {
    AppMode.AutoStandby -> R.string.mode_auto
    AppMode.ManualHold -> R.string.mode_manual
    AppMode.Preheat -> R.string.mode_preheat
    AppMode.Drying -> R.string.mode_drying
    AppMode.Tempering -> R.string.mode_tempering
    AppMode.SafeStop -> R.string.mode_safe_stop
}

@StringRes
fun AppMode.detailRes(): Int = when (this) {
    AppMode.AutoStandby -> R.string.mode_auto_detail
    AppMode.ManualHold -> R.string.mode_manual_detail
    AppMode.Preheat -> R.string.mode_preheat_detail
    AppMode.Drying -> R.string.mode_drying_detail
    AppMode.Tempering -> R.string.mode_tempering_detail
    AppMode.SafeStop -> R.string.mode_safe_stop_detail
}
