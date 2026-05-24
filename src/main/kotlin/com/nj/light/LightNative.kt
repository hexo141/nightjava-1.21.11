package com.nj.light

import java.nio.ByteBuffer

object LightNative {

    private var available = false

    fun init() {
        available = try {
            nibbleBulkFill(ByteBuffer.allocateDirect(1), 0, 1)
            true
        } catch (e: UnsatisfiedLinkError) {
            false
        }
    }

    @JvmStatic
    fun isAvailable(): Boolean = available

    @JvmStatic
    external fun nibbleBulkCopy(dst: ByteBuffer, src: ByteBuffer, count: Int)

    @JvmStatic
    external fun nibbleBulkFill(buf: ByteBuffer, value: Int, count: Int)

    @JvmStatic
    external fun nibbleBulkDiff(bufA: ByteBuffer, bufB: ByteBuffer, count: Int, outDiffs: IntArray): Int

    @JvmStatic
    external fun propagateBlockLight(
        lightBuf: ByteBuffer,
        sectionX: Int, sectionY: Int, sectionZ: Int,
        opacityGrid: ByteArray, sourceLuminance: ByteArray,
        queuePositions: LongArray, queueFlags: LongArray,
        decreaseCount: Int, increaseCount: Int
    ): Int

    @JvmStatic
    external fun propagateSkyLight(
        lightBuf: ByteBuffer,
        sectionX: Int, sectionY: Int, sectionZ: Int,
        opacityGrid: ByteArray,
        queuePositions: LongArray, queueFlags: LongArray,
        decreaseCount: Int, increaseCount: Int,
        sectionsBelow: Int
    ): Int
}