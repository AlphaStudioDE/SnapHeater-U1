/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val SnapYellow = Color(0xFFFFC928)
private val HeaterTeal = Color(0xFF0EA5A3)
private val PanelInk = Color(0xFF151820)
private val WarmRed = Color(0xFFE05A47)

private val DarkColors = darkColorScheme(
    primary = SnapYellow,
    onPrimary = Color(0xFF1F1A00),
    primaryContainer = Color(0xFF3D3208),
    onPrimaryContainer = Color(0xFFFFE38A),
    secondary = HeaterTeal,
    secondaryContainer = Color(0xFF123F42),
    onSecondaryContainer = Color(0xFF8CEBE7),
    tertiary = WarmRed,
    tertiaryContainer = Color(0xFF4B211B),
    onTertiaryContainer = Color(0xFFFFB5A9),
    background = Color(0xFF0F1117),
    onBackground = Color(0xFFE8EAED),
    surface = PanelInk,
    onSurface = Color(0xFFF2F3F5),
    surfaceVariant = Color(0xFF242A33),
    onSurfaceVariant = Color(0xFFBEC6D0),
    outline = Color(0xFF4A5360),
)

private val LightColors = lightColorScheme(
    primary = Color(0xFF8A6800),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFFFE7A3),
    onPrimaryContainer = Color(0xFF2A2100),
    secondary = Color(0xFF007A78),
    secondaryContainer = Color(0xFFB9EFEC),
    onSecondaryContainer = Color(0xFF002A2B),
    tertiary = Color(0xFFB13E2F),
    tertiaryContainer = Color(0xFFFFDAD4),
    onTertiaryContainer = Color(0xFF410000),
    background = Color(0xFFF7F8FA),
    onBackground = Color(0xFF1B1D22),
    surface = Color.White,
    onSurface = Color(0xFF1B1D22),
    surfaceVariant = Color(0xFFE9EDF2),
    onSurfaceVariant = Color(0xFF4E5662),
    outline = Color(0xFF7E8793),
)

@Composable
fun SnapHeaterTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    val colors: ColorScheme = if (darkTheme) DarkColors else LightColors

    MaterialTheme(
        colorScheme = colors,
        typography = MaterialTheme.typography,
        content = content,
    )
}
