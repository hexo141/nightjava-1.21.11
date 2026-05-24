package com.nj.mixin;

import com.nj.light.LightNative;
import com.nj.light.NightjavaDirectBufferAccess;
import net.minecraft.world.chunk.ChunkNibbleArray;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import java.nio.ByteBuffer;

@Mixin(ChunkNibbleArray.class)
public abstract class ChunkNibbleArrayMixin implements NightjavaDirectBufferAccess {

    @Shadow
    protected byte[] bytes;

    @Shadow
    public abstract byte[] asByteArray();

    @Unique
    private ByteBuffer nightjava$directBuffer;

    @Unique
    public ByteBuffer nightjava$getDirectBuffer() {
        if (!LightNative.isAvailable()) return null;
        if (nightjava$directBuffer == null) {
            byte[] arr = asByteArray();
            nightjava$directBuffer = ByteBuffer.allocateDirect(2048);
            nightjava$directBuffer.put(arr);
            nightjava$directBuffer.position(0);
        }
        return nightjava$directBuffer;
    }

    @Unique
    public void nightjava$syncFromDirect() {
        if (nightjava$directBuffer != null) {
            nightjava$directBuffer.position(0);
            byte[] arr = asByteArray();
            nightjava$directBuffer.get(arr);
        }
    }

    @Inject(method = "copy", at = @At("HEAD"), cancellable = true)
    private void onCopy(CallbackInfoReturnable<ChunkNibbleArray> cir) {
        if (!LightNative.isAvailable()) return;
        if (bytes == null) return;

        ChunkNibbleArray copy = new ChunkNibbleArray();
        byte[] copyBytes = copy.asByteArray();
        ByteBuffer dstBuf = ByteBuffer.allocateDirect(2048);
        ByteBuffer srcBuf = ByteBuffer.allocateDirect(2048);
        srcBuf.put(bytes);
        srcBuf.position(0);

        LightNative.nibbleBulkCopy(dstBuf, srcBuf, 2048);
        dstBuf.position(0);
        dstBuf.get(copyBytes);

        cir.setReturnValue(copy);
    }
}