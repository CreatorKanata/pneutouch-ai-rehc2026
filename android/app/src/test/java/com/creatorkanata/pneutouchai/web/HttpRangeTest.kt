package com.creatorkanata.pneutouchai.web

import org.junit.Assert.assertEquals
import org.junit.Test

// Same cases as tools/serve_demo.py's Range handling.
class HttpRangeTest {
    private val size = 1000L

    @Test
    fun noHeaderMeansWholeFile() {
        assertEquals(ByteRange.Whole, parseRange(null, size))
        assertEquals(ByteRange.Whole, parseRange("  ", size))
    }

    @Test
    fun openEndedAndBoundedRanges() {
        assertEquals(ByteRange.Partial(0, 999), parseRange("bytes=0-", size))
        assertEquals(ByteRange.Partial(100, 199), parseRange("bytes=100-199", size))
        assertEquals(ByteRange.Partial(900, 999), parseRange("bytes=900-5000", size))
        assertEquals(ByteRange.Partial(999, 999), parseRange("bytes=999-999", size))
    }

    @Test
    fun suffixRanges() {
        assertEquals(ByteRange.Partial(800, 999), parseRange("bytes=-200", size))
        assertEquals(ByteRange.Partial(0, 999), parseRange("bytes=-5000", size))
    }

    @Test
    fun unsatisfiableRanges() {
        for (header in listOf("bytes=1000-", "bytes=200-100", "bytes=-0", "bytes=-", "items=0-1",
            "bytes=0-1,5-6", "bytes=abc-", "bytes=99999999999999999999-")) {
            assertEquals(header, ByteRange.Unsatisfiable, parseRange(header, size))
        }
        assertEquals(ByteRange.Unsatisfiable, parseRange("bytes=0-", 0))
    }

    @Test
    fun partialLengthAndContentRange() {
        val range = ByteRange.Partial(100, 199)
        assertEquals(100L, range.length)
        assertEquals("bytes 100-199/1000", range.contentRange(size))
    }
}
