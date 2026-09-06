package com.alphastudio.snapheateru1.data

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import org.json.JSONObject
import java.util.UUID

data class RecordedEvent(val time: Long, val code: String, val level: String, val approximate: Boolean)

/** Independent from notification permission/receipt. Device + boot + sequence deduplicates. */
class EventHistory(context: Context) : SQLiteOpenHelper(context,"event_history.db",null,1) {
    init { setWriteAheadLoggingEnabled(true) }
    override fun onCreate(db: SQLiteDatabase) {
        db.execSQL("CREATE TABLE events (device TEXT NOT NULL, boot TEXT NOT NULL, seq TEXT NOT NULL, time INTEGER NOT NULL, code TEXT NOT NULL, level TEXT NOT NULL, approximate INTEGER NOT NULL, PRIMARY KEY(device,boot,seq))")
        db.execSQL("CREATE INDEX event_time ON events(device,time)")
        db.execSQL("CREATE TABLE event_anchors (device TEXT NOT NULL, boot TEXT NOT NULL, anchor INTEGER NOT NULL, PRIMARY KEY(device,boot))")
    }
    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) = Unit
    @Synchronized fun ingest(device: String, page: JSONObject, receivedAt: Long=System.currentTimeMillis()) {
        val key=device.uppercase()
        require(key.matches(Regex("[A-F0-9]{12}")) && page.getString("device_id").equals(key,true))
        val boot=page.getString("boot")
        require(boot.matches(Regex("[a-fA-F0-9]{16}")))
        val now=page.optLong("now_ms",-1)
        require(now>=-1)
        val db=writableDatabase
        db.beginTransaction()
        try {
            val anchor=db.rawQuery("SELECT anchor FROM event_anchors WHERE device=? AND boot=?",arrayOf(key,boot)).use {
                if(it.moveToFirst()) it.getLong(0) else receivedAt-now.coerceAtLeast(0)
            }
            if(now>=0) db.insertWithOnConflict("event_anchors",null,ContentValues().apply {
                put("device",key);put("boot",boot);put("anchor",anchor)
            },SQLiteDatabase.CONFLICT_IGNORE)
            val rows=page.getJSONArray("events")
            require(rows.length()<=32)
            for(i in 0 until rows.length()) {
                val row=rows.getJSONObject(i)
                val seq=row.getLong("seq"); val ms=row.optLong("ms",-1)
                val code=row.getString("code");val level=row.getString("level")
                require(seq in 1..0xFFFFFFFFL && code.length<=32 && level.length<=8)
                require(ms>=-1 && (now<0 || ms<=now))
                if(level in listOf("warn","error","critical") || code.startsWith("sensor_freeze_") || code=="heater_fault_cleared") {
                    insert(db,key,boot,seq.toString(),if(now>=0 && ms>=0) anchor+ms else receivedAt,code,level,true)
                }
            }
            db.setTransactionSuccessful()
        } finally {db.endTransaction()}
    }
    private fun insert(db: SQLiteDatabase,device: String,boot: String,seq: String,time: Long,code: String,level: String,approximate: Boolean) {
        check(db.insertWithOnConflict("events",null,ContentValues().apply {
            put("device",device);put("boot",boot);put("seq",seq);put("time",time)
            put("code",code);put("level",level);put("approximate",if(approximate) 1 else 0)
        },SQLiteDatabase.CONFLICT_IGNORE)!=-1L || db.rawQuery("SELECT 1 FROM events WHERE device=? AND boot=? AND seq=?",arrayOf(device,boot,seq)).use {it.moveToFirst()})
    }
    @Synchronized fun action(device: String,code: String) {
        require(device.matches(Regex("[a-fA-F0-9]{12}")))
        insert(writableDatabase,device.uppercase(),"phone",UUID.randomUUID().toString(),System.currentTimeMillis(),code,"info",false)
    }
    @Synchronized fun read(device: String, limit: Int): List<RecordedEvent> =
        readableDatabase.rawQuery("SELECT time,code,level,approximate FROM events WHERE device=? ORDER BY time DESC,boot DESC,seq DESC LIMIT ?",arrayOf(device.uppercase(),limit.coerceIn(1,10000).toString())).use {
            buildList {while(it.moveToNext()) add(RecordedEvent(it.getLong(0),it.getString(1),it.getString(2),it.getInt(3)!=0))}
        }
}
