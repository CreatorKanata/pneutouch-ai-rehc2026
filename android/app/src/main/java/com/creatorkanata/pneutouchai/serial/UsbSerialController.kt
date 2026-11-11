package com.creatorkanata.pneutouchai.serial

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import android.util.Log
import androidx.core.content.ContextCompat
import com.hoho.android.usbserial.driver.UsbSerialDriver
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import com.hoho.android.usbserial.util.SerialInputOutputManager
import java.io.IOException
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

/** Receives what the board sends, and hears when the link is gone (unplug or I/O error). */
interface SerialEvents {
    fun onData(bytes: ByteArray)

    fun onLost(message: String)
}

/**
 * One USB serial link to the board. [requestPort] runs on the main thread; open/write/close run in order
 * on one background thread. Every `done` gets null on success or a Japanese message for the web app.
 */
class UsbSerialController(private val context: Context, private val events: SerialEvents) {
    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager
    private val permission = UsbPermission(context, usbManager)
    private val worker: ExecutorService = Executors.newSingleThreadExecutor()

    @Volatile private var driver: UsbSerialDriver? = null
    @Volatile private var link: OpenLink? = null

    private class OpenLink(val port: UsbSerialPort, val io: SerialInputOutputManager)

    private val detachReceiver = object : BroadcastReceiver() {
        override fun onReceive(receiverContext: Context, intent: Intent) {
            val device = detachedDevice(intent) ?: return
            if (device.deviceName == driver?.device?.deviceName) worker.execute { lose(link, "USBケーブルが外れました") }
        }
    }

    init {
        ContextCompat.registerReceiver(context, detachReceiver, IntentFilter(UsbManager.ACTION_USB_DEVICE_DETACHED),
            ContextCompat.RECEIVER_NOT_EXPORTED)
    }

    fun requestPort(done: (String?) -> Unit) {
        val drivers = try {
            UsbSerialProber.getDefaultProber().findAllDrivers(usbManager)
        } catch (error: RuntimeException) {
            Log.e(TAG, "USB probe failed", error)
            return done("USB機器を調べられませんでした（${error.message}）")
        }
        val found = chooseDevice(drivers) { UsbId(it.device.vendorId, it.device.productId) }
            ?: return done("ボードが見つかりません。USBケーブルでボード（CN9）をつないでください。")
        permission.request(found.device) { granted ->
            if (granted) driver = found
            done(if (granted) null else "USBの使用が許可されませんでした。もう一度つなぐボタンを押して許可してください。")
        }
    }

    fun open(settings: SerialSettings, done: (String?) -> Unit) = worker.execute {
        closeLink()
        done(
            try {
                link = openLink(settings)
                null
            } catch (error: IOException) {
                Log.e(TAG, "open failed", error)
                "ポートを開けませんでした（${error.message}）"
            } catch (error: RuntimeException) {
                Log.e(TAG, "open failed", error)
                "ポートを開けませんでした（${error.message}）"
            },
        )
    }

    fun write(bytes: ByteArray, done: (String?) -> Unit) = worker.execute {
        val port = link?.port ?: return@execute done("接続していません")
        done(
            try {
                port.write(bytes, WRITE_TIMEOUT_MS)
                null
            } catch (error: IOException) {
                Log.e(TAG, "write failed", error)
                "送信できませんでした（${error.message}）"
            },
        )
    }

    fun close(done: (String?) -> Unit) = worker.execute {
        closeLink()
        done(null)
    }

    /** Activity teardown: close the port and stop listening for detach. */
    fun shutdown() {
        try {
            context.unregisterReceiver(detachReceiver)
        } catch (error: IllegalArgumentException) {
            Log.w(TAG, "detach receiver was not registered", error)
        }
        worker.execute { closeLink() }
        worker.shutdown()
    }

    private fun openLink(settings: SerialSettings): OpenLink {
        val selected = driver ?: throw IOException("ボードが選ばれていません")
        val index = portIndexFor(selected.ports.size) ?: throw IOException("シリアルポートがありません")
        val connection = usbManager.openDevice(selected.device) ?: throw IOException("USB機器を開けません（許可がありません）")
        val port = selected.ports[index]
        try {
            port.open(connection)
            port.setParameters(settings.baudRate, settings.dataBits, settings.stopBits, settings.parity)
        } catch (error: IOException) {
            closeQuietly(port)
            throw error
        }
        val io = SerialInputOutputManager(port)
        val opened = OpenLink(port, io)
        io.listener = listenerFor(opened)
        io.start()
        return opened
    }

    private fun listenerFor(owner: OpenLink) = object : SerialInputOutputManager.Listener {
        override fun onNewData(data: ByteArray) {
            if (link === owner) events.onData(data)
        }

        override fun onRunError(error: Exception) {
            Log.w(TAG, "serial read stopped", error)
            worker.execute { lose(owner, "受信が止まりました（${error.message ?: "USBエラー"}）") }
        }
    }

    // Only the link that is still current reports a loss; a stale error after close is ignored.
    private fun lose(owner: OpenLink?, message: String) {
        if (owner == null || link !== owner) return
        closeLink()
        events.onLost(message)
    }

    private fun closeLink() {
        val current = link ?: return
        link = null
        current.io.stop()
        closeQuietly(current.port)
    }

    private fun closeQuietly(port: UsbSerialPort) {
        try {
            if (port.isOpen) port.close()
        } catch (error: IOException) {
            Log.w(TAG, "close failed", error)
        }
    }

    private companion object {
        const val TAG = "PneuTouchSerial"
        const val WRITE_TIMEOUT_MS = 1000

        fun detachedDevice(intent: Intent): UsbDevice? =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
            } else {
                @Suppress("DEPRECATION")
                intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
            }
    }
}
