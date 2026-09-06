/*
 * SnapHeater U1
 * Copyright (c) 2026 Damian Borkowski
 * SPDX-License-Identifier: MIT
 */

package com.alphastudio.snapheateru1

import android.os.Bundle
import android.content.Intent
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateOf
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import com.alphastudio.snapheateru1.ui.SnapHeaterApp
import com.alphastudio.snapheateru1.ui.AppLanguageProvider
import com.alphastudio.snapheateru1.ui.theme.SnapHeaterTheme

class MainActivity : ComponentActivity() {
    private var notificationDevice by mutableStateOf("")
    private fun readNotification(intent: Intent?) {
        notificationDevice=intent?.getStringExtra("notification_device").orEmpty()
            .takeIf {it.matches(Regex("[a-fA-F0-9]{12}"))}.orEmpty()
    }
    override fun onNewIntent(intent: Intent) {super.onNewIntent(intent);setIntent(intent);readNotification(intent)}
    override fun onStart() { super.onStart(); com.alphastudio.snapheateru1.data.NotificationMonitorService.uiVisible=true }
    override fun onStop() { com.alphastudio.snapheateru1.data.NotificationMonitorService.uiVisible=false; super.onStop() }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        readNotification(intent)
        setContent {
            AppLanguageProvider(this) {
                SnapHeaterTheme {
                    SnapHeaterApp(notificationDevice) {notificationDevice="";intent?.removeExtra("notification_device")}
                }
            }
        }
    }
}
