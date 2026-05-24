package com.nj.render

import net.minecraft.client.render.Camera
import net.minecraft.client.render.Frustum
import net.minecraft.entity.Entity
import org.joml.Matrix4f
import java.nio.ByteBuffer
import java.nio.ByteOrder

object RenderNative {

    private var available = false

    fun init() {
        available = try {
            getOrCreateFrustumBuffer(16)
            true
        } catch (e: UnsatisfiedLinkError) {
            false
        }
    }

    @JvmStatic
    fun isAvailable(): Boolean = available

    private var cachedVisible: BooleanArray? = null

    @JvmStatic
    external fun getOrCreateFrustumBuffer(maxEntities: Int): ByteBuffer

    @JvmStatic
    external fun updateFrustumPlanes(matrix: FloatArray, cx: Double, cy: Double, cz: Double)

    @JvmStatic
    external fun batchFrustumCullEntities(count: Int): Int

    @JvmStatic
    external fun getVisibilityResult(index: Int): Boolean

    @JvmStatic
    fun computeVisibility(
        entityList: List<Entity>,
        frustum: Frustum,
        camera: Camera
    ): BooleanArray {
        val count = entityList.size
        if (count == 0) return BooleanArray(0)

        var visible = cachedVisible
        if (visible == null || visible.size < count) {
            visible = BooleanArray(count)
            cachedVisible = visible
        }

        val byteBuf = getOrCreateFrustumBuffer(count)
        val buf = byteBuf.order(ByteOrder.nativeOrder()).asFloatBuffer()

        for (i in 0 until count) {
            val box = entityList[i].boundingBox
            val off = i * 7
            buf.put(off, box.minX.toFloat())
            buf.put(off + 1, box.minY.toFloat())
            buf.put(off + 2, box.minZ.toFloat())
            buf.put(off + 3, box.maxX.toFloat())
            buf.put(off + 4, box.maxY.toFloat())
            buf.put(off + 5, box.maxZ.toFloat())
            buf.put(off + 6, 0f)
        }

        val pos = camera.getCameraPos()
        val mat = Matrix4f()
        (frustum as com.nj.mixin.FrustumAccessor).getPositionProjectionMatrix().get(mat)

        val matrixData = FloatArray(16)
        mat.get(matrixData)

        updateFrustumPlanes(matrixData, pos.x, pos.y, pos.z)
        batchFrustumCullEntities(count)

        for (j in 0 until count) {
            visible!![j] = buf.get(j * 7 + 6) >= 0.5f
        }
        return visible
    }
}