package com.nj.mixin;

import com.nj.light.LightNative;
import com.nj.light.NightjavaDirectBufferAccess;
import net.minecraft.util.math.BlockPos;
import net.minecraft.util.math.ChunkSectionPos;
import net.minecraft.util.math.Direction;
import net.minecraft.world.chunk.ChunkNibbleArray;
import net.minecraft.world.chunk.light.ChunkLightProvider;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;

import java.nio.ByteBuffer;

@Mixin(ChunkLightProvider.class)
public abstract class ChunkLightProviderMixin {

    @Shadow
    @Final
    protected static Direction[] DIRECTIONS;

    @Shadow
    protected abstract ChunkNibbleArray getLightSection(ChunkSectionPos pos);

    @Shadow
    protected abstract int getOpacity(net.minecraft.block.BlockState state);

    @Shadow
    protected abstract net.minecraft.block.BlockState getStateForLighting(BlockPos pos);

    @Shadow
    protected abstract int getLightLevel(BlockPos pos);

    /**
     * Build opacity grid for a section. Each entry is opacity 0-15.
     * Returns null if all blocks are transparent (no need for propagation).
     */
    private byte[] buildOpacityGrid(ChunkSectionPos sectionPos) {
        int sx = sectionPos.getMinX();
        int sy = sectionPos.getMinY();
        int sz = sectionPos.getMinZ();
        byte[] grid = new byte[2048];
        boolean hasOpaque = false;

        BlockPos.Mutable mpos = new BlockPos.Mutable();
        for (int y = 0; y < 16; y++) {
            for (int z = 0; z < 16; z++) {
                for (int x = 0; x < 16; x++) {
                    mpos.set(sx + x, sy + y, sz + z);
                    int op = getOpacity(getStateForLighting(mpos));
                    if (op > 1) hasOpaque = true;
                    int idx = (y << 8) | (z << 4) | x;
                    int byteIdx = idx >> 1;
                    int shift = (idx & 1) << 2;
                    grid[byteIdx] = (byte)((grid[byteIdx] & ~(0xF << shift)) | ((op & 0xF) << shift));
                }
            }
        }
        return hasOpaque ? grid : null;
    }

    /**
     * Attempt to accelerate light propagation within a single section using C++.
     * Falls back to Java path if native is unavailable or section crosses boundaries.
     */
    protected boolean tryNativePropagate(long blockPos, long flags, boolean isIncrease, boolean isSkyLight) {
        if (!LightNative.isAvailable()) return false;

        long sectionPos = ChunkSectionPos.fromBlockPos(blockPos);
        ChunkNibbleArray nibbleArray = getLightSection(ChunkSectionPos.from(sectionPos));
        if (nibbleArray == null) return false;

        ChunkSectionPos spos = ChunkSectionPos.from(sectionPos);
        byte[] opacityGrid = buildOpacityGrid(spos);
        if (opacityGrid == null) return false;

        byte[] srcLuminance = null;
        if (!isSkyLight) {
            srcLuminance = new byte[2048];
        }

        ByteBuffer lightBuf = ((NightjavaDirectBufferAccess)(Object)nibbleArray).nightjava$getDirectBuffer();
        if (lightBuf == null) return false;

        int lightLevel = (int)(flags & 0xF);
        int lx = BlockPos.unpackLongX(blockPos) - spos.getMinX();
        int ly = BlockPos.unpackLongY(blockPos) - spos.getMinY();
        int lz = BlockPos.unpackLongZ(blockPos) - spos.getMinZ();
        int posPacked = lx | (ly << 8) | (lz << 4);

        int flagPacked = (int)(flags & 0x7FF);
        long[] posArray = new long[]{posPacked};
        long[] flagArray = new long[]{flagPacked};

        if (isIncrease) {
            LightNative.propagateBlockLight(
                lightBuf, spos.getSectionX(), spos.getSectionY(), spos.getSectionZ(),
                opacityGrid, srcLuminance != null ? srcLuminance : new byte[2048],
                posArray, flagArray, 0, 1);
        } else {
            LightNative.propagateBlockLight(
                lightBuf, spos.getSectionX(), spos.getSectionY(), spos.getSectionZ(),
                opacityGrid, srcLuminance != null ? srcLuminance : new byte[2048],
                posArray, flagArray, 1, 0);
        }

        ((NightjavaDirectBufferAccess)(Object)nibbleArray).nightjava$syncFromDirect();
        return true;
    }
}