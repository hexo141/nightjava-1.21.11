package com.nj.mixin;

import com.nj.collision.EntityCollisionNative;
import net.minecraft.entity.Entity;
import net.minecraft.entity.LivingEntity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyVariable;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.List;

@Mixin(LivingEntity.class)
public abstract class LivingEntityCrammingMixin {

    private static final int PAIR_STRIDE = 8;
    private static final int BATCH_THRESHOLD = 4;

    @Unique
    private List<Entity> nightjava$crammedList;

    @ModifyVariable(method = "tickCramming", at = @At(value = "STORE"), ordinal = 0)
    private List<Entity> captureList(List<Entity> list) {
        nightjava$crammedList = list;
        return list;
    }

    @Inject(method = "tickCramming", at = @At(value = "INVOKE", target = "Lnet/minecraft/entity/LivingEntity;pushAway(Lnet/minecraft/entity/Entity;)V"), cancellable = true)
    private void cancelPushAway(CallbackInfo ci) {
        if (nightjava$crammedList != null && nightjava$crammedList.size() >= BATCH_THRESHOLD) {
            ci.cancel();
        }
    }

    @Inject(method = "tickCramming", at = @At("RETURN"))
    private void processBatch(CallbackInfo ci) {
        List<Entity> list = nightjava$crammedList;
        nightjava$crammedList = null;

        if (list == null || list.size() < BATCH_THRESHOLD) {
            return;
        }

        LivingEntity self = (LivingEntity) (Object) this;

        ByteBuffer byteBuf = EntityCollisionNative.getOrCreateCollisionBuffer(list.size());
        FloatBuffer buf = byteBuf.order(ByteOrder.nativeOrder()).asFloatBuffer();

        int validCount = 0;
        for (int i = 0; i < list.size(); i++) {
            Entity other = list.get(i);
            int base = validCount * PAIR_STRIDE;
            buf.put(base, (float) self.getX());
            buf.put(base + 1, (float) self.getZ());
            buf.put(base + 2, (float) other.getX());
            buf.put(base + 3, (float) other.getZ());
            int bits = 0;
            if (self.isPushable()) bits |= 1;
            if (other.isPushable()) bits |= 2;
            buf.put(base + 4, Float.intBitsToFloat(bits));
            validCount++;
        }

        if (validCount > 0) {
            EntityCollisionNative.processEntityCollisions(validCount);

            for (int i = 0; i < list.size(); i++) {
                Entity other = list.get(i);
                int base = i * PAIR_STRIDE;
                float dx = buf.get(base);
                float dz = buf.get(base + 1);
                if (dx != 0.0f || dz != 0.0f) {
                    self.addVelocity(dx, 0.0, dz);
                }
                float odx = buf.get(base + 2);
                float odz = buf.get(base + 3);
                if (odx != 0.0f || odz != 0.0f) {
                    other.addVelocity(odx, 0.0, odz);
                }
            }
        }
    }
}