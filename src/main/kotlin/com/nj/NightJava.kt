package com.nj

import com.nj.datafixer.DataFixerNative
import net.fabricmc.api.ModInitializer
import org.slf4j.LoggerFactory

object NightJava : ModInitializer {
    private val logger = LoggerFactory.getLogger("nightjava")

    override fun onInitialize() {
        NativeLoader.loadNativeLibrary()
        if (NativeLoader.isLoaded) {
            logger.info("NightJava mod initialized with native particle acceleration")
            if (DataFixerNative.isAvailable) {
                logger.info("DataFixer native acceleration enabled for faster world loading")
            }
        } else {
            logger.info("NightJava mod initialized (Java fallback mode)")
        }
    }
}
