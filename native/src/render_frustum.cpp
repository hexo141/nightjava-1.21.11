#include "render_frustum.h"
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <queue>
#include <emmintrin.h>
#include <pmmintrin.h>

static float* gFrustumPlanes = nullptr;
static float* gFrustumBuffer = nullptr;
static jint gFrustumBufferCapacity = 0;
static double gCameraX = 0.0;
static double gCameraY = 0.0;
static double gCameraZ = 0.0;
static jint gLastCount = 0;

static constexpr int PARALLEL_THRESHOLD = 256;

static void extractFrustumPlanes(const float* m, float* planes) {
    planes[0] = m[3] + m[0];  planes[1] = m[7] + m[4];  planes[2] = m[11] + m[8];  planes[3] = m[15] + m[12];
    planes[4] = m[3] - m[0];  planes[5] = m[7] - m[4];  planes[6] = m[11] - m[8];  planes[7] = m[15] - m[12];
    planes[8] = m[3] + m[1];  planes[9] = m[7] + m[5];  planes[10] = m[11] + m[9]; planes[11] = m[15] + m[13];
    planes[12] = m[3] - m[1]; planes[13] = m[7] - m[5]; planes[14] = m[11] - m[9]; planes[15] = m[15] - m[13];
    planes[16] = m[3] + m[2]; planes[17] = m[7] + m[6]; planes[18] = m[11] + m[10]; planes[19] = m[15] + m[14];
    planes[20] = m[3] - m[2]; planes[21] = m[7] - m[6]; planes[22] = m[11] - m[10]; planes[23] = m[15] - m[14];

    for (int i = 0; i < 6; i++) {
        float len = std::sqrtf(planes[i * 4] * planes[i * 4] + planes[i * 4 + 1] * planes[i * 4 + 1] + planes[i * 4 + 2] * planes[i * 4 + 2]);
        if (len > 1e-10f) {
            float inv = 1.0f / len;
            planes[i * 4]     *= inv;
            planes[i * 4 + 1] *= inv;
            planes[i * 4 + 2] *= inv;
            planes[i * 4 + 3] *= inv;
        }
    }
}

static bool testAabbSimd(const float* aabb, const float* planes) {
    float minX = aabb[0] - (float)gCameraX;
    float minY = aabb[1] - (float)gCameraY;
    float minZ = aabb[2] - (float)gCameraZ;
    float maxX = aabb[3] - (float)gCameraX;
    float maxY = aabb[4] - (float)gCameraY;
    float maxZ = aabb[5] - (float)gCameraZ;

    __m128 minV = _mm_set_ps(0.0f, minZ, minY, minX);
    __m128 maxV = _mm_set_ps(0.0f, maxZ, maxY, maxX);

    for (int p = 0; p < 6; p++) {
        __m128 planeV = _mm_loadu_ps(planes + p * 4);
        __m128 selMin = _mm_and_ps(_mm_cmplt_ps(planeV, _mm_setzero_ps()), minV);
        __m128 selMax = _mm_andnot_ps(_mm_cmplt_ps(planeV, _mm_setzero_ps()), maxV);
        __m128 pnt = _mm_or_ps(selMin, selMax);
        __m128 prod = _mm_mul_ps(planeV, pnt);
        float dist = planeV[3] + prod[0] + prod[1] + prod[2];
        if (dist < 0.0f) {
            return false;
        }
    }
    return true;
}

static void cullRange(int start, int end) {
    for (int i = start; i < end; i++) {
        float* aabb = gFrustumBuffer + i * 7;
        aabb[6] = testAabbSimd(aabb, gFrustumPlanes) ? 1.0f : 0.0f;
    }
}

static void cullSingleThreaded(int count) {
    cullRange(0, count);
}

class RenderThreadPool {
public:
    static RenderThreadPool& get() {
        static RenderThreadPool instance;
        return instance;
    }

    void execute(int totalCount) {
        int numThreads = (int)mThreads.size();
        int chunkSize = (totalCount + numThreads - 1) / numThreads;
        if (chunkSize < 1) chunkSize = 1;
        int numChunks = (totalCount + chunkSize - 1) / chunkSize;

        mTasksDone.store(0);

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStart = 0;
            mEnd = totalCount;
            mChunkSize = chunkSize;
            mReady = true;
        }
        mWorkCV.notify_all();

        {
            std::unique_lock<std::mutex> lock(mMutex);
            mDoneCV.wait(lock, [&]() { return mTasksDone.load() >= numChunks; });
        }
    }

    ~RenderThreadPool() {
        mStop = true;
        mWorkCV.notify_all();
        for (auto& t : mThreads) {
            if (t.joinable()) t.join();
        }
    }

private:
    RenderThreadPool() {
        int n = (int)std::thread::hardware_concurrency();
        if (n < 2) n = 2;
        if (n > 8) n = 8;
        for (int i = 0; i < n; i++) {
            mThreads.emplace_back([this]() {
                while (true) {
                    int start, end;
                    {
                        std::unique_lock<std::mutex> lock(mMutex);
                        mWorkCV.wait(lock, [this]() { return mStop || mReady; });
                        if (mStop) return;

                        start = mStart;
                        mStart += mChunkSize;
                        if (start >= mEnd) {
                            continue;
                        }
                        end = start + mChunkSize;
                        if (end > mEnd) end = mEnd;
                    }

                    cullRange(start, end);
                    mTasksDone.fetch_add(1);
                    mDoneCV.notify_one();
                }
            });
        }
    }

    std::vector<std::thread> mThreads;
    std::mutex mMutex;
    std::condition_variable mWorkCV;
    std::condition_variable mDoneCV;
    std::atomic<bool> mStop{false};
    std::atomic<int> mTasksDone{0};
    int mStart = 0;
    int mEnd = 0;
    int mChunkSize = 0;
    bool mReady = false;
};

extern "C" {

JNIEXPORT jobject JNICALL Java_com_nj_render_RenderNative_getOrCreateFrustumBuffer(JNIEnv* env, jclass cls, jint maxEntities) {
    jint needed = maxEntities * 7;
    if (gFrustumBuffer == nullptr || needed > gFrustumBufferCapacity) {
        delete[] gFrustumBuffer;
        gFrustumBuffer = new float[needed]();
        gFrustumBufferCapacity = needed;
    }
    return env->NewDirectByteBuffer(gFrustumBuffer, needed * sizeof(float));
}

JNIEXPORT void JNICALL Java_com_nj_render_RenderNative_updateFrustumPlanes(JNIEnv* env, jclass cls, jfloatArray matrix, jdouble cx, jdouble cy, jdouble cz) {
    if (gFrustumPlanes == nullptr) {
        gFrustumPlanes = new float[24];
    }

    gCameraX = cx;
    gCameraY = cy;
    gCameraZ = cz;

    jfloat* m = env->GetFloatArrayElements(matrix, nullptr);
    extractFrustumPlanes(m, gFrustumPlanes);
    env->ReleaseFloatArrayElements(matrix, m, JNI_ABORT);
}

JNIEXPORT jint JNICALL Java_com_nj_render_RenderNative_batchFrustumCullEntities(JNIEnv* env, jclass cls, jint count) {
    if (gFrustumPlanes == nullptr || gFrustumBuffer == nullptr || count <= 0) return 0;

    gLastCount = count;

    if (count < PARALLEL_THRESHOLD) {
        cullSingleThreaded(count);
    } else {
        RenderThreadPool::get().execute(count);
    }

    return count;
}

JNIEXPORT jboolean JNICALL Java_com_nj_render_RenderNative_getVisibilityResult(JNIEnv* env, jclass cls, jint index) {
    if (gFrustumBuffer == nullptr || index < 0 || index >= gLastCount) return JNI_TRUE;
    return gFrustumBuffer[index * 7 + 6] >= 0.5f ? JNI_TRUE : JNI_FALSE;
}

}