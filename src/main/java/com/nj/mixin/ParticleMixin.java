package com.nj.mixin;

import net.minecraft.client.particle.Particle;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;

@Mixin(Particle.class)
public abstract class ParticleMixin {

    @Shadow(remap = false)
    protected double lastX;
    @Shadow(remap = false)
    protected double lastY;
    @Shadow(remap = false)
    protected double lastZ;
    @Shadow(remap = false)
    protected double x;
    @Shadow(remap = false)
    protected double y;
    @Shadow(remap = false)
    protected double z;
    @Shadow(remap = false)
    protected double velocityX;
    @Shadow(remap = false)
    protected double velocityY;
    @Shadow(remap = false)
    protected double velocityZ;
    @Shadow(remap = false)
    protected int age;
    @Shadow(remap = false)
    protected int maxAge;
    @Shadow(remap = false)
    protected float gravityStrength;
    @Shadow(remap = false)
    protected boolean dead;
}