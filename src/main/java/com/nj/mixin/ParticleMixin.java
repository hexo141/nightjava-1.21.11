package com.nj.mixin;

import com.nj.NativeLoader;
import com.nj.particle.ParticleNativeLib;
import net.minecraft.client.particle.Particle;
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

    private static final int STRIDE = 15;

    private static FloatBuffer nativeBuffer;

    private static synchronized void ensureBufferCapacity() {
        if (nativeBuffer == null) {
            nativeBuffer = ByteBuffer.allocateDirect(STRIDE * 4)
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

        boolean success = ParticleNativeLib.tickSingleParticle(nativeBuffer);

        if (success) {
            this.lastX = nativeBuffer.get(3);
            this.lastY = nativeBuffer.get(4);
            this.lastZ = nativeBuffer.get(5);
            this.x = nativeBuffer.get(9);
            this.y = nativeBuffer.get(10);
            this.z = nativeBuffer.get(11);
            this.velocityX = nativeBuffer.get(12);
            this.velocityY = nativeBuffer.get(13);
            this.velocityZ = nativeBuffer.get(14);

            this.age++;

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
    protected boolean dead;
}
