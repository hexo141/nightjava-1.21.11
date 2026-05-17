#include <jni.h>
#include <immintrin.h>
#include <cmath>

extern "C" {
    JNIEXPORT jfloatArray JNICALL Java_com_nj_particle_ParticleNativeLib_computeBillboardVertices(
        JNIEnv* env, jclass clazz,
        jfloatArray particlePositions, jfloatArray particleSizes,
        jfloatArray particleColors, jfloatArray cameraRotation,
        jfloat tickProgress, jfloatArray cameraPosition, jint vertexCount) {

        int particleCount = particleCount;
        jfloat* positions = env->GetFloatArrayElements(particlePositions, nullptr);
        jfloat* sizes = env->GetFloatArrayElements(particleSizes, nullptr);
        jfloat* colors = env->GetFloatArrayElements(particleColors, nullptr);
        jfloat* cameraRot = env->GetFloatArrayElements(cameraRotation, nullptr);
        jfloat* camPos = env->GetFloatArrayElements(cameraPosition, nullptr);

        particleCount = vertexCount / 4;
        jfloatArray result = env->NewFloatArray(vertexCount * 8);
        jfloat* vertices = env->GetFloatArrayElements(result, nullptr);

        float qx = cameraRot[0], qy = cameraRot[1], qz = cameraRot[2], qw = cameraRot[3];
        float right_x = 1.0f - 2.0f * (qy * qy + qz * qz);
        float right_y = 2.0f * (qx * qy - qz * qw);
        float right_z = 2.0f * (qx * qz + qy * qw);
        float up_x = 2.0f * (qx * qy + qz * qw);
        float up_y = 1.0f - 2.0f * (qx * qx + qz * qz);
        float up_z = 2.0f * (qy * qz - qx * qw);

        int i = 0;
        int simd_end = (particleCount / 4) * 4;

        for (; i < simd_end; i += 4) {
            for (int j = 0; j < 4; ++j) {
                int pi = i + j;
                int vi = pi * 8;

                float px = positions[pi * 3] - camPos[0];
                float py = positions[pi * 3 + 1] - camPos[1];
                float pz = positions[pi * 3 + 2] - camPos[2];

                float size = sizes[pi];
                float r = colors[pi * 4], g = colors[pi * 4 + 1];
                float b = colors[pi * 4 + 2], a = colors[pi * 4 + 3];

                const float corners[4][2] = {
                    {-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f}
                };

                for (int c = 0; c < 4; ++c) {
                    int vi_c = vi + c * 6;
                    float cx = corners[c][0], cy = corners[c][1];
                    float offsetX = (cx * right_x + cy * up_x) * size;
                    float offsetY = (cx * right_y + cy * up_y) * size;
                    float offsetZ = (cx * right_z + cy * up_z) * size;

                    vertices[vi_c] = px + offsetX;
                    vertices[vi_c + 1] = py + offsetY;
                    vertices[vi_c + 2] = pz + offsetZ;
                    vertices[vi_c + 3] = r;
                    vertices[vi_c + 4] = g;
                    vertices[vi_c + 5] = b;
                }
            }
        }

        for (; i < particleCount; ++i) {
            int vi = i * 8;

            float px = positions[i * 3] - camPos[0];
            float py = positions[i * 3 + 1] - camPos[1];
            float pz = positions[i * 3 + 2] - camPos[2];

            float size = sizes[i];
            float r = colors[i * 4], g = colors[i * 4 + 1];
            float b = colors[i * 4 + 2], a = colors[i * 4 + 3];

            const float corners[4][2] = {
                {-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f}
            };

            for (int c = 0; c < 4; ++c) {
                int vi_c = vi + c * 6;
                float cx = corners[c][0], cy = corners[c][1];
                float offsetX = (cx * right_x + cy * up_x) * size;
                float offsetY = (cx * right_y + cy * up_y) * size;
                float offsetZ = (cx * right_z + cy * up_z) * size;

                vertices[vi_c] = px + offsetX;
                vertices[vi_c + 1] = py + offsetY;
                vertices[vi_c + 2] = pz + offsetZ;
                vertices[vi_c + 3] = r;
                vertices[vi_c + 4] = g;
                vertices[vi_c + 5] = b;
            }
        }

        env->ReleaseFloatArrayElements(particlePositions, positions, JNI_ABORT);
        env->ReleaseFloatArrayElements(particleSizes, sizes, JNI_ABORT);
        env->ReleaseFloatArrayElements(particleColors, colors, JNI_ABORT);
        env->ReleaseFloatArrayElements(cameraRotation, cameraRot, JNI_ABORT);
        env->ReleaseFloatArrayElements(cameraPosition, camPos, JNI_ABORT);
        env->ReleaseFloatArrayElements(result, vertices, 0);

        return result;
    }
}
