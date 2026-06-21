/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import com.alphastudio.snapheateru1.ui.SnapHeaterApp
import com.alphastudio.snapheateru1.ui.theme.SnapHeaterTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            SnapHeaterTheme {
                SnapHeaterApp()
            }
        }
    }
}
