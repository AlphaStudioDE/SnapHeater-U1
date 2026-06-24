/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.theme

import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val SnapOrange = Color(0xFFFF7A1A)
private val DeepOrange = Color(0xFFE85D04)
private val HeatRed = Color(0xFFFF4D2E)
private val PureBlack = Color(0xFF050505)
private val PanelBlack = Color(0xFF101010)
private val PanelGray = Color(0xFF1C1C1C)
private val SoftWhite = Color(0xFFF6F6F6)
private val MutedWhite = Color(0xFFC8C8C8)

private val DarkColors = darkColorScheme(
    primary = SnapOrange,
    onPrimary = Color.Black,
    primaryContainer = Color(0xFF2A1608),
    onPrimaryContainer = Color(0xFFFFB47A),
    secondary = DeepOrange,
    onSecondary = Color.Black,
    secondaryContainer = Color(0xFF241207),
    onSecondaryContainer = Color(0xFFFFB68A),
    tertiary = HeatRed,
    onTertiary = Color.Black,
    tertiaryContainer = Color(0xFF2C0D08),
    onTertiaryContainer = Color(0xFFFFB4A5),
    background = PureBlack,
    onBackground = SoftWhite,
    surface = PanelBlack,
    onSurface = SoftWhite,
    surfaceVariant = PanelGray,
    onSurfaceVariant = MutedWhite,
    outline = Color(0xFF5A5A5A),
    outlineVariant = Color(0xFF303030),
    inverseSurface = SoftWhite,
    inverseOnSurface = PureBlack,
)

@Composable
fun SnapHeaterTheme(
    content: @Composable () -> Unit,
) {
    MaterialTheme(
        colorScheme = DarkColors,
        typography = MaterialTheme.typography,
        content = content,
    )
}
