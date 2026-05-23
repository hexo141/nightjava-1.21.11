#include <jni.h>
#include <cmath>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <cstring>

static const int NUM_THREADS = 16;
static const int PARTICLE_STRIDE = 13;

class ParticleThreadPool {
private:
    std::vector<std::thread> workers;
    std::atomic<bool> shutdown_flag;

    std::mutex work_mutex;
    std::condition_variable work_cv;
    std::condition_variable done_cv;

    std::atomic<int> next_chunk;
    int total_chunks;
    int chunk_size;
    float* data;
    int count;
    int stride;
    std::atomic<int> active_workers;
    bool work_ready;
    bool idle;

    void worker_loop(int thread_id) {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(work_mutex);
                work_cv.wait(lock, [this]() { return work_ready || shutdown_flag.load(); });
                if (shutdown_flag.load()) return;
            }

            while (true) {
                int chunk = next_chunk.fetch_add(1);
                if (chunk >= total_chunks) break;

                int start = chunk * chunk_size;
                int end = (chunk == total_chunks - 1) ? count : start + chunk_size;

                process_chunk(data, start, end, stride);
            }

            int done = active_workers.fetch_sub(1);
            if (done == 1) {
                std::lock_guard<std::mutex> lock(work_mutex);
                work_ready = false;
                idle = true;
                done_cv.notify_one();
            }

            {
                std::unique_lock<std::mutex> lock(work_mutex);
                work_cv.wait(lock, [this]() { return work_ready || shutdown_flag.load(); });
                if (shutdown_flag.load()) return;
            }
        }
    }

    static void process_chunk(float* data, int start, int end, int stride) {
        const float gravity_delta = 0.04f;

        for (int i = start; i < end; i++) {
            int idx = i * stride;

            float oldX = data[idx + 3];
            float oldY = data[idx + 4];
            float oldZ = data[idx + 5];

            float vx = data[idx + 6];
            float vy = data[idx + 7];
            float vz = data[idx + 8];
            float gravity = data[idx + 9];
            float vMultiplier = data[idx + 10];
            int age = (int)data[idx + 11];
            int maxAge = (int)data[idx + 12];

            if (age >= maxAge) continue;

            vy -= gravity_delta * gravity;

            data[idx] = oldX;
            data[idx + 1] = oldY;
            data[idx + 2] = oldZ;
            data[idx + 3] = oldX + vx;
            data[idx + 4] = oldY + vy;
            data[idx + 5] = oldZ + vz;

            vx *= vMultiplier;
            vy *= vMultiplier;
            vz *= vMultiplier;

            data[idx + 6] = vx;
            data[idx + 7] = vy;
            data[idx + 8] = vz;
            data[idx + 11] = (float)(age + 1);
        }
    }

public:
    ParticleThreadPool() : shutdown_flag(false), work_ready(false), idle(true), active_workers(0) {
        for (int i = 0; i < NUM_THREADS; i++) {
            workers.emplace_back(&ParticleThreadPool::worker_loop, this, i);
        }
    }

    ~ParticleThreadPool() {
        shutdown_flag.store(true);
        work_cv.notify_all();
        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }
    }

    void execute(float* buf, int particle_count, int strd) {
        data = buf;
        count = particle_count;
        stride = strd;

        int num_chunks = NUM_THREADS;
        if (particle_count < NUM_THREADS * 16) {
            num_chunks = (particle_count + 15) / 16;
            if (num_chunks < 1) num_chunks = 1;
        }

        total_chunks = num_chunks;
        chunk_size = (particle_count + num_chunks - 1) / num_chunks;
        next_chunk.store(0);
        active_workers.store(num_chunks);

        {
            std::lock_guard<std::mutex> lock(work_mutex);
            idle = false;
            work_ready = true;
        }
        work_cv.notify_all();

        {
            std::unique_lock<std::mutex> lock(work_mutex);
            done_cv.wait(lock, [this]() { return idle; });
        }
    }

    void execute_render(float* physicsBuf, int particle_count, int physicsStride,
                        float* vertexBuf, int vertexStride,
                        float camX, float camY, float camZ,
                        float rotX, float rotY, float rotZ, float rotW,
                        float tickDelta) {

        int num_chunks = NUM_THREADS;
        if (particle_count < NUM_THREADS * 16) {
            num_chunks = (particle_count + 15) / 16;
            if (num_chunks < 1) num_chunks = 1;
        }

        total_chunks = num_chunks;
        chunk_size = (particle_count + num_chunks - 1) / num_chunks;
        count = particle_count;
        next_chunk.store(0);
        active_workers.store(num_chunks);

        float qx = rotX, qy = rotY, qz = rotZ, qw = rotW;

        float rx = 1.0f - 2.0f * (qy * qy + qz * qz);
        float ry = 2.0f * (qx * qy - qz * qw);
        float rz = 2.0f * (qx * qz + qy * qw);
        float ux = 2.0f * (qx * qy + qz * qw);
        float uy = 1.0f - 2.0f * (qx * qx + qz * qz);
        float uz = 2.0f * (qy * qz - qx * qw);

        {
            std::lock_guard<std::mutex> lock(work_mutex);
            idle = false;
            work_ready = true;
            data = physicsBuf;
            stride = physicsStride;
        }
        work_cv.notify_all();

        for (int t = 0; t < num_chunks; t++) {
            int chunk = next_chunk.fetch_add(1);
            if (chunk >= total_chunks) break;

            int start = chunk * chunk_size;
            int end = (chunk == total_chunks - 1) ? particle_count : start + chunk_size;

            for (int i = start; i < end; i++) {
                int pidx = i * physicsStride;
                int vidx = i * vertexStride;

                float lastX = physicsBuf[pidx];
                float lastY = physicsBuf[pidx + 1];
                float lastZ = physicsBuf[pidx + 2];
                float x = physicsBuf[pidx + 3];
                float y = physicsBuf[pidx + 4];
                float z = physicsBuf[pidx + 5];

                int age = (int)physicsBuf[pidx + 11];
                int maxAge = (int)physicsBuf[pidx + 12];
                if (age >= maxAge) {
                    for (int j = 0; j < vertexStride; j++) {
                        vertexBuf[vidx + j] = 0.0f;
                    }
                    continue;
                }

                float px = (lastX + (x - lastX) * tickDelta) - camX;
                float py = (lastY + (y - lastY) * tickDelta) - camY;
                float pz = (lastZ + (z - lastZ) * tickDelta) - camZ;

                vertexBuf[vidx] = px;
                vertexBuf[vidx + 1] = py;
                vertexBuf[vidx + 2] = pz;
            }
        }

        active_workers.store(0);
        {
            std::lock_guard<std::mutex> lock(work_mutex);
            work_ready = false;
            idle = true;
            done_cv.notify_one();
        }

        {
            std::unique_lock<std::mutex> lock(work_mutex);
            done_cv.wait(lock, [this]() { return idle; });
        }
    }
};

static ParticleThreadPool* g_pool = nullptr;

static void init_pool() {
    if (g_pool == nullptr) {
        g_pool = new ParticleThreadPool();
    }
}

static float* g_persistentBuffer = nullptr;
static int g_bufferCapacity = 0;
static jobject g_globalBufferRef = nullptr;
static std::mutex g_bufferMutex;

