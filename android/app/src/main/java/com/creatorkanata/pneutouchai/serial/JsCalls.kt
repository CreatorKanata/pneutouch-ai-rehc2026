package com.creatorkanata.pneutouchai.serial

// Scripts for evaluateJavascript that call the shim's receiver (assets/web-serial-shim.js).
private const val RECEIVER = "window.__pneuTouchSerial"

/** A JavaScript string literal; safe for any text, including line separators and `</script>`. */
fun jsString(text: String): String = buildString {
    append('"')
    for (c in text) {
        when {
            c == '"' -> append("\\\"")
            c == '\\' -> append("\\\\")
            c == '\n' -> append("\\n")
            c == '\r' -> append("\\r")
            c == '<' -> append("\\u003c")
            c < ' ' || c == ' ' || c == ' ' -> append("\\u%04x".format(c.code))
            else -> append(c)
        }
    }
    append('"')
}

fun settleScript(id: Int, error: String?): String = "$RECEIVER&&$RECEIVER.settle($id,${error?.let(::jsString) ?: "null"})"

fun dataScript(base64: String): String = "$RECEIVER&&$RECEIVER.data(${jsString(base64)})"

fun lostScript(message: String): String = "$RECEIVER&&$RECEIVER.lost(${jsString(message)})"
