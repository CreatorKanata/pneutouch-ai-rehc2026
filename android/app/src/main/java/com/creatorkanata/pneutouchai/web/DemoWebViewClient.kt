package com.creatorkanata.pneutouchai.web

import android.graphics.Bitmap
import android.net.Uri
import android.util.Log
import android.webkit.RenderProcessGoneDetail
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.WebViewAssetLoader

/**
 * Serves the demo from the APK on https://appassets.androidplatform.net/ and keeps the WebView on that
 * origin: other navigations are refused and other-origin requests get 403 (the demo needs none).
 */
class DemoWebViewClient(
    private val loader: WebViewAssetLoader,
    private val onPageStarted: (WebView) -> Unit,
    private val onRendererGone: () -> Unit,
) : WebViewClient() {
    override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse {
        if (!isAppOrigin(request.url)) return emptyResponse(403, "Forbidden", emptyMap())
        val response = loader.shouldInterceptRequest(request.url)
            ?: return emptyResponse(404, "Not Found", emptyMap())
        val range = request.requestHeaders.entries.firstOrNull { it.key.equals("Range", ignoreCase = true) }?.value
        return withRange(response, range)
    }

    override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
        val outside = !isAppOrigin(request.url)
        if (outside) Log.w(TAG, "blocked navigation to ${request.url}")
        return outside
    }

    override fun onPageStarted(view: WebView, url: String?, favicon: Bitmap?) = onPageStarted(view)

    override fun onRenderProcessGone(view: WebView, detail: RenderProcessGoneDetail): Boolean {
        Log.e(TAG, "WebView renderer gone (crash=${detail.didCrash()})")
        onRendererGone()
        return true
    }

    private companion object {
        const val TAG = "PneuTouchWeb"

        fun isAppOrigin(url: Uri): Boolean = url.scheme == "https" && url.host == APP_HOST && url.port == -1
    }
}