extern "C" {

    JNIEXPORT jobject JNICALL Java_com_nj_particle_ParticleNativeLib_getOrCreateNativeBuffer(
        JNIEnv* env, jclass, jint minParticleCount) {

        std::lock_guard<std::mutex> lock(g_bufferMutex);

        int requiredFloats = minParticleCount * PARTICLE_STRIDE;
        if (requiredFloats > g_bufferCapacity) {
            if (g_persistentBuffer) {
                delete[] g_persistentBuffer;
                g_persistentBuffer = nullptr;
            }
            if (g_globalBufferRef) {
                env->DeleteGlobalRef(g_globalBufferRef);
                g_globalBufferRef = nullptr;
            }
            g_persistentBuffer = new float[requiredFloats];
            std::memset(g_persistentBuffer, 0, requiredFloats * sizeof(float));
            g_bufferCapacity = requiredFloats;
        }

        if (!g_globalBufferRef) {
            jobject localRef = env->NewDirectByteBuffer(g_persistentBuffer,
                (jlong)g_bufferCapacity * sizeof(float));
            g_globalBufferRef = env->NewGlobalRef(localRef);
            env->DeleteLocalRef(localRef);
        }

        return g_globalBufferRef;
    }

    JNIEXPORT jint JNICALL Java_com_nj_particle_ParticleNativeLib_getNativeBufferCapacity(
        JNIEnv*, jclass) {
        return g_bufferCapacity / PARTICLE_STRIDE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_tickAllParticlesInPlace(
        JNIEnv* env, jclass, jint particleCount) {

        if (particleCount <= 0 || !g_persistentBuffer) return JNI_FALSE;

        init_pool();
        g_pool->execute(g_persistentBuffer, (int)particleCount, PARTICLE_STRIDE);

        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_computeCameraRelativePositions(
        JNIEnv* env, jclass,
        jobject vertexBuffer, jint particleCount, jint vertexStride,
        jfloat camX, jfloat camY, jfloat camZ,
        jfloat rotX, jfloat rotY, jfloat rotZ, jfloat rotW,
        jfloat tickDelta) {

        if (particleCount <= 0 || !g_persistentBuffer) return JNI_FALSE;

        float* vbuf = (float*)env->GetDirectBufferAddress(vertexBuffer);
        if (!vbuf) return JNI_FALSE;

        float qx = rotX, qy = rotY, qz = rotZ, qw = rotW;

        float rx = 1.0f - 2.0f * (qy * qy + qz * qz);
        float ry = 2.0f * (qx * qy - qz * qw);
        float rz = 2.0f * (qx * qz + qy * qw);
        float ux = 2.0f * (qx * qy + qz * qw);
        float uy = 1.0f - 2.0f * (qx * qx + qz * qz);
        float uz = 2.0f * (qy * qz - qx * qw);

        for (int i = 0; i < particleCount; i++) {
            int pidx = i * PARTICLE_STRIDE;
            int vidx = i * vertexStride;

            float lastX = g_persistentBuffer[pidx];
            float lastY = g_persistentBuffer[pidx + 1];
            float lastZ = g_persistentBuffer[pidx + 2];
            float x = g_persistentBuffer[pidx + 3];
            float y = g_persistentBuffer[pidx + 4];
            float z = g_persistentBuffer[pidx + 5];

            int age = (int)g_persistentBuffer[pidx + 11];
            int maxAge = (int)g_persistentBuffer[pidx + 12];

            if (age >= maxAge) {
                for (int j = 0; j < vertexStride; j++) {
                    vbuf[vidx + j] = 0.0f;
                }
                continue;
            }

            float px = (lastX + (x - lastX) * tickDelta) - camX;
            float py = (lastY + (y - lastY) * tickDelta) - camY;
            float pz = (lastZ + (z - lastZ) * tickDelta) - camZ;

            vbuf[vidx] = px;
            vbuf[vidx + 1] = py;
            vbuf[vidx + 2] = pz;
        }

        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_computeBillboardVerticesNative(
        JNIEnv* env, jclass,
        jobject vertexBuffer, jint particleCount, jint vertexStride,
        jfloat camX, jfloat camY, jfloat camZ,
        jfloat rotX, jfloat rotY, jfloat rotZ, jfloat rotW,
        jfloat tickDelta) {

        if (particleCount <= 0 || !g_persistentBuffer) return JNI_FALSE;

        float* vbuf = (float*)env->GetDirectBufferAddress(vertexBuffer);
        if (!vbuf) return JNI_FALSE;

        init_pool();
        g_pool->execute_render(g_persistentBuffer, (int)particleCount, PARTICLE_STRIDE,
                               vbuf, vertexStride,
                               camX, camY, camZ,
                               rotX, rotY, rotZ, rotW,
                               tickDelta);

        return JNI_TRUE;
    }

    JNIEXPORT jboolean JNICALL Java_com_nj_particle_ParticleNativeLib_tickSingleParticle(
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

        vy -= 0.04f * gravity;

        float newX = x + vx;
        float newY = y + vy;
        float newZ = z + vz;

        vx *= 0.98f;
        vy *= 0.98f;
        vz *= 0.98f;

        data[0] = x;
        data[1] = y;
        data[2] = z;
        data[3] = newX;
        data[4] = newY;
        data[5] = newZ;
        data[6] = vx;
        data[7] = vy;
        data[8] = vz;
        data[9] = newX;
        data[10] = newY;
        data[11] = newZ;
        data[12] = vx;
        data[13] = vy;
        data[14] = vz;

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

        init_pool();
        g_pool->execute(data, (int)particleCount, (int)stride);

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

            x += vx;
            y += vy;
            z += vz;

            vx *= multiplier;
            vy *= multiplier;
            vz *= multiplier;

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

        for (int i = 0; i < count; ++i) {
            pos_arr[i * 3] += vel_arr[i * 3];
            pos_arr[i * 3 + 1] += vel_arr[i * 3 + 1];
            pos_arr[i * 3 + 2] += vel_arr[i * 3 + 2];
            age_arr[i]++;
        }

        for (int i = 0; i < count * 3; ++i) {
            vel_arr[i] *= velocityMultiplier;
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