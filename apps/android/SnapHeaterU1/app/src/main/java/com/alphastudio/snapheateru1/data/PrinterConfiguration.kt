package com.alphastudio.snapheateru1.data

import org.json.JSONObject
import java.util.UUID
import com.alphastudio.snapheateru1.model.HeaterSnapshot

fun printerConfiguration(host: String, port: Int, apiKey: String, id: String=UUID.randomUUID().toString().replace("-","")): JSONObject {
    require(host.length in 1..63 && host.matches(Regex("[A-Za-z0-9.-]+")))
    require(port in 1..65535)
    require(apiKey.length<=128 && apiKey.matches(Regex("[A-Za-z0-9_-]*")))
    require(id.matches(Regex("[a-fA-F0-9]{32}")))
    return JSONObject().put("printer_setup",JSONObject().put("host",host).put("port",port).put("api_key",apiKey).put("id",id))
}

class PrinterSetupException(val reason: String): IllegalStateException(reason)
fun awaitPrinterSetup(id: String, initial: HeaterSnapshot, read: () -> HeaterSnapshot,
    clock: () -> Long = {System.nanoTime()/1_000_000}, pause: () -> Unit = {Thread.sleep(500)}): HeaterSnapshot {
    val deadline=clock()+45_000
    var current=initial
    while(true) {
        if(current.printerSetupId!=id) throw PrinterSetupException("unsupported_or_replaced")
        when(current.printerSetupPhase) {
            "succeeded" -> return current
            "testing","connecting","saving" -> Unit
            else -> throw PrinterSetupException(current.printerSetupPhase)
        }
        if(clock()>=deadline) throw PrinterSetupException("timeout")
        pause()
        current=read() // No write retry; Panda continues if this phone disconnects.
    }
}
