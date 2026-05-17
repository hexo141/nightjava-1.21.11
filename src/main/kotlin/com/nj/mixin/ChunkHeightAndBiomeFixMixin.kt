package com.nj.mixin

import com.nj.datafixer.DataFixerNative
import net.minecraft.datafixer.fix.ChunkHeightAndBiomeFix
import org.spongepowered.asm.mixin.Mixin
import org.spongepowered.asm.mixin.injection.At
import org.spongepowered.asm.mixin.injection.Inject
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable

@Mixin(ChunkHeightAndBiomeFix::class)
abstract class ChunkHeightAndBiomeFixMixin {

    @Inject(method = ["makeRule"], at = [At("HEAD")], remap = false)
    private fun onMakeRuleHead(ci: CallbackInfoReturnable<*>) {
        if (DataFixerNative.isAvailable) {
            org.slf4j.LoggerFactory.getLogger("nightjava-datafixer").debug("ChunkHeightAndBiomeFix native acceleration available")
        }
    }
}
