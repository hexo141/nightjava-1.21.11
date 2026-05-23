package com.nj

import net.fabricmc.api.ModInitializer
import org.slf4j.LoggerFactory

object NightJava : ModInitializer {
    private val logger = LoggerFactory.getLogger("nightjava")

    override fun onInitialize() {
        NativeLoader.loadNativeLibrary()
        if (NativeLoader.isLoaded) {
            logger.info("NightJava mod initialized with native particle acceleration")
        } else {
            logger.info("NightJava mod initialized (Java fallback mode)")
        }
    }
}
