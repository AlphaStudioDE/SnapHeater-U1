/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1.ui.theme

import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

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

private val SnapTypography = Typography(
    titleLarge = TextStyle(fontSize = 24.sp, lineHeight = 30.sp, fontWeight = FontWeight.Bold),
    titleMedium = TextStyle(fontSize = 18.sp, lineHeight = 24.sp, fontWeight = FontWeight.Bold),
    titleSmall = TextStyle(fontSize = 15.sp, lineHeight = 20.sp, fontWeight = FontWeight.Bold),
    bodyLarge = TextStyle(fontSize = 16.sp, lineHeight = 23.sp, fontWeight = FontWeight.Normal),
    bodyMedium = TextStyle(fontSize = 14.sp, lineHeight = 20.sp, fontWeight = FontWeight.Normal),
    bodySmall = TextStyle(fontSize = 12.sp, lineHeight = 17.sp, fontWeight = FontWeight.Normal),
    labelLarge = TextStyle(fontSize = 14.sp, lineHeight = 18.sp, fontWeight = FontWeight.Bold),
    labelMedium = TextStyle(fontSize = 12.sp, lineHeight = 16.sp, fontWeight = FontWeight.SemiBold),
)

private val SnapShapes = Shapes(
    extraSmall = androidx.compose.foundation.shape.RoundedCornerShape(6.dp),
    small = androidx.compose.foundation.shape.RoundedCornerShape(8.dp),
    medium = androidx.compose.foundation.shape.RoundedCornerShape(8.dp),
    large = androidx.compose.foundation.shape.RoundedCornerShape(8.dp),
    extraLarge = androidx.compose.foundation.shape.RoundedCornerShape(8.dp),
)

@Composable
fun SnapHeaterTheme(
    content: @Composable () -> Unit,
) {
    MaterialTheme(
        colorScheme = DarkColors,
        typography = SnapTypography,
        shapes = SnapShapes,
        content = content,
    )
}
