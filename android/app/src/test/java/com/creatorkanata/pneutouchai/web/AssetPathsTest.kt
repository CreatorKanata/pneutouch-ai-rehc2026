package com.creatorkanata.pneutouchai.web

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AssetPathsTest {
    @Test
    fun resolvesFilesAndDirectoryIndex() {
        assertEquals("web/demo-visualizer/index.html", assetPathFor("web/demo-visualizer", ""))
        assertEquals("web/demo-visualizer/app.mjs", assetPathFor("web/demo-visualizer", "app.mjs"))
        assertEquals("web/videos/demo/static.mp4", assetPathFor("web/videos/demo", "static.mp4"))
        assertEquals("web/a/b/index.html", assetPathFor("web/a", "b/"))
    }

    @Test
    fun rejectsTraversalAndOddPaths() {
        for (path in listOf("../config.json", "a/../../x", "./app.mjs", "a\\b", "a\u0000b", "a//b", "/abs")) {
            assertNull(path, assetPathFor("web/demo-visualizer", path))
        }
    }

    @Test
    fun mimeTypesMatchTheServer() {
        assertEquals("text/javascript", mimeTypeFor("web/shared/serial.mjs"))
        assertEquals("text/html", mimeTypeFor("web/demo-visualizer/index.html"))
        assertEquals("text/css", mimeTypeFor("x/style.css"))
        assertEquals("application/json", mimeTypeFor("web/config.json"))
        assertEquals("video/mp4", mimeTypeFor("web/videos/demo/STATIC.MP4"))
        assertEquals("image/jpeg", mimeTypeFor("a.jpg"))
        assertEquals("image/png", mimeTypeFor("a.png"))
        assertEquals("application/octet-stream", mimeTypeFor("a.bin"))
        assertEquals("application/octet-stream", mimeTypeFor("noextension"))
    }
}
