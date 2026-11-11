package com.creatorkanata.pneutouchai

import android.app.Activity
import android.os.Build
import android.os.Bundle
import android.view.WindowManager
import android.webkit.WebView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.creatorkanata.pneutouchai.serial.SerialBridge
import com.creatorkanata.pneutouchai.web.DemoWebViewClient
import com.creatorkanata.pneutouchai.web.SHIM_ASSET
import com.creatorkanata.pneutouchai.web.START_URL
import com.creatorkanata.pneutouchai.web.buildAssetLoader
import com.creatorkanata.pneutouchai.web.configureWebView
import com.creatorkanata.pneutouchai.web.installShim

/**
 * Full-screen WebView showing src/demo-visualizer. Rotation and other config changes are handled in the
 * manifest (configChanges), so the page is never reloaded by them.
 */
class MainActivity : Activity() {
    private lateinit var webView: WebView
    private lateinit var bridge: SerialBridge

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes = WindowManager.LayoutParams().apply {
                copyFrom(window.attributes)
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
        webView = WebView(this)
        setContentView(webView)
        hideSystemBars()
        configureWebView(webView)
        bridge = SerialBridge(this, webView)
        webView.addJavascriptInterface(bridge, "PneuTouchSerial")
        val shim = assets.open(SHIM_ASSET).bufferedReader().use { it.readText() }
        val injectOnStart = !installShim(webView, shim)
        webView.webViewClient = DemoWebViewClient(
            buildAssetLoader(assets),
            onPageStarted = { view ->
                bridge.reset()
                if (injectOnStart) view.evaluateJavascript(shim, null)
            },
            onRendererGone = { recreate() },
        )
        webView.loadUrl(START_URL)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    override fun onResume() {
        super.onResume()
        webView.onResume()
    }

    override fun onPause() {
        webView.onPause()
        super.onPause()
    }

    override fun onDestroy() {
        bridge.shutdown()
        webView.destroy()
        super.onDestroy()
    }

    private fun hideSystemBars() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsetsCompat.Type.systemBars())
        }
    }
}
