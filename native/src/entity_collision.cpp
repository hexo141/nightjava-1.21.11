#include "entity_collision.h"
#include <cmath>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <memory>

static constexpr int PAIR_STRIDE = 8;

static float* g_collisionBuffer = nullptr;
static int g_collisionCapacity = 0;
static jobject g_collisionGlobalRef = nullptr;

struct CollisionThreadPool {
    std::thread workers[16];
    std::atomic<bool> shutdown_flag{false};
    std::atomic<int> next_chunk{0};
    std::atomic<int> active_workers{0};

    float* data = nullptr;
    int count = 0;
    int stride = 0;
    int total_chunks = 0;
    int chunk_size = 0;

    std::mutex work_mutex;
    std::condition_variable work_cv;
    std::condition_variable done_cv;
    bool work_ready = false;
    bool idle = true;

    CollisionThreadPool() {
        for (int i = 0; i < 16; i++) {
            workers[i] = std::thread(&CollisionThreadPool::worker_loop, this, i);
        }
    }

    ~CollisionThreadPool() {
        shutdown_flag.store(true);
        {
            std::lock_guard<std::mutex> lock(work_mutex);
            work_ready = true;
        }
        work_cv.notify_all();
        for (int i = 0; i < 16; i++) {
            if (workers[i].joinable()) {
                workers[i].join();
            }
        }
    }

    void worker_loop(int id) {
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
                int end = std::min(start + chunk_size, count);
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

    void execute(float* buf, int pair_count, int strd) {
        if (pair_count <= 0) return;

        {
            std::unique_lock<std::mutex> lock(work_mutex);
            data = buf;
            count = pair_count;
            stride = strd;
            chunk_size = std::max(1, pair_count / 16);
            total_chunks = (pair_count + chunk_size - 1) / chunk_size;
            next_chunk.store(0);
            active_workers.store(16);
            idle = false;
            work_ready = true;
        }
        work_cv.notify_all();

        {
            std::unique_lock<std::mutex> lock(work_mutex);
            done_cv.wait(lock, [this]() { return idle; });
        }
    }

    static void process_chunk(float* buf, int start, int end, int strd) {
        for (int i = start; i < end; i++) {
            float* pair = buf + i * strd;

            float dx = pair[2] - pair[0];
            float dz = pair[3] - pair[1];

            float dist = std::max(std::abs(dx), std::abs(dz));
            if (dist < 0.01f) {
                pair[0] = 0.0f;
                pair[1] = 0.0f;
                pair[2] = 0.0f;
                pair[3] = 0.0f;
                continue;
            }

            dist = std::sqrt(dist);
            dx /= dist;
            dz /= dist;

            float invDist = 1.0f / dist;
            if (invDist > 1.0f) {
                invDist = 1.0f;
            }

            dx *= invDist * 0.05f;
            dz *= invDist * 0.05f;

            int bits = static_cast<int>(pair[4]);
            bool pushable0 = (bits & 1) != 0;
            bool pushable1 = (bits & 2) != 0;

            pair[0] = pushable0 ? -dx : 0.0f;
            pair[1] = pushable0 ? -dz : 0.0f;
            pair[2] = pushable1 ? dx : 0.0f;
            pair[3] = pushable1 ? dz : 0.0f;
        }
    }
};

static std::unique_ptr<CollisionThreadPool> g_collisionPool;

static void init_pool() {
    if (!g_collisionPool) {
        g_collisionPool = std::make_unique<CollisionThreadPool>();
    }
}

extern "C" {

JNIEXPORT jobject JNICALL Java_com_nj_collision_EntityCollisionNative_getOrCreateCollisionBuffer(
    JNIEnv* env, jclass, jint minPairs)
{
    int requiredFloats = minPairs * PAIR_STRIDE;

    if (requiredFloats > g_collisionCapacity) {
        if (g_collisionGlobalRef) {
            env->DeleteGlobalRef(g_collisionGlobalRef);
            g_collisionGlobalRef = nullptr;
        }
        delete[] g_collisionBuffer;
        g_collisionBuffer = nullptr;

        g_collisionBuffer = new float[requiredFloats];
        std::memset(g_collisionBuffer, 0, requiredFloats * sizeof(float));
        g_collisionCapacity = requiredFloats;
    }

    if (!g_collisionGlobalRef) {
        jobject localRef = env->NewDirectByteBuffer(
            g_collisionBuffer, g_collisionCapacity * sizeof(float));
        g_collisionGlobalRef = env->NewGlobalRef(localRef);
        env->DeleteLocalRef(localRef);
    }

    return g_collisionGlobalRef;
}

JNIEXPORT jboolean JNICALL Java_com_nj_collision_EntityCollisionNative_processEntityCollisions(
    JNIEnv*, jclass, jint pairCount)
{
    if (pairCount <= 0 || !g_collisionBuffer) {
        return JNI_FALSE;
    }

    init_pool();
    g_collisionPool->execute(g_collisionBuffer, (int)pairCount, PAIR_STRIDE);

    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_com_nj_collision_EntityCollisionNative_cleanupCollisionNative(
    JNIEnv* env, jclass)
{
    g_collisionPool.reset();

    if (g_collisionGlobalRef) {
        env->DeleteGlobalRef(g_collisionGlobalRef);
        g_collisionGlobalRef = nullptr;
    }
    delete[] g_collisionBuffer;
    g_collisionBuffer = nullptr;
    g_collisionCapacity = 0;
}

}