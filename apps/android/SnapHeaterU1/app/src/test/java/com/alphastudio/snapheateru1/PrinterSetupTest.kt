package com.alphastudio.snapheateru1
import com.alphastudio.snapheateru1.data.*
import com.alphastudio.snapheateru1.model.HeaterSnapshot
import org.junit.Assert.*
import org.junit.Test

class PrinterSetupTest {
    private val id="0123456789abcdef0123456789abcdef"
    @Test fun payloadIsDedicatedAndRejectsHeaderInjection() {
        val p=printerConfiguration("printer.local",7125,"test-key",id)
        assertEquals(1,p.length());assertEquals("test-key",p.getJSONObject("printer_setup").getString("api_key"))
        assertTrue(runCatching {printerConfiguration("printer.local",7125,"x\r\nOther: a",id)}.isFailure)
        assertTrue(runCatching {printerConfiguration("http://printer",7125,"",id)}.isFailure)
        assertTrue(runCatching {printerConfiguration("printer",0,"",id)}.isFailure)
    }
    @Test fun onlyMatchingSuccessfulOperationCompletes() {
        val initial=HeaterSnapshot(printerSetupId=id,printerSetupPhase="testing")
        var reads=0
        val result=awaitPrinterSetup(id,initial,{reads++;initial.copy(printerSetupPhase="succeeded")},{0},{})
        assertEquals(1,reads);assertEquals("succeeded",result.printerSetupPhase)
        val error=runCatching {awaitPrinterSetup(id,initial,{initial.copy(printerSetupPhase="auth_failed")},{0},{})}.exceptionOrNull()
        assertEquals("auth_failed",(error as PrinterSetupException).reason)
        assertTrue(runCatching {awaitPrinterSetup("different",initial,{error("must not read")},{0},{})}.isFailure)
    }
    @Test fun waitIsBoundedWithoutResendingCommands() {
        val initial=HeaterSnapshot(printerSetupId=id,printerSetupPhase="testing")
        var now=0L;var reads=0
        val error=runCatching {awaitPrinterSetup(id,initial,{reads++;initial},{now},{now+=500})}.exceptionOrNull()
        assertEquals("timeout",(error as PrinterSetupException).reason);assertEquals(90,reads)
    }
}
