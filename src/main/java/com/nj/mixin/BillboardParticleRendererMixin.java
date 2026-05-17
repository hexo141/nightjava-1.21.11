package com.nj.mixin;

import net.minecraft.client.particle.BillboardParticleRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

@Mixin(BillboardParticleRenderer.class)
public abstract class BillboardParticleRendererMixin {

    @Inject(method = "render", at = @At("HEAD"))
    private void onRenderHead(CallbackInfoReturnable<?> cir) {
    }
}
