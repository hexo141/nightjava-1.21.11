package com.nj.mixin;

import net.minecraft.client.particle.ParticleManager;
import net.minecraft.particle.ParticleGroup;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Invoker;

@Mixin(ParticleManager.class)
public interface ParticleManagerAccessor {

    @Invoker("addTo")
    void invokeAddTo(ParticleGroup group, int count);
}