package com.alphastudio.snapheateru1.ui

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.app.NotificationCompat
import com.alphastudio.snapheateru1.MainActivity
import com.alphastudio.snapheateru1.R

/** Advisory only; receiving or dismissing it never changes heater operation. */
fun postVirtualDoorNotification(context: Context, device: String, message: String) {
    val manager = context.getSystemService(NotificationManager::class.java)
    manager.createNotificationChannel(NotificationChannel("virtual_doors", context.getString(R.string.vdoor_title), NotificationManager.IMPORTANCE_DEFAULT))
    val intent = PendingIntent.getActivity(context, 0, Intent(context, MainActivity::class.java), PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    runCatching {
        manager.notify(device, 7001, NotificationCompat.Builder(context, "virtual_doors")
            .setSmallIcon(android.R.drawable.ic_dialog_alert)
            .setContentTitle(context.getString(R.string.vdoor_title))
            .setContentText(message).setStyle(NotificationCompat.BigTextStyle().bigText(message))
            .setContentIntent(intent).setAutoCancel(true).build())
    }
}
