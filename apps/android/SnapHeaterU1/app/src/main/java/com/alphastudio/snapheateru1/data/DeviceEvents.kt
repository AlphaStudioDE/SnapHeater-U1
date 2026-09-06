package com.alphastudio.snapheateru1.data

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import com.alphastudio.snapheateru1.MainActivity
import com.alphastudio.snapheateru1.R
import com.alphastudio.snapheateru1.ble.SnapHeaterBleGattClient
import org.json.JSONObject

data class AdvisoryEvent(val sequence: Long, val code: String, val level: String)
fun unreadEvents(page: JSONObject, boot: String, cursor: Long): List<AdvisoryEvent> {
    val seen=if(page.getString("boot")==boot) cursor else 0
    val rows=page.getJSONArray("events")
    return (0 until rows.length()).map { rows.getJSONObject(it) }
        .filter { it.getLong("seq")>seen }
        .map { AdvisoryEvent(it.getLong("seq"),it.getString("code"),it.getString("level")) }
        .sortedBy { it.sequence }
}

/** Local receipt is saved only after Android accepts the notification request. */
object DeviceEvents {
    suspend fun fetch(context: Context, address: String): JSONObject =
        if(address.startsWith("ble://"))
            JSONObject(SnapHeaterBleGattClient(context,address.removePrefix("ble://")).readEvents())
        else SnapHeaterApiClient(address,RestCredentials(context).get(address)).notifications()

    @Synchronized fun receive(context: Context, expectedDevice: String, page: JSONObject) {
        require(page.getString("device_id").equals(expectedDevice,true)) { "Different Panda at this address" }
        // Persist even when Android notifications are disabled or cannot be delivered.
        val storageFailure=runCatching {EventHistory(context).use { it.ingest(expectedDevice,page) }}.exceptionOrNull()
        val manager=context.getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(NotificationChannel("panda_events",context.getString(R.string.events_title),NotificationManager.IMPORTANCE_DEFAULT))
        if(!NotificationManagerCompat.from(context).areNotificationsEnabled() ||
            manager.getNotificationChannel("panda_events").importance==NotificationManager.IMPORTANCE_NONE) {
            storageFailure?.let {throw it}; return
        }
        val preferences=context.getSharedPreferences("event_receipts",Context.MODE_PRIVATE)
        val key=expectedDevice.uppercase()
        val boot=page.getString("boot")
        val previousBoot=preferences.getString(key+"_boot","").orEmpty()
        val previous=preferences.getLong(key+"_seq",0)
        val events=unreadEvents(page,previousBoot,previous)
        val intent=PendingIntent.getActivity(context,0,Intent(context,MainActivity::class.java)
            .setAction("open_panda_$key").putExtra("notification_device",key)
            .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        for(event in events) {
            val warning=event.level in listOf("warn","error","critical")
            val complete=event.code.contains("complete") || event.code in listOf("health_test_ok","health_test_weak","stability_report_ready")
            if(warning || complete) {
                val title=context.getString(if(warning) R.string.events_warning else R.string.events_complete)
                val message="SH_${key.takeLast(4)} · ${eventDescription(context,event.code)}"
                manager.notify(key+"_"+boot,event.sequence.toInt(),NotificationCompat.Builder(context,"panda_events")
                    .setSmallIcon(android.R.drawable.ic_dialog_alert).setContentTitle(title)
                    .setContentText(message).setStyle(NotificationCompat.BigTextStyle().bigText(message))
                    .setContentIntent(intent).setAutoCancel(true).build())
            }
            check(preferences.edit().putString(key+"_boot",boot).putLong(key+"_seq",event.sequence).commit())
        }
        if(events.isNotEmpty() && previousBoot==boot && events.first().sequence>previous+1 && previous>0) {
            manager.notify(key,6999,NotificationCompat.Builder(context,"panda_events")
                .setSmallIcon(android.R.drawable.ic_dialog_alert).setContentTitle(context.getString(R.string.events_title))
                .setContentText(context.getString(R.string.events_gap)).setContentIntent(intent).setAutoCancel(true).build())
        }
        storageFailure?.let {throw it} // Report storage failure without suppressing safety notifications.
    }
}

fun eventDescription(context: Context, code: String): String { return context.getString(when(code) {
    "sensor_freeze_warning" -> R.string.freeze_event_warning
    "sensor_raw_frozen" -> R.string.freeze_event_stopped
    "sensor_freeze_ended" -> R.string.freeze_event_ended
    "sensor_sample_stale" -> R.string.freeze_event_stale
    "heater_fault_latched" -> R.string.freeze_event_latched
    "heater_fault_cleared" -> R.string.freeze_event_cleared
    "phone_freeze_continue" -> R.string.freeze_event_continue
    "phone_freeze_stop_requested" -> R.string.freeze_event_stop_requested
    "phone_freeze_stop_confirmed" -> R.string.freeze_event_stop_confirmed
    "phone_freeze_stop_failed" -> R.string.freeze_event_stop_failed
    else -> return code
}) }
