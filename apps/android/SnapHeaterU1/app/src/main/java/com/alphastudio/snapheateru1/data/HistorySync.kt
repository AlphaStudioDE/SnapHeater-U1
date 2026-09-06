package com.alphastudio.snapheateru1.data

import android.content.Context
import android.os.SystemClock
import com.alphastudio.snapheateru1.ble.SnapHeaterBleGattClient
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import org.json.JSONObject

class HistorySync(private val context: Context, private val store: TemperatureHistory) {
    /** At most two bounded requests. Call between control polls, not in a catch-up loop. */
    suspend fun step(address: String, device: String): Boolean {
        val expected=store.cursor(device)
        suspend fun fetch(after: Long): Pair<JSONObject,Long> {
            val wall=System.currentTimeMillis()
            val started=SystemClock.elapsedRealtime()
            val json=if(address.startsWith("ble://"))
                JSONObject(SnapHeaterBleGattClient(context,address.removePrefix("ble://")).readHistory(after))
            else SnapHeaterApiClient(address,RestCredentials(context).get(address)).history(after)
            currentCoroutineContext().ensureActive()
            val elapsed=SystemClock.elapsedRealtime()-started
            return json to historyAnchor(wall,elapsed,json.getLong("now_s"))
        }
        val batch=fetchHistoryBatch(device,expected,::fetch)
        currentCoroutineContext().ensureActive()
        store.ingest(batch.page,expected,batch.anchor)
        return batch.page.next<batch.page.last
    }
}
