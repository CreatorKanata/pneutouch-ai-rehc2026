import java.io.File

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// The web demo is not copied into git: it is synced from the repository at build time.
// URL path under https://appassets.androidplatform.net/ -> repository directory (same as tools/serve_demo.py).
val webDirectories = mapOf(
    "demo-visualizer" to "src/demo-visualizer",
    "shared" to "src/shared",
    "videos/demo" to "videos/demo",
    "images/demo" to "images/demo",
    "images/logo" to "images/logo",
)

/** Copies the web app and media into `<outputDir>/web/` and writes `web/config.json` from config.py. */
abstract class PrepareWebAssets : DefaultTask() {
    @get:Inject abstract val files: FileSystemOperations

    @get:Internal abstract val repoRoot: DirectoryProperty

    @get:Input abstract val directories: MapProperty<String, String>

    @get:InputFiles
    @get:PathSensitive(PathSensitivity.RELATIVE)
    val sources: FileTree
        get() = repoRoot.get().asFileTree.matching {
            directories.get().values.forEach { include("$it/**") }
            include("config.py")
        }

    @get:OutputDirectory abstract val outputDir: DirectoryProperty

    @TaskAction
    fun prepare() {
        val root = repoRoot.get().asFile
        val web = outputDir.get().asFile.resolve("web")
        files.delete { delete(outputDir) }
        directories.get().forEach { (target, source) ->
            val from = root.resolve(source)
            if (!from.isDirectory) throw GradleException("Web asset directory is missing: $from")
            files.copy { from(from); into(web.resolve(target)) }
        }
        web.resolve("config.json").writeText(demoConfigJson(root.resolve("config.py")))
    }

    // Same values as /config.json in tools/serve_demo.py.
    private fun demoConfigJson(configPy: File): String {
        if (!configPy.isFile) throw GradleException("config.py not found: $configPy")
        val text = configPy.readText()
        fun value(name: String): String =
            Regex("""^$name\s*=\s*([0-9_.]+)\s*(#.*)?$""", RegexOption.MULTILINE).find(text)?.groupValues?.get(1)
                ?.replace("_", "")
                ?: throw GradleException("Cannot read $name from $configPy (expected a plain number)")
        val baud = value("UART_BAUD").toIntOrNull()
            ?: throw GradleException("UART_BAUD in $configPy is not an integer")
        val noDataSeconds = value("NO_DATA_WARNING_S").toDoubleOrNull()
            ?: throw GradleException("NO_DATA_WARNING_S in $configPy is not a number")
        val noDataMs = noDataSeconds * 1000
        val noDataText = if (noDataMs % 1.0 == 0.0) noDataMs.toLong().toString() else noDataMs.toString()
        return """{"baudRate": $baud, "noDataMs": $noDataText}"""
    }
}

val prepareWebAssets = tasks.register<PrepareWebAssets>("prepareWebAssets") {
    repoRoot.set(rootProject.layout.projectDirectory.dir(".."))
    directories.set(webDirectories)
    outputDir.set(layout.buildDirectory.dir("generated/webAssets"))
}

android {
    namespace = "com.creatorkanata.pneutouchai"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.creatorkanata.pneutouchai"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    // Videos are streamed with HTTP Range from the APK: keep them stored, not deflated.
    androidResources {
        noCompress += listOf("mp4")
    }
}

kotlin {
    jvmToolchain(17)
}

androidComponents {
    onVariants { variant ->
        variant.sources.assets?.addGeneratedSourceDirectory(prepareWebAssets, PrepareWebAssets::outputDir)
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.16.0")
    implementation("androidx.webkit:webkit:1.14.0")
    implementation("com.github.mik3y:usb-serial-for-android:3.12.0")
    testImplementation("junit:junit:4.13.2")
}
