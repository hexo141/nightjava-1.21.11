package com.nj

import org.slf4j.LoggerFactory
import java.nio.file.Files

object NativeLoader {
    private val logger = LoggerFactory.getLogger("nightjava-native")

    @JvmField
    var isLoaded = false

    fun loadNativeLibrary() {
        if (isLoaded) {
            logger.info("Native library already loaded")
            return
        }

        try {
            val os = System.getProperty("os.name").lowercase()
            val arch = System.getProperty("os.arch").lowercase()
            val archDir = when {
                arch.contains("amd64") || arch.contains("x86_64") || arch.contains("x64") -> "x64"
                arch.contains("aarch64") || arch.contains("arm64") -> "arm64"
                else -> arch
            }

            val platformDir = when {
                os.contains("win") -> "natives/windows/$archDir"
                os.contains("linux") -> "natives/linux/$archDir"
                os.contains("mac") -> "natives/macos/$archDir"
                else -> {
                    logger.warn("Unsupported OS: $os, falling back to Java particle calculations")
                    return
                }
            }

            val libName = if (os.contains("win")) "nightjava-client.dll"
            else if (os.contains("mac")) "libnightjava-client.dylib"
            else "libnightjava-client.so"

            val resourcePath = "/$platformDir/$libName"
            val inputStream = NativeLoader::class.java.getResourceAsStream(resourcePath)
                ?: throw IllegalStateException("Native library not found: $resourcePath")

            val tempDir = Files.createTempDirectory("nightjava-natives")
            tempDir.toFile().deleteOnExit()

            val outputFile = tempDir.resolve(libName).toFile()
            outputFile.deleteOnExit()

            inputStream.use { input ->
                outputFile.outputStream().use { output ->
                    input.copyTo(output)
                }
            }

            System.load(outputFile.absolutePath)
            isLoaded = true
            logger.info("Native particle library loaded successfully from: ${outputFile.absolutePath}")
        } catch (e: Exception) {
            isLoaded = false
            logger.warn("Failed to load native particle library, falling back to Java calculations: ${e.message}")
        }
    }
}
