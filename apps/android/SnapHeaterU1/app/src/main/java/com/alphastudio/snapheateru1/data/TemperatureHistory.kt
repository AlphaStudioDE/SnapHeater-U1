package com.alphastudio.snapheateru1.data

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import com.alphastudio.snapheateru1.model.AppMode
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import java.io.Writer

data class TemperaturePoint(val time: Long, val chamber: Double?, val ptc: Double?, val target: Double?,
    val paused: Boolean, val segment: String="phone", val sequence: Long=0)
data class HistoryGap(val time: Long, val kind: String, val missing: Long)

/** Phone-private store. Cursor and samples always commit in the same transaction. */
class TemperatureHistory(context: Context) : SQLiteOpenHelper(context, "temperature_history.db", null, 2) {
    init { setWriteAheadLoggingEnabled(true) }
    override fun onCreate(db: SQLiteDatabase) {
        db.execSQL("CREATE TABLE samples (device TEXT NOT NULL, time INTEGER NOT NULL, chamber INTEGER NOT NULL, ptc INTEGER NOT NULL, target INTEGER NOT NULL, paused INTEGER NOT NULL, PRIMARY KEY(device,time))")
        createDeviceTables(db)
    }
    private fun createDeviceTables(db: SQLiteDatabase) {
        db.execSQL("CREATE TABLE IF NOT EXISTS device_samples (device TEXT NOT NULL, boot TEXT NOT NULL, seq INTEGER NOT NULL, time INTEGER NOT NULL, chamber REAL, ptc REAL, target REAL, paused INTEGER NOT NULL, flags INTEGER NOT NULL, fault INTEGER NOT NULL, PRIMARY KEY(device,boot,seq))")
        db.execSQL("CREATE INDEX IF NOT EXISTS history_time ON device_samples(device,time)")
        db.execSQL("CREATE TABLE IF NOT EXISTS history_boots (device TEXT NOT NULL, boot TEXT NOT NULL, anchor INTEGER NOT NULL, first_ms INTEGER, last_ms INTEGER, PRIMARY KEY(device,boot))")
        db.execSQL("CREATE TABLE IF NOT EXISTS history_cursor (device TEXT PRIMARY KEY, boot TEXT NOT NULL, seq INTEGER NOT NULL)")
        db.execSQL("CREATE TABLE IF NOT EXISTS history_gaps (device TEXT NOT NULL, boot TEXT NOT NULL, seq INTEGER NOT NULL, time INTEGER NOT NULL, kind TEXT NOT NULL, missing INTEGER NOT NULL, PRIMARY KEY(device,boot,seq,kind))")
    }
    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        if(oldVersion<2) createDeviceTables(db) // Preserve all v1 phone measurements.
    }
    @Synchronized fun cursor(device: String): HistoryCursor =
        readableDatabase.rawQuery("SELECT boot,seq FROM history_cursor WHERE device=?",arrayOf(device.uppercase())).use {
            if(it.moveToFirst()) HistoryCursor(it.getString(0),it.getLong(1)) else HistoryCursor()
        }

    @Synchronized fun ingest(page: HistoryPage, expected: HistoryCursor, proposedAnchor: Long) {
        val db=writableDatabase
        db.beginTransaction()
        try {
            check(cursor(page.device)==expected) { "History cursor changed during request" }
            check(page.boot!=expected.boot || page.next>=expected.sequence) { "History moved backwards within one boot" }
            val anchor=db.rawQuery("SELECT anchor FROM history_boots WHERE device=? AND boot=?",arrayOf(page.device,page.boot)).use {
                if(it.moveToFirst()) it.getLong(0) else proposedAnchor
            }
            val bootValues=ContentValues().apply {
                put("device",page.device);put("boot",page.boot);put("anchor",anchor)
            }
            db.insertWithOnConflict("history_boots",null,bootValues,SQLiteDatabase.CONFLICT_IGNORE)
            fun gap(seq: Long, time: Long, kind: String, missing: Long) {
                val values=ContentValues().apply {
                    put("device",page.device);put("boot",page.boot);put("seq",seq);put("time",time)
                    put("kind",kind);put("missing",missing)
                }
                db.insertWithOnConflict("history_gaps",null,values,SQLiteDatabase.CONFLICT_IGNORE)
            }
            if(expected.boot.isNotBlank() && expected.boot!=page.boot)
                gap(0,anchor+(page.samples.firstOrNull()?.seconds ?: page.nowSeconds)*1000,"restart",-1)
            if(page.gap) {
                val start=if(expected.boot==page.boot) expected.sequence else 0
                gap(page.first,anchor+(page.samples.firstOrNull()?.seconds ?: page.nowSeconds)*1000,"overwritten",page.first-start-1)
            }
            page.samples.forEach { sample ->
                val values=ContentValues().apply {
                    put("device",page.device);put("boot",page.boot);put("seq",sample.sequence)
                    put("time",anchor+sample.seconds*1000)
                    put("chamber",sample.chamber);put("ptc",sample.ptc);put("target",sample.target)
                    put("paused",if(sample.flags and 4!=0) 1 else 0)
                    put("flags",sample.flags);put("fault",sample.fault)
                }
                db.insertWithOnConflict("device_samples",null,values,SQLiteDatabase.CONFLICT_IGNORE)
                if(sample.chamber==null || sample.ptc==null)
                    gap(sample.sequence,anchor+sample.seconds*1000,"invalid_sensor",1)
            }
            if(page.samples.isNotEmpty()) {
                val first=anchor+page.samples.first().seconds*1000
                val last=anchor+page.samples.last().seconds*1000
                db.execSQL("UPDATE history_boots SET first_ms=MIN(COALESCE(first_ms,?),?), last_ms=MAX(COALESCE(last_ms,?),?) WHERE device=? AND boot=?",
                    arrayOf(first,first,last,last,page.device,page.boot))
            }
            db.insertWithOnConflict("history_cursor",null,ContentValues().apply {
                put("device",page.device);put("boot",page.boot);put("seq",page.next)
            },SQLiteDatabase.CONFLICT_REPLACE)
            db.setTransactionSuccessful()
        } finally { db.endTransaction() }
    }

    /** Compatibility with older firmware only, until the first successful ring page. */
    @Synchronized fun record(snapshot: HeaterSnapshot, now: Long = System.currentTimeMillis()) {
        if (!snapshot.historyReadingValid || !snapshot.deviceId.matches(Regex("[a-fA-F0-9]{12}"))) return
        val device = snapshot.deviceId.uppercase()
        if(cursor(device).boot.isNotBlank()) return
        readableDatabase.rawQuery("SELECT MAX(time) FROM samples WHERE device=?", arrayOf(device)).use {
            if (it.moveToFirst() && !it.isNull(0) && now >= it.getLong(0) && now-it.getLong(0)<10_000) return
        }
        writableDatabase.insertOrThrow("samples", null, ContentValues().apply {
            put("device",device);put("time",now);put("chamber",snapshot.chamberC);put("ptc",snapshot.ptcC)
            put("target",if(snapshot.mode==AppMode.SafeStop || snapshot.paused) 0 else snapshot.effectiveTargetC)
            put("paused",if(snapshot.paused) 1 else 0)
        })
    }

    // Panda is authoritative inside synchronized boot coverage. Legacy rows are retained
    // physically, but hidden in overlap to avoid displaying two independent samplings.
    private val combined = """
        SELECT time,chamber,ptc,target,paused,boot AS segment,seq,flags,fault FROM device_samples WHERE device=?
        UNION ALL
        SELECT s.time,s.chamber,s.ptc,s.target,s.paused,'phone',0,0,0 FROM samples s WHERE s.device=?
        AND NOT EXISTS (SELECT 1 FROM history_boots b WHERE b.device=s.device AND s.time BETWEEN b.first_ms-10000 AND b.last_ms+10000)
    """.trimIndent()

    @Synchronized fun read(device: String, since: Long): List<TemperaturePoint> =
        readableDatabase.rawQuery("SELECT * FROM ($combined) WHERE time>=? ORDER BY time DESC,segment,seq LIMIT 10000",
            arrayOf(device.uppercase(),device.uppercase(),since.toString())).use { c ->
            buildList {
                while(c.moveToNext()) add(TemperaturePoint(c.getLong(0),
                    if(c.isNull(1)) null else c.getDouble(1),if(c.isNull(2)) null else c.getDouble(2),
                    if(c.isNull(3)) null else c.getDouble(3),c.getInt(4)!=0,c.getString(5),c.getLong(6)))
            }.asReversed()
        }

    @Synchronized fun gaps(device: String, since: Long): List<HistoryGap> =
        readableDatabase.rawQuery("SELECT time,kind,missing FROM history_gaps WHERE device=? AND time>=? ORDER BY time",
            arrayOf(device.uppercase(),since.toString())).use { c ->
            buildList { while(c.moveToNext()) add(HistoryGap(c.getLong(0),c.getString(1),c.getLong(2))) }
        }

    /** Stream the selected device only; null values and loss markers remain explicit. */
    fun export(device: String, writer: Writer) {
        writer.write("record_type,unix_time_ms,chamber_c,ptc_c,target_c,paused,boot,sequence,flags,fault,gap_kind,missing_samples\n")
        val measurements="SELECT 'sample' AS kind,time,chamber,ptc,target,paused,segment,seq,flags,fault,'' AS gap_kind,0 AS missing FROM ($combined)"
        val gaps="SELECT 'gap',time,NULL,NULL,NULL,NULL,boot,seq,NULL,NULL,kind,missing FROM history_gaps WHERE device=?"
        readableDatabase.rawQuery("$measurements UNION ALL $gaps ORDER BY time",
            arrayOf(device.uppercase(),device.uppercase(),device.uppercase())).use { c ->
            while(c.moveToNext()) writer.write((0 until c.columnCount).joinToString(",") {
                if(c.isNull(it)) "" else c.getString(it)
            }+"\n")
        }
    }
}
