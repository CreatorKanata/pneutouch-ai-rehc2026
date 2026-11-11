package com.creatorkanata.pneutouchai.serial

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.Base64
import android.util.Log
import android.webkit.JavascriptInterface
import android.webkit.WebView
import org.json.JSONException
import org.json.JSONObject

/**
 * `window.PneuTouchSerial` for assets/web-serial-shim.js. JS calls carry an id; the result comes back
 * through `window.__pneuTouchSerial.settle(id, error)`. Received bytes go to `data(base64)`.
 */
class SerialBridge(context: Context, private val webView: WebView) : SerialEvents {
    private val main = Handler(Looper.getMainLooper())
    private val controller = UsbSerialController(context, this)

    @JavascriptInterface
    fun requestPort(id: Int) {
        main.post { controller.requestPort { error -> settle(id, error) } }
    }

    @JavascriptInterface
    fun open(id: Int, optionsJson: String) {
        val settings = parseSettings(optionsJson)
            ?: return settle(id, "この通信設定は使えません（$optionsJson）")
        controller.open(settings) { error -> settle(id, error) }
    }

    @JavascriptInterface
    fun write(id: Int, base64: String) {
        val bytes = try {
            Base64.decode(base64, Base64.NO_WRAP)
        } catch (error: IllegalArgumentException) {
            return settle(id, "送信データが不正です")
        }
        controller.write(bytes) { error -> settle(id, error) }
    }

    @JavascriptInterface
    fun close(id: Int) {
        controller.close { error -> settle(id, error) }
    }

    /** A new page has no port object: drop whatever the previous page left open. */
    fun reset() = controller.close { }

    fun shutdown() = controller.shutdown()

    override fun onData(bytes: ByteArray) = run(dataScript(Base64.encodeToString(bytes, Base64.NO_WRAP)))

    override fun onLost(message: String) = run(lostScript(message))

    private fun settle(id: Int, error: String?) = run(settleScript(id, error))

    private fun run(script: String) {
        main.post { webView.evaluateJavascript(script, null) }
    }

    private fun parseSettings(json: String): SerialSettings? = try {
        val options = JSONObject(json)
        serialSettingsOf(options.getInt("baudRate"), options.optInt("dataBits", 8), options.optInt("stopBits", 1),
            options.optString("parity", "none"), options.optString("flowControl", "none"))
    } catch (error: JSONException) {
        Log.w("PneuTouchSerial", "bad open options", error)
        null
    }
}
