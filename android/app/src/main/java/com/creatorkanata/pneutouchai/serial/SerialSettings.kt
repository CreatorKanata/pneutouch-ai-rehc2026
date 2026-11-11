package com.creatorkanata.pneutouchai.serial

import com.hoho.android.usbserial.driver.UsbSerialPort

/** Port parameters in usb-serial-for-android's constants. */
data class SerialSettings(val baudRate: Int, val dataBits: Int, val stopBits: Int, val parity: Int)

private val PARITIES = mapOf(
    "none" to UsbSerialPort.PARITY_NONE,
    "even" to UsbSerialPort.PARITY_EVEN,
    "odd" to UsbSerialPort.PARITY_ODD,
)
private val STOP_BITS = mapOf(1 to UsbSerialPort.STOPBITS_1, 2 to UsbSerialPort.STOPBITS_2)

/** Web Serial `open()` options -> settings, or null when the board link cannot use them. */
fun serialSettingsOf(baudRate: Int, dataBits: Int, stopBits: Int, parity: String, flowControl: String): SerialSettings? {
    if (baudRate <= 0 || dataBits !in 7..8 || flowControl != "none") return null
    val stop = STOP_BITS[stopBits] ?: return null
    val parityValue = PARITIES[parity] ?: return null
    return SerialSettings(baudRate, dataBits, stop, parityValue)
}
