package com.nj.mixin

import com.nj.datafixer.DataFixerNative
import net.minecraft.datafixer.fix.ChunkPalettedStorageFix
import org.spongepowered.asm.mixin.Mixin
import org.spongepowered.asm.mixin.injection.At
import org.spongepowered.asm.mixin.injection.Inject
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable

@Mixin(ChunkPalettedStorageFix::class)
abstract class ChunkPalettedStorageFixMixin {

    @Inject(method = ["makeRule"], at = [At("HEAD")], remap = false)
    private fun onMakeRuleHead(ci: CallbackInfoReturnable<*>) {
        if (DataFixerNative.isAvailable) {
            org.slf4j.LoggerFactory.getLogger("nightjava-datafixer").debug("ChunkPalettedStorageFix native acceleration available")
        }
    }
}
