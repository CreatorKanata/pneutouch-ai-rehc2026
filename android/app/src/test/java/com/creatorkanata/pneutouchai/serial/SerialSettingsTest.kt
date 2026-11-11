package com.creatorkanata.pneutouchai.serial

import com.hoho.android.usbserial.driver.UsbSerialPort
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SerialSettingsTest {
    @Test
    fun mapsWebSerialOptionsToPortParameters() {
        assertEquals(SerialSettings(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE),
            serialSettingsOf(115200, 8, 1, "none", "none"))
        assertEquals(SerialSettings(9600, 7, UsbSerialPort.STOPBITS_2, UsbSerialPort.PARITY_EVEN),
            serialSettingsOf(9600, 7, 2, "even", "none"))
        assertEquals(UsbSerialPort.PARITY_ODD, serialSettingsOf(9600, 8, 1, "odd", "none")?.parity)
    }

    @Test
    fun rejectsUnsupportedOptions() {
        assertNull(serialSettingsOf(0, 8, 1, "none", "none"))
        assertNull(serialSettingsOf(115200, 6, 1, "none", "none"))
        assertNull(serialSettingsOf(115200, 8, 3, "none", "none"))
        assertNull(serialSettingsOf(115200, 8, 1, "mark", "none"))
        assertNull(serialSettingsOf(115200, 8, 1, "none", "hardware"))
    }
}
