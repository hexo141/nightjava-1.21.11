package com.nj.mixin;

import com.nj.NativeLoader;
import com.nj.particle.ParticleNativeLib;
import net.minecraft.client.particle.Particle;
import net.minecraft.client.particle.ParticleManager;
import net.minecraft.client.particle.ParticleRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.Iterator;
import java.util.Queue;

@Mixin(ParticleRenderer.class)
public abstract class ParticleRendererMixin {

    private static final int STRIDE = 13;

    @Shadow
    private Queue<Particle> particles;

    @Shadow
    protected ParticleManager particleManager;

    @Inject(method = "tick", at = @At("HEAD"), cancellable = true)
    private void batchTickNative(CallbackInfo ci) {
        if (!NativeLoader.isLoaded) {
            return;
        }

        int count = particles.size();
        if (count == 0) {
            return;
        }

        ByteBuffer byteBuf = ParticleNativeLib.getOrCreateNativeBuffer(count);
        FloatBuffer buffer = byteBuf.order(ByteOrder.nativeOrder()).asFloatBuffer();

        int idx = 0;
        for (Particle particle : particles) {
            ParticleAccessor p = (ParticleAccessor) particle;

            buffer.put(idx, (float) p.getLastX());
            buffer.put(idx + 1, (float) p.getLastY());
            buffer.put(idx + 2, (float) p.getLastZ());
            buffer.put(idx + 3, (float) p.getX());
            buffer.put(idx + 4, (float) p.getY());
            buffer.put(idx + 5, (float) p.getZ());
            buffer.put(idx + 6, (float) p.getVelocityX());
            buffer.put(idx + 7, (float) p.getVelocityY());
            buffer.put(idx + 8, (float) p.getVelocityZ());
            buffer.put(idx + 9, p.getGravityStrength());
            buffer.put(idx + 10, p.getVelocityMultiplier());
            buffer.put(idx + 11, (float) p.getAge());
            buffer.put(idx + 12, (float) particle.getMaxAge());

            idx += STRIDE;
        }

        boolean success = ParticleNativeLib.tickAllParticlesInPlace(count);

        if (success) {
            idx = 0;
            Iterator<Particle> it = particles.iterator();
            while (it.hasNext()) {
                Particle particle = it.next();
                ParticleAccessor p = (ParticleAccessor) particle;

                p.setLastX(buffer.get(idx));
                p.setLastY(buffer.get(idx + 1));
                p.setLastZ(buffer.get(idx + 2));
                p.setX(buffer.get(idx + 3));
                p.setY(buffer.get(idx + 4));
                p.setZ(buffer.get(idx + 5));
                p.setVelocityX(buffer.get(idx + 6));
                p.setVelocityY(buffer.get(idx + 7));
                p.setVelocityZ(buffer.get(idx + 8));
                int newAge = (int) buffer.get(idx + 11);
                p.setAge(newAge);

                if (newAge >= particle.getMaxAge()) {
                    p.setDead(true);
                }

                if (p.isDead()) {
                    particle.getGroup().ifPresent(group ->
                        ((ParticleManagerAccessor) particleManager).invokeAddTo(group, -1));
                    it.remove();
                }

                idx += STRIDE;
            }
        }

        ci.cancel();
    }
}