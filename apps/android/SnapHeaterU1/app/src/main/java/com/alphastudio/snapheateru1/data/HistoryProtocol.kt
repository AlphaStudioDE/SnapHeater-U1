package com.alphastudio.snapheateru1.data

import org.json.JSONArray
import org.json.JSONObject

data class HistoryCursor(val boot: String="", val sequence: Long=0)
data class HistorySample(val sequence: Long, val seconds: Long, val chamber: Double?,
    val ptc: Double?, val target: Double?, val flags: Int, val fault: Int)
data class HistoryPage(val device: String, val boot: String, val nowSeconds: Long,
    val first: Long, val next: Long, val last: Long, val gap: Boolean, val samples: List<HistorySample>)
data class HistoryBatch(val page: HistoryPage, val anchor: Long)

suspend fun fetchHistoryBatch(device: String, expected: HistoryCursor,
    fetch: suspend (Long)->Pair<JSONObject,Long>): HistoryBatch {
    var after=expected.sequence
    var fetched=fetch(after)
    require(fetched.first.optString("device_id").equals(device,true)) { "Different Panda at this address" }
    if(fetched.first.optString("boot")!=expected.boot && after>0) {
        after=0
        fetched=fetch(0)
    }
    return HistoryBatch(parseHistoryPage(fetched.first,device,after),fetched.second)
}

private fun unsigned(value: Any): Long {
    require(value is Number)
    val n=value.toDouble()
    require(n.isFinite() && n>=0 && n<=4294967295.0 && n==n.toLong().toDouble())
    return n.toLong()
}
private fun JSONArray.temperature(index: Int): Double? {
    if(isNull(index)) return null
    val raw=get(index); require(raw is Number)
    return raw.toDouble().also { require(it.isFinite() && it in -3276.0..3276.0) }
}

/** Fail closed on malformed or mixed-device pages; never advance a cursor for them. */
fun parseHistoryPage(root: JSONObject, expectedDevice: String, after: Long): HistoryPage {
    require(after in 0..4294967295L)
    val device=root.getString("device_id").uppercase()
    require(device.matches(Regex("[A-F0-9]{12}")) && device.equals(expectedDevice,true))
    val boot=root.getString("boot")
    require(boot.matches(Regex("[a-fA-F0-9]{16}")))
    val now=unsigned(root.get("now_s"))
    val first=unsigned(root.get("first")); val last=unsigned(root.get("last"))
    val next=unsigned(root.get("next"))
    require(first>=1 && first<=last+1 && (last==0L || first<=last))
    val effectiveAfter=if(after>last) 0 else after
    val expectedFirst=maxOf(effectiveAfter+1,first)
    val rows=root.getJSONArray("samples")
    require(rows.length()<=32)
    val samples=(0 until rows.length()).map { index ->
        val row=rows.getJSONArray(index); require(row.length()==7)
        val seq=unsigned(row.get(0)); val seconds=unsigned(row.get(1))
        val flags=unsigned(row.get(5)); val fault=unsigned(row.get(6))
        require(seq==expectedFirst+index && seq<=last && seconds<=now && flags<=7 && fault<=255)
        HistorySample(seq,seconds,row.temperature(2),row.temperature(3),row.temperature(4),flags.toInt(),fault.toInt())
    }
    require(samples.zipWithNext().all { (a,b)-> b.seconds>a.seconds })
    require(samples.size==minOf(32L,(last-expectedFirst+1).coerceAtLeast(0)).toInt())
    require(next==(samples.lastOrNull()?.sequence ?: effectiveAfter))
    val gap=root.get("gap"); require(gap is Boolean && gap==(effectiveAfter<first-1))
    return HistoryPage(device,boot,now,first,next,last,gap,samples)
}

/** No double-counting retries, and no reuse of the previous boot's sequence. */
fun historyAnchor(requestWallMs: Long, elapsedMs: Long, nowSeconds: Long): Long {
    require(elapsedMs in 0..60000 && nowSeconds in 0..4294967295L)
    return requestWallMs+elapsedMs/2-nowSeconds*1000
}
