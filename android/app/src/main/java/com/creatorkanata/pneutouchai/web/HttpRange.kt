package com.creatorkanata.pneutouchai.web

/** A single-range `Range: bytes=` request, resolved against the file size (as tools/serve_demo.py). */
sealed interface ByteRange {
    data object Whole : ByteRange

    data object Unsatisfiable : ByteRange

    data class Partial(val start: Long, val end: Long) : ByteRange {
        val length: Long get() = end - start + 1

        fun contentRange(size: Long): String = "bytes $start-$end/$size"
    }
}

private val RANGE = Regex("""bytes=(\d*)-(\d*)""")

fun parseRange(header: String?, size: Long): ByteRange {
    if (header.isNullOrBlank()) return ByteRange.Whole
    val match = RANGE.matchEntire(header.trim()) ?: return ByteRange.Unsatisfiable
    val (left, right) = match.destructured
    if (left.isEmpty() && right.isEmpty()) return ByteRange.Unsatisfiable
    val range = if (left.isNotEmpty()) {
        val start = left.toLongOrNull() ?: return ByteRange.Unsatisfiable
        val end = if (right.isEmpty()) size - 1 else minOf(size - 1, right.toLongOrNull() ?: Long.MAX_VALUE)
        ByteRange.Partial(start, end)
    } else {
        val count = right.toLongOrNull() ?: return ByteRange.Unsatisfiable
        if (count == 0L) return ByteRange.Unsatisfiable
        ByteRange.Partial(maxOf(0, size - count), size - 1)
    }
    return if (range.start >= size || range.end < range.start) ByteRange.Unsatisfiable else range
}
