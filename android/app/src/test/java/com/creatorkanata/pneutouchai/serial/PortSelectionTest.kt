package com.creatorkanata.pneutouchai.serial

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class PortSelectionTest {
    @Test
    fun uartIsChannelBWhenTheDeviceHasTwoPorts() {
        assertEquals(1, portIndexFor(2))
        assertEquals(1, portIndexFor(4))
        assertEquals(0, portIndexFor(1))
        assertNull(portIndexFor(0))
    }

    @Test
    fun prefersTheFt2232hThenOtherFtdiAndIgnoresOthers() {
        val devices = listOf("cdc" to UsbId(0x2341, 0x0043), "ft232" to UsbId(0x0403, 0x6001),
            "ft2232h" to UsbId(0x0403, 0x6010))
        assertEquals("ft2232h", chooseDevice(devices) { it.second }?.first)
        assertEquals("ft232", chooseDevice(devices.take(2)) { it.second }?.first)
        assertNull(chooseDevice(devices.take(1)) { it.second })
        assertNull(chooseDevice(emptyList<UsbId>()) { it })
    }
}
