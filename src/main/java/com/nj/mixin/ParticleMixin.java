package com.nj.mixin;

import com.nj.NativeLoader;
import com.nj.particle.ParticleNativeLib;
import net.minecraft.client.particle.Particle;
import net.minecraft.client.world.ClientWorld;
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
    private static final int BATCH_SIZE = 512;
    private static final int STRIDE = 10;

    private static FloatBuffer nativeBuffer;
    private static int bufferCapacity = 0;
    private static int tickCount = 0;
    private static long lastLogTime = 0;

    private static synchronized void ensureBufferCapacity() {
        int required = BATCH_SIZE * STRIDE;
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

        ParticleMixin accessor = (ParticleMixin)(Object)this;

        if (accessor.getAge() >= accessor.getMaxAge()) {
            accessor.setDead(true);
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

        nativeBuffer.put(0, (float)accessor.getX());
        nativeBuffer.put(1, (float)accessor.getY());
        nativeBuffer.put(2, (float)accessor.getZ());
        nativeBuffer.put(3, (float)accessor.getVelocityX());
        nativeBuffer.put(4, (float)accessor.getVelocityY());
        nativeBuffer.put(5, (float)accessor.getVelocityZ());
        nativeBuffer.put(6, this.gravityStrength);
        nativeBuffer.put(7, this.velocityMultiplier);
        nativeBuffer.put(8, accessor.getAge());
        nativeBuffer.put(9, accessor.getMaxAge());

        boolean success = ParticleNativeLib.tickSingleParticle(nativeBuffer);

        if (success) {
            accessor.setX(nativeBuffer.get(0));
            accessor.setY(nativeBuffer.get(1));
            accessor.setZ(nativeBuffer.get(2));
            accessor.setVelocityX(nativeBuffer.get(3));
            accessor.setVelocityY(nativeBuffer.get(4));
            accessor.setVelocityZ(nativeBuffer.get(5));
            accessor.setAge((int)nativeBuffer.get(8));

            if (accessor.getAge() >= accessor.getMaxAge()) {
                accessor.setDead(true);
            }

            ci.cancel();
        }
    }

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

    public double getX() { return x; }
    public double getY() { return y; }
    public double getZ() { return z; }
    public double getVelocityX() { return velocityX; }
    public double getVelocityY() { return velocityY; }
    public double getVelocityZ() { return velocityZ; }
    public int getAge() { return age; }
    public int getMaxAge() { return maxAge; }
    public boolean isDead() { return dead; }

    public void setX(double v) { x = v; }
    public void setY(double v) { y = v; }
    public void setZ(double v) { z = v; }
    public void setVelocityX(double v) { velocityX = v; }
    public void setVelocityY(double v) { velocityY = v; }
    public void setVelocityZ(double v) { velocityZ = v; }
    public void setAge(int v) { age = v; }
    public void setDead(boolean v) { dead = v; }
}
