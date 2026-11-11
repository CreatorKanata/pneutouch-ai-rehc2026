package com.creatorkanata.pneutouchai.web

import android.util.Log
import android.webkit.WebResourceResponse
import java.io.ByteArrayInputStream
import java.io.IOException
import java.io.InputStream

/**
 * WebViewAssetLoader's PathHandler cannot see request headers, so the client applies `Range` here to the
 * full 200 response: 206 with the requested slice, 416 when unsatisfiable, unchanged otherwise.
 */
fun withRange(response: WebResourceResponse, rangeHeader: String?): WebResourceResponse {
    if (rangeHeader == null || response.statusCode != 200) return response
    val size = response.responseHeaders?.get("Content-Length")?.toLongOrNull() ?: return response
    return when (val range = parseRange(rangeHeader, size)) {
        ByteRange.Whole -> response
        ByteRange.Unsatisfiable -> {
            closeQuietly(response.data)
            emptyResponse(416, "Range Not Satisfiable", mapOf("Content-Range" to "bytes */$size"))
        }
        is ByteRange.Partial -> partial(response, range, size)
    }
}

private fun partial(response: WebResourceResponse, range: ByteRange.Partial, size: Long): WebResourceResponse {
    val data = response.data
    return try {
        skipFully(data, range.start)
        val headers = fileHeaders(range.length) + ("Content-Range" to range.contentRange(size))
        WebResourceResponse(response.mimeType, response.encoding, 206, "Partial Content", headers,
            LimitedInputStream(data, range.length))
    } catch (error: IOException) {
        Log.e("PneuTouchAssets", "range read failed", error)
        closeQuietly(data)
        emptyResponse(500, "Internal Server Error", emptyMap())
    }
}

fun emptyResponse(status: Int, reason: String, headers: Map<String, String>): WebResourceResponse =
    WebResourceResponse("text/plain", "utf-8", status, reason, headers + ("Content-Length" to "0"),
        ByteArrayInputStream(ByteArray(0)))

private fun skipFully(stream: InputStream, count: Long) {
    var remaining = count
    while (remaining > 0) {
        val skipped = stream.skip(remaining)
        if (skipped <= 0) {
            if (stream.read() < 0) throw IOException("asset ended before byte $count")
            remaining -= 1
        } else {
            remaining -= skipped
        }
    }
}

private fun closeQuietly(stream: InputStream?) {
    try {
        stream?.close()
    } catch (error: IOException) {
        Log.w("PneuTouchAssets", "close failed", error)
    }
}

/** Reads at most [limit] bytes from [source]. */
private class LimitedInputStream(private val source: InputStream, private var limit: Long) : InputStream() {
    override fun read(): Int {
        if (limit <= 0) return -1
        val value = source.read()
        if (value >= 0) limit -= 1
        return value
    }

    override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
        if (limit <= 0) return -1
        val count = source.read(buffer, offset, minOf(length.toLong(), limit).toInt())
        if (count > 0) limit -= count
        return count
    }

    override fun available(): Int = minOf(source.available().toLong(), limit).toInt()

    override fun close() = source.close()
}
