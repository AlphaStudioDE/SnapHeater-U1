package com.alphastudio.snapheateru1

import com.alphastudio.snapheateru1.data.*
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test

class HistoryProtocolTest {
    private val device="AABBCCDDEEFF"
    private fun page(first: Long=1,last: Long=32,after: Long=0): JSONObject {
        val start=maxOf(first,after+1)
        val rows=JSONArray()
        if(start<=last) for(i in start..minOf(start+31,last))
            rows.put(JSONArray().put(i).put(i*10).put(35.1).put(42.2).put(45).put(0).put(0))
        return JSONObject().put("device_id",device).put("boot","0123456789abcdef")
            .put("now_s",maxOf(last*10,0)).put("first",first).put("last",last)
            .put("next",if(rows.length()>0) rows.getJSONArray(rows.length()-1).getLong(0) else after)
            .put("gap",after<first-1).put("samples",rows)
    }
    private fun rejects(json: JSONObject, after: Long=0) {
        assertTrue(runCatching {parseHistoryPage(json,device,after)}.isFailure)
    }
    @Test fun pagesAreContiguousAndReplayHasStableIdentity() {
        val raw=page(last=40)
        val first=parseHistoryPage(raw,device,0)
        assertEquals(first,parseHistoryPage(raw,device,0))
        assertEquals(32L,first.next)
        assertEquals(8,parseHistoryPage(page(last=40,after=32),device,32).samples.size)
    }
    @Test fun overwriteIsVisibleEvenOnFirstConnection() {
        val result=parseHistoryPage(page(first=81,last=800),device,0)
        assertTrue(result.gap); assertEquals(81L,result.samples.first().sequence)
    }
    @Test fun malformedAndWrongDevicePagesAreRejected() {
        rejects(page().put("device_id","FFFFFFFFFFFF"))
        rejects(page().put("next",31))
        rejects(page().put("last",-1))
        rejects(page().put("first",1.5))
        rejects(page().put("gap",true))
        rejects(page().put("boot","unexpected"))
        val unordered=page()
        unordered.getJSONArray("samples").getJSONArray(1).put(0,7)
        rejects(unordered)
        val future=page()
        future.getJSONArray("samples").getJSONArray(0).put(1,99999)
        rejects(future)
    }
    @Test fun nullReadingsRemainNullRatherThanZero() {
        val raw=page()
        raw.getJSONArray("samples").getJSONArray(0).put(2,JSONObject.NULL)
        assertNull(parseHistoryPage(raw,device,0).samples.first().chamber)
    }
    @Test fun emptyAndCaughtUpPagesAreValid() {
        assertTrue(parseHistoryPage(page(last=0),device,0).samples.isEmpty())
        assertTrue(parseHistoryPage(page(last=32,after=32),device,32).samples.isEmpty())
    }
    @Test fun clockAnchorUsesRoundTripMidpoint() {
        assertEquals(995100L,historyAnchor(1000000,200,5))
        assertTrue(runCatching {historyAnchor(1000000,60001,5)}.isFailure)
    }
    @Test fun rebootRefetchesFromZeroRatherThanSkippingEarlySamples() = kotlinx.coroutines.runBlocking {
        val calls=mutableListOf<Long>()
        val batch=fetchHistoryBatch(device,HistoryCursor("fedcba9876543210",20)) { after ->
            calls+=after
            page(last=40,after=after) to 100000L
        }
        assertEquals(listOf(20L,0L),calls)
        assertEquals(1L,batch.page.samples.first().sequence)
    }
    @Test fun sameBootUsesExactlyOneRequest() = kotlinx.coroutines.runBlocking {
        var count=0
        val batch=fetchHistoryBatch(device,HistoryCursor("0123456789abcdef",32)) { after ->
            count++
            page(last=40,after=after) to 100000L
        }
        assertEquals(1,count);assertEquals(33L,batch.page.samples.first().sequence)
    }
    @Test fun wrongDeviceDoesNotTriggerFollowupOrIngestion() = kotlinx.coroutines.runBlocking {
        var count=0
        val result=runCatching {fetchHistoryBatch(device,HistoryCursor("old",32)) {
            count++
            page().put("device_id","FFFFFFFFFFFF") to 0L
        }}
        assertTrue(result.isFailure);assertEquals(1,count)
    }
}
