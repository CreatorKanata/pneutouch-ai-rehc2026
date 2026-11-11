package com.creatorkanata.pneutouchai.web

/**
 * Maps the path after a URL prefix to an APK asset path under [baseDir]; `""` or `dir/` means `index.html`.
 * Returns null for traversal (`.`/`..`), empty segments, backslashes or NUL, as tools/serve_demo.py does.
 */
fun assetPathFor(baseDir: String, relative: String): String? {
    if ('\\' in relative || '\u0000' in relative) return null
    val path = if (relative.isEmpty() || relative.endsWith("/")) relative + "index.html" else relative
    val segments = path.split('/')
    if (segments.any { it.isEmpty() || it == "." || it == ".." }) return null
    return "$baseDir/$path"
}

private val MIME_TYPES = mapOf(
    "html" to "text/html",
    "css" to "text/css",
    "js" to "text/javascript",
    "mjs" to "text/javascript",
    "json" to "application/json",
    "mp4" to "video/mp4",
    "jpg" to "image/jpeg",
    "jpeg" to "image/jpeg",
    "png" to "image/png",
    "svg" to "image/svg+xml",
)

/** Module scripts need a JavaScript MIME type, so `.mjs` is mapped explicitly. */
fun mimeTypeFor(path: String): String {
    val name = path.substringAfterLast('/')
    if ('.' !in name) return "application/octet-stream"
    return MIME_TYPES[name.substringAfterLast('.').lowercase()] ?: "application/octet-stream"
}
