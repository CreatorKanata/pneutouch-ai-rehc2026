package com.creatorkanata.pneutouchai.serial

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import androidx.core.content.ContextCompat

/** Asks the user for access to one USB device; [done] runs on the main thread with the result. */
class UsbPermission(private val context: Context, private val usbManager: UsbManager) {
    private val action = "${context.packageName}.USB_PERMISSION"

    fun request(device: UsbDevice, done: (Boolean) -> Unit) {
        if (usbManager.hasPermission(device)) return done(true)
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(receiverContext: Context, intent: Intent) {
                context.unregisterReceiver(this)
                done(usbManager.hasPermission(device))
            }
        }
        ContextCompat.registerReceiver(context, receiver, IntentFilter(action), ContextCompat.RECEIVER_NOT_EXPORTED)
        // The system fills in the device extras, so the PendingIntent must be mutable (and explicit).
        val mutable = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) PendingIntent.FLAG_MUTABLE else 0
        val intent = Intent(action).setPackage(context.packageName)
        val pending = PendingIntent.getBroadcast(context, 0, intent, mutable or PendingIntent.FLAG_UPDATE_CURRENT)
        try {
            usbManager.requestPermission(device, pending)
        } catch (error: RuntimeException) {
            context.unregisterReceiver(receiver)
            done(false)
        }
    }
}
