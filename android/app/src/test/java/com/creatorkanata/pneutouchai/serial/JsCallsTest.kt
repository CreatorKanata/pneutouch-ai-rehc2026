package com.creatorkanata.pneutouchai.serial

import org.junit.Assert.assertEquals
import org.junit.Test

class JsCallsTest {
    @Test
    fun quotesStringsSafelyForEvaluateJavascript() {
        assertEquals("\"a\\\"b\\\\c\\n\\r\\u2028\\u2029\\u003c/script>\"", jsString("a\"b\\c\n\r\u2028\u2029</script>"))
        assertEquals("\"USBケーブル\"", jsString("USBケーブル"))
        assertEquals("\"\\u0001\"", jsString("\u0001"))
    }

    @Test
    fun buildsReceiverCalls() {
        assertEquals("window.__pneuTouchSerial&&window.__pneuTouchSerial.settle(3,null)", settleScript(3, null))
        assertEquals("window.__pneuTouchSerial&&window.__pneuTouchSerial.settle(4,\"x\")", settleScript(4, "x"))
        assertEquals("window.__pneuTouchSerial&&window.__pneuTouchSerial.data(\"QUI=\")", dataScript("QUI="))
        assertEquals("window.__pneuTouchSerial&&window.__pneuTouchSerial.lost(\"y\")", lostScript("y"))
    }
}
