package com.nj.mixin;

import com.nj.NativeLoader;
import com.nj.particle.ParticleNativeLib;
import net.minecraft.client.particle.Particle;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

@Mixin(Particle.class)
public abstract class ParticleMixin {

    private static final Logger LOGGER = LoggerFactory.getLogger("nightjava-particle-mixin");
    private static final int STRIDE = 12;

    private static FloatBuffer nativeBuffer;
    private static int bufferCapacity = 0;
    private static int tickCount = 0;
    private static long lastLogTime = 0;

    private static synchronized void ensureBufferCapacity() {
        int required = STRIDE * 4;
        if (nativeBuffer == null || bufferCapacity < required) {
            bufferCapacity = required;
            nativeBuffer = ByteBuffer.allocateDirect(required * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer();
        }
    }

    @Inject(method = "tick", at = @At("HEAD"), cancellable = true)
    private void nativeTickHead(CallbackInfo ci) {
        if (!NativeLoader.isLoaded) {
            return;
        }

        if (this.age >= this.maxAge) {
            this.dead = true;
            ci.cancel();
            return;
        }

        ensureBufferCapacity();
        nativeBuffer.clear();

        tickCount++;
        long currentTime = System.currentTimeMillis() / 1000;
        if (currentTime > lastLogTime) {
            lastLogTime = currentTime;
            LOGGER.info("[C++] ticks:{}/s", tickCount);
            tickCount = 0;
        }

        nativeBuffer.put(0, (float)this.lastX);
        nativeBuffer.put(1, (float)this.lastY);
        nativeBuffer.put(2, (float)this.lastZ);
        nativeBuffer.put(3, (float)this.x);
        nativeBuffer.put(4, (float)this.y);
        nativeBuffer.put(5, (float)this.z);
        nativeBuffer.put(6, (float)this.velocityX);
        nativeBuffer.put(7, (float)this.velocityY);
        nativeBuffer.put(8, (float)this.velocityZ);
        nativeBuffer.put(9, this.gravityStrength);
        nativeBuffer.put(10, this.velocityMultiplier);
        nativeBuffer.put(11, this.age);

        boolean success = ParticleNativeLib.tickSingleParticleFull(nativeBuffer);

        if (success) {
            this.lastX = nativeBuffer.get(0);
            this.lastY = nativeBuffer.get(1);
            this.lastZ = nativeBuffer.get(2);
            this.x = nativeBuffer.get(3);
            this.y = nativeBuffer.get(4);
            this.z = nativeBuffer.get(5);
            this.velocityX = nativeBuffer.get(6);
            this.velocityY = nativeBuffer.get(7);
            this.velocityZ = nativeBuffer.get(8);
            this.age = (int)nativeBuffer.get(11);

            if (this.age >= this.maxAge) {
                this.dead = true;
            }

            ci.cancel();
        }
    }

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
    protected float velocityMultiplier;
    @Shadow(remap = false)
    protected boolean dead;
}
