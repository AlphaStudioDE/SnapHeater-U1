package com.alphastudio.snapheateru1.data

import android.app.Service
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.alphastudio.snapheateru1.MainActivity
import com.alphastudio.snapheateru1.R
import kotlinx.coroutines.*

/** One explicitly selected Panda; never discovers or substitutes another printer/device. */
class NotificationMonitorService : Service() {
    companion object {
        @Volatile var uiVisible=false
        @Volatile var activeAddress=""
        fun stop(context: Context) { context.stopService(Intent(context,NotificationMonitorService::class.java)) }
    }
    private val scope=CoroutineScope(SupervisorJob()+Dispatchers.IO)
    private var worker: Job?=null
    override fun onBind(intent: Intent?): IBinder?=null
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if(intent?.action=="stop") {stopSelf();return START_NOT_STICKY}
        val address=intent?.getStringExtra("address").orEmpty()
        val device=intent?.getStringExtra("device").orEmpty()
        if(address.isBlank() || !device.matches(Regex("[A-Fa-f0-9]{12}"))) {stopSelf();return START_NOT_STICKY}
        activeAddress=address
        worker?.cancel()
        val manager=getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(NotificationChannel("panda_monitor",getString(R.string.monitor_title),NotificationManager.IMPORTANCE_LOW))
        val open=PendingIntent.getActivity(this,0,Intent(this,MainActivity::class.java),PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val stop=PendingIntent.getService(this,1,Intent(this,NotificationMonitorService::class.java).setAction("stop"),PendingIntent.FLAG_IMMUTABLE)
        fun notification(message: String)=NotificationCompat.Builder(this,"panda_monitor")
            .setSmallIcon(android.R.drawable.ic_dialog_info).setContentTitle(getString(R.string.monitor_title))
            .setContentText(message).setContentIntent(open).setOngoing(true)
            .addAction(0,getString(R.string.monitor_stop),stop).build()
        startForeground(7000,notification(getString(R.string.monitor_active)))
        worker=scope.launch {
            while(isActive) {
                if(!uiVisible) {
                    val message=try {
                        val page=DeviceEvents.fetch(applicationContext,address)
                        ensureActive()
                        DeviceEvents.receive(applicationContext,device,page)
                        getString(R.string.monitor_active)
                    } catch(cancelled: CancellationException) {throw cancelled}
                    catch(_: Exception) {getString(R.string.monitor_disconnected)}
                    manager.notify(7000,notification(message))
                }
                delay(15000)
            }
        }
        return START_NOT_STICKY // Never resume stale device selection after process death.
    }
    override fun onDestroy() {
        scope.cancel(); activeAddress=""
        super.onDestroy()
    }
}
