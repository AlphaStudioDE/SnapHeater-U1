package com.alphastudio.snapheateru1.data

import com.alphastudio.snapheateru1.model.HeaterSnapshot
import com.alphastudio.snapheateru1.model.AppMode
import org.json.JSONObject

internal fun clearFaultAndVerify(
    read: () -> HeaterSnapshot,
    send: (JSONObject) -> HeaterSnapshot,
    pause: () -> Unit = { Thread.sleep(500) },
): HeaterSnapshot {
    val before=read() // Refresh revision; never replay a stale clear automatically.
    check(before.faultClearSupported && !before.faultInhibited)
    check(before.deviceId.isNotBlank())
    if(!before.faultLatched) {
        check(before.mode==AppMode.SafeStop)
        return before
    }
    val deadline=System.nanoTime()+30_000_000_000L
    var current=send(JSONObject().put("clear_heater_fault",true))
    repeat(10) {
        check(current.deviceId.equals(before.deviceId,true))
        check(current.faultClearSupported && !current.faultInhibited)
        if (!current.faultLatched) {
            check(current.mode==AppMode.SafeStop)
            return current
        }
        check(System.nanoTime()<deadline) { "Fault clear confirmation timed out" }
        pause()
        current=read()
    }
    error("Fault clear not confirmed: ${current.faultReason}")
}
