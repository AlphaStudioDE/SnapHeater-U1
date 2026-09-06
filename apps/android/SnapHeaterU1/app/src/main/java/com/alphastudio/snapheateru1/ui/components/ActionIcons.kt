/*
 * SnapHeater U1 — Copyright (c) 2026 Damian Borkowski — SPDX-License-Identifier: MIT
 */
package com.alphastudio.snapheateru1.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.PowerSettingsNew
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.addPathNodes
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.Dp
import androidx.compose.material3.LocalContentColor
import com.alphastudio.snapheateru1.model.AppMode

// Original 24-unit line icons; no external raster assets or runtime image loading.
object ModeIcons {
    val Preheat = lineIcon("Preheat", "M7 14V5a3 3 0 0 1 6 0v9a5 5 0 1 1-6 0 M10 7v10 M17 8l3-3 3 3 M20 5v10")
    val Heat = lineIcon("Heat", "M9 14V5a3 3 0 0 1 6 0v9a5 5 0 1 1-6 0 M12 7v10 M11 18h2")
    val Drying = lineIcon("Drying", "M15 12a7 7 0 1 1-14 0 7 7 0 0 1 14 0 M10 12a2 2 0 1 1-4 0 2 2 0 0 1 4 0 M8 5v3 M8 16v3 M1 12h3 M12 12h3 M18 19c-3-3 3-5 0-8s3-5 0-8 M22 19c-3-3 3-5 0-8s3-5 0-8")
    val Printer = lineIcon("Printer", "M3 21V3h18v18Z M3 7h18 M10 7v4h4V7 M11 11l1 2 1-2 M7 17h10v4 M5 17h14")
    val Cooldown = lineIcon("Cooldown", "M7 14V5a3 3 0 0 1 6 0v9a5 5 0 1 1-6 0 M10 7v10 M17 12l3 3 3-3 M20 5v10")
}

private fun lineIcon(name: String, path: String) =
    ImageVector.Builder(name, 24.dp, 24.dp, 24f, 24f)
        .addPath(pathData = addPathNodes(path), fill = null,
            stroke = SolidColor(Color.Black), strokeLineWidth = 1.8f,
            strokeLineCap = StrokeCap.Round, strokeLineJoin = StrokeJoin.Round)
        .build()

fun AppMode.actionIcon(): ImageVector = when (this) {
    AppMode.Preheat -> ModeIcons.Preheat
    AppMode.ManualHold -> ModeIcons.Heat
    AppMode.Drying -> ModeIcons.Drying
    AppMode.AutoStandby -> ModeIcons.Printer
    AppMode.AutoStandbyTempering -> ModeIcons.Printer
    AppMode.Tempering -> ModeIcons.Cooldown
    AppMode.SafeStop -> Icons.Outlined.PowerSettingsNew
}

@Composable
fun ModeIcon(mode: AppMode, size: Dp = 36.dp, tint: Color = LocalContentColor.current) {
    if (mode == AppMode.AutoStandbyTempering) {
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp),
            verticalAlignment = Alignment.CenterVertically) {
            Icon(ModeIcons.Printer, contentDescription = null, tint = tint, modifier = Modifier.size(size))
            Icon(Icons.Outlined.Add, contentDescription = null, tint = tint, modifier = Modifier.size(size * 0.6f))
            Icon(ModeIcons.Cooldown, contentDescription = null, tint = tint, modifier = Modifier.size(size))
        }
    } else {
        Icon(mode.actionIcon(), contentDescription = null, tint = tint, modifier = Modifier.size(size))
    }
}

@Composable
fun ActionLabel(mode: AppMode, label: String) {
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically) {
        ModeIcon(mode, size = 20.dp)
        Text(label, modifier = Modifier.weight(1f, fill = false))
    }
}

@Composable
fun ActionLabel(icon: ImageVector, label: String) {
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically) {
        // The adjacent text supplies the accessible name; avoid double announcements.
        Icon(icon, contentDescription = null, modifier = Modifier.size(20.dp))
        Text(label, modifier = Modifier.weight(1f, fill = false))
    }
}
