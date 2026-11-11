package com.creatorkanata.pneutouchai.serial

const val FTDI_VENDOR_ID = 0x0403
const val FT2232H_PRODUCT_ID = 0x6010

data class UsbId(val vendorId: Int, val productId: Int)

/** The board's FT2232H exposes two ports and channel B (index 1) is the UART. */
fun portIndexFor(portCount: Int): Int? = when {
    portCount >= 2 -> 1
    portCount == 1 -> 0
    else -> null
}

/** Prefers the FT2232H, then any other FTDI device; non-FTDI devices are not the board. */
fun <T> chooseDevice(devices: List<T>, idOf: (T) -> UsbId): T? {
    val ftdi = devices.filter { idOf(it).vendorId == FTDI_VENDOR_ID }
    return ftdi.firstOrNull { idOf(it).productId == FT2232H_PRODUCT_ID } ?: ftdi.firstOrNull()
}
