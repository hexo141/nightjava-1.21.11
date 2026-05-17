package com.nj.particle;

import java.nio.FloatBuffer;

object ParticleNativeLib {
    @JvmStatic external fun batchTickParticles(
        positions: FloatArray,
        velocities: FloatArray,
        ages: IntArray,
        maxAges: IntArray,
        count: Int,
        gravityStrength: Float,
        velocityMultiplier: Float
    ): Boolean

    @JvmStatic external fun batchTickParticlesDirect(
        buffer: FloatBuffer,
        particleCount: Int,
        stride: Int
    ): Boolean

    @JvmStatic external fun batchTickParticlesFull(
        buffer: FloatBuffer,
        particleCount: Int,
        stride: Int
    ): Boolean

    @JvmStatic external fun applyPhysicsOnly(
        buffer: FloatBuffer,
        particleCount: Int,
        stride: Int
    ): Boolean

    @JvmStatic external fun tickSingleParticle(
        buffer: FloatBuffer
    ): Boolean

    @JvmStatic external fun tickSingleParticleFull(
        buffer: FloatBuffer
    ): Boolean

    @JvmStatic external fun computeBillboardVertices(
        particlePositions: FloatArray,
        particleSizes: FloatArray,
        particleColors: FloatArray,
        cameraRotation: FloatArray,
        tickProgress: Float,
        cameraPosition: FloatArray,
        vertexCount: Int
    ): FloatArray

    @JvmStatic external fun batchMoveParticles(
        positions: FloatArray,
        velocities: FloatArray,
        count: Int,
        worldBounds: FloatArray
    ): Boolean
}
