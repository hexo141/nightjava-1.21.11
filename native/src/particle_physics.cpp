#include <jni.h>
#include <cmath>

extern "C" {
    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_tickSingleParticleFull(
        JNIEnv* env, jclass clazz,
        jobject buffer) {

        float* data = (float*)env->GetDirectBufferAddress(buffer);
        if (data == nullptr) return JNI_FALSE;

        float lastX = data[0];
        float lastY = data[1];
        float lastZ = data[2];
        float x = data[3];
        float y = data[4];
        float z = data[5];
        float vx = data[6];
        float vy = data[7];
        float vz = data[8];
        float gravity = data[9];
        float multiplier = data[10];
        int age = (int)data[11];

        float newX = x;
        float newY = y;
        float newZ = z;

        vy -= 0.04f * gravity;

        vx *= multiplier;
        vy *= multiplier;
        vz *= multiplier;

        newX += vx;
        newY += vy;
        newZ += vz;

        age++;

        data[0] = newX;
        data[1] = newY;
        data[2] = newZ;
        data[3] = newX;
        data[4] = newY;
        data[5] = newZ;
        data[6] = vx;
        data[7] = vy;
        data[8] = vz;
        data[11] = (float)age;

        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_tickSingleParticle(
        JNIEnv* env, jclass clazz,
        jobject buffer) {

        float* data = (float*)env->GetDirectBufferAddress(buffer);
        if (data == nullptr) return JNI_FALSE;

        float x = data[0];
        float y = data[1];
        float z = data[2];
        float vx = data[3];
        float vy = data[4];
        float vz = data[5];
        float gravity = data[6];
        float multiplier = data[7];
        int age = (int)data[8];
        int maxAge = (int)data[9];

        vy -= 0.04f * gravity;

        vx *= multiplier;
        vy *= multiplier;
        vz *= multiplier;

        x += vx;
        y += vy;
        z += vz;

        age++;

        data[0] = x;
        data[1] = y;
        data[2] = z;
        data[3] = vx;
        data[4] = vy;
        data[5] = vz;
        data[8] = (float)age;

        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_applyPhysicsOnly(
        JNIEnv* env, jclass clazz,
        jobject buffer, jint particleCount, jint stride) {

        if (particleCount <= 0) return JNI_TRUE;

        float* data = (float*)env->GetDirectBufferAddress(buffer);
        if (data == nullptr) return JNI_FALSE;

        const float gravity_delta = 0.04f;
        const float multiplier = 0.98f;

        for (int i = 0; i < particleCount; i++) {
            int idx = i * stride;

            float vx = data[idx];
            float vy = data[idx + 1];
            float vz = data[idx + 2];
            float gravity = data[idx + 3];

            vy -= gravity_delta * gravity;

            vx *= multiplier;
            vy *= multiplier;
            vz *= multiplier;

            data[idx] = vx;
            data[idx + 1] = vy;
            data[idx + 2] = vz;
        }

        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchTickParticlesFull(
        JNIEnv* env, jclass clazz,
        jobject buffer, jint particleCount, jint stride) {

        if (particleCount <= 0) return JNI_TRUE;

        float* data = (float*)env->GetDirectBufferAddress(buffer);
        if (data == nullptr) return JNI_FALSE;

        const float gravity_delta = 0.04f;
        const float multiplier = 0.98f;

        for (int i = 0; i < particleCount; i++) {
            int idx = i * stride;

            float lastX = data[idx];
            float lastY = data[idx + 1];
            float lastZ = data[idx + 2];
            float x = data[idx + 3];
            float y = data[idx + 4];
            float z = data[idx + 5];
            float vx = data[idx + 6];
            float vy = data[idx + 7];
            float vz = data[idx + 8];
            float gravity = data[idx + 9];
            int age = (int)data[idx + 11];
            int maxAge = (int)data[idx + 12];

            if (age >= maxAge) continue;

            vy -= gravity_delta * gravity;
            vx *= multiplier;
            vy *= multiplier;
            vz *= multiplier;

            x += vx;
            y += vy;
            z += vz;
            age++;

            data[idx] = x;
            data[idx + 1] = y;
            data[idx + 2] = z;
            data[idx + 3] = x;
            data[idx + 4] = y;
            data[idx + 5] = z;
            data[idx + 6] = vx;
            data[idx + 7] = vy;
            data[idx + 8] = vz;
            data[idx + 11] = (float)age;
        }

        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchTickParticlesDirect(
        JNIEnv* env, jclass clazz,
        jobject buffer, jint particleCount, jint stride) {

        if (particleCount <= 0) return JNI_TRUE;

        float* data = (float*)env->GetDirectBufferAddress(buffer);
        if (data == nullptr) return JNI_FALSE;

        const float gravity_delta = 0.04f;
        const float multiplier = 0.98f;

        for (int i = 0; i < particleCount; i++) {
            int idx = i * stride;

            float x = data[idx];
            float y = data[idx + 1];
            float z = data[idx + 2];
            float vx = data[idx + 3];
            float vy = data[idx + 4];
            float vz = data[idx + 5];
            float gravity = data[idx + 6];

            vy -= gravity_delta * gravity;
            vx *= multiplier;
            vy *= multiplier;
            vz *= multiplier;

            x += vx;
            y += vy;
            z += vz;

            data[idx] = x;
            data[idx + 1] = y;
            data[idx + 2] = z;
            data[idx + 3] = vx;
            data[idx + 4] = vy;
            data[idx + 5] = vz;
        }

        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchTickParticles(
        JNIEnv* env, jclass clazz,
        jfloatArray positions, jfloatArray velocities,
        jintArray ages, jintArray maxAges, jint count,
        jfloat gravityStrength, jfloat velocityMultiplier) {

        if (count <= 0) return JNI_TRUE;

        jfloat* pos_arr = env->GetFloatArrayElements(positions, nullptr);
        jfloat* vel_arr = env->GetFloatArrayElements(velocities, nullptr);
        jint* age_arr = env->GetIntArrayElements(ages, nullptr);

        const float gravityDelta = 0.04f * gravityStrength;
        for (int i = 0; i < count; ++i) {
            vel_arr[i * 3 + 1] -= gravityDelta;
        }

        for (int i = 0; i < count * 3; ++i) {
            vel_arr[i] *= velocityMultiplier;
        }

        for (int i = 0; i < count; ++i) {
            pos_arr[i * 3] += vel_arr[i * 3];
            pos_arr[i * 3 + 1] += vel_arr[i * 3 + 1];
            pos_arr[i * 3 + 2] += vel_arr[i * 3 + 2];
            age_arr[i]++;
        }

        env->ReleaseFloatArrayElements(positions, pos_arr, 0);
        env->ReleaseFloatArrayElements(velocities, vel_arr, 0);
        env->ReleaseIntArrayElements(ages, age_arr, 0);

        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_batchMoveParticles(
        JNIEnv* env, jclass clazz,
        jfloatArray positions, jfloatArray velocities,
        jint count, jfloatArray worldBounds) {

        if (count <= 0) return JNI_TRUE;

        jfloat* pos_arr = env->GetFloatArrayElements(positions, nullptr);
        jfloat* vel_arr = env->GetFloatArrayElements(velocities, nullptr);
        jfloat* bounds = env->GetFloatArrayElements(worldBounds, nullptr);

        float min_x = bounds[0], min_y = bounds[1], min_z = bounds[2];
        float max_x = bounds[3], max_y = bounds[4], max_z = bounds[5];

        for (int i = 0; i < count; ++i) {
            float px = pos_arr[i * 3];
            float py = pos_arr[i * 3 + 1];
            float pz = pos_arr[i * 3 + 2];

            if (px < min_x) { px = min_x; vel_arr[i * 3] = 0; }
            if (px > max_x) { px = max_x; vel_arr[i * 3] = 0; }
            if (py < min_y) { py = min_y; vel_arr[i * 3 + 1] = 0; }
            if (py > max_y) { py = max_y; vel_arr[i * 3 + 1] = 0; }
            if (pz < min_z) { pz = min_z; vel_arr[i * 3 + 2] = 0; }
            if (pz > max_z) { pz = max_z; vel_arr[i * 3 + 2] = 0; }

            pos_arr[i * 3] = px;
            pos_arr[i * 3 + 1] = py;
            pos_arr[i * 3 + 2] = pz;
        }

        env->ReleaseFloatArrayElements(positions, pos_arr, 0);
        env->ReleaseFloatArrayElements(velocities, vel_arr, 0);
        env->ReleaseFloatArrayElements(worldBounds, bounds, JNI_ABORT);

        return JNI_TRUE;
    }
}
