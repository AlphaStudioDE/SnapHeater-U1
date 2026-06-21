package com.example.snapheater

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import androidx.core.app.NotificationCompat

/**
 * Minimal helper. In the real app call maybeShowPreheatDoneNotification(statusJson)
 * from the BLE status callback. The app should run BLE in a foreground service if
 * notifications must work reliably in the background.
 */
class SnapHeaterPreheatNotificationHelper(private val context: Context) {
    private val channelId = "snapheater_preheat"
    private var alreadyShownForCurrentEvent = false

    init {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(channelId, "SnapHeater preheat", NotificationManager.IMPORTANCE_DEFAULT)
            context.getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
        }
    }

    fun maybeShowPreheatDoneNotification(statusJson: String, acknowledge: () -> Unit) {
        val done = statusJson.contains("\"pd\":true") || statusJson.contains("\"preheat_complete_pending\":true")
        if (!done) {
            alreadyShownForCurrentEvent = false
            return
        }
        if (alreadyShownForCurrentEvent) return
        alreadyShownForCurrentEvent = true

        val notification = NotificationCompat.Builder(context, channelId)
            .setSmallIcon(android.R.drawable.stat_notify_more)
            .setContentTitle("SnapHeater U1")
            .setContentText("Preheating complete. Chamber hold time finished.")
            .setAutoCancel(true)
            .build()

        context.getSystemService(NotificationManager::class.java).notify(1001, notification)
        acknowledge()
    }
}
