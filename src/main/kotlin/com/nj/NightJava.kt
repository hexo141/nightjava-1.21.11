package com.nj

import com.nj.light.LightNative
import com.nj.render.RenderNative
import net.fabricmc.api.ModInitializer
import org.slf4j.LoggerFactory

object NightJava : ModInitializer {
    private val logger = LoggerFactory.getLogger("nightjava")

    override fun onInitialize() {
        NativeLoader.loadNativeLibrary()
        if (NativeLoader.isLoaded) {
            RenderNative.init()
            LightNative.init()
            val renderOk = RenderNative.isAvailable()
            val lightOk = LightNative.isAvailable()
            val parts = mutableListOf<String>()
            if (renderOk) parts.add("render")
            if (lightOk) parts.add("light")
            if (parts.isNotEmpty()) {
                logger.info("NightJava native acceleration active: ${parts.joinToString(", ")}")
            } else {
                logger.info("NightJava mod initialized with native particle acceleration")
            }
        } else {
            logger.info("NightJava mod initialized (Java fallback mode)")
        }
    }
}
