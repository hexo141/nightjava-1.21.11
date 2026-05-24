package com.nj.collision

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer

object EntityCollisionNative {
    @JvmStatic external fun getOrCreateCollisionBuffer(minPairs: Int): ByteBuffer

    @JvmStatic external fun processEntityCollisions(pairCount: Int): Boolean

    @JvmStatic external fun cleanupCollisionNative()
}