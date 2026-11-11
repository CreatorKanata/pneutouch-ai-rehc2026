package com.creatorkanata.pneutouchai.web

import android.annotation.SuppressLint
import android.content.pm.ApplicationInfo
import android.util.Log
import android.webkit.WebView
import androidx.webkit.WebViewCompat
import androidx.webkit.WebViewFeature

const val SHIM_ASSET = "web-serial-shim.js"
private const val TAG = "PneuTouchWeb"

/** JS and DOM storage on; no file/content access; videos may autoplay. */
@SuppressLint("SetJavaScriptEnabled")
fun configureWebView(webView: WebView) {
    webView.settings.apply {
        javaScriptEnabled = true
        domStorageEnabled = true
        mediaPlaybackRequiresUserGesture = false
        allowFileAccess = false
        allowContentAccess = false
        javaScriptCanOpenWindowsAutomatically = false
        setSupportMultipleWindows(false)
        setSupportZoom(false)
    }
    val debuggable = webView.context.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE != 0
    WebView.setWebContentsDebuggingEnabled(debuggable)
}

/**
 * Runs the Web Serial shim before the page's scripts, only on the app origin. Returns false when this
 * WebView lacks document-start scripts; the caller then injects it on page start instead.
 */
fun installShim(webView: WebView, script: String): Boolean {
    if (!WebViewFeature.isFeatureSupported(WebViewFeature.DOCUMENT_START_SCRIPT)) {
        Log.w(TAG, "DOCUMENT_START_SCRIPT unsupported; injecting the shim on page start")
        return false
    }
    WebViewCompat.addDocumentStartJavaScript(webView, script, setOf("https://$APP_HOST"))
    return true
}
