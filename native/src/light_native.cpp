#include "light_native.h"
#include <cstring>

extern void nibble_bulk_copy(uint8_t* dst, const uint8_t* src, int byteCount);
extern void nibble_bulk_fill(uint8_t* buf, int value, int byteCount);
extern int nibble_bulk_diff(const uint8_t* a, const uint8_t* b, int byteCount,
                             int* outX, int* outY, int* outZ, int maxDiffs);
extern int propagate_block_light_internal(
    uint8_t* lightBytes, const uint8_t* opacityGrid, const uint8_t* sourceLuminance,
    const jlong* queuePositions, const jlong* queueFlags,
    int decreaseCount, int increaseCount);
extern int propagate_sky_light_internal(
    uint8_t* lightBytes, const uint8_t* opacityGrid,
    const jlong* queuePositions, const jlong* queueFlags,
    int decreaseCount, int increaseCount, int sectionsBelow);

JNIEXPORT void JNICALL Java_com_nj_light_LightNative_nibbleBulkCopy(
    JNIEnv* env, jclass cls,
    jobject dstBuf, jobject srcBuf, jint count)
{
    uint8_t* dst = (uint8_t*)env->GetDirectBufferAddress(dstBuf);
    uint8_t* src = (uint8_t*)env->GetDirectBufferAddress(srcBuf);
    if (dst && src) {
        int bytes = count < 0 ? 2048 : (count > 2048 ? 2048 : count);
        nibble_bulk_copy(dst, src, bytes);
    }
}

JNIEXPORT void JNICALL Java_com_nj_light_LightNative_nibbleBulkFill(
    JNIEnv* env, jclass cls,
    jobject buf, jint value, jint count)
{
    uint8_t* data = (uint8_t*)env->GetDirectBufferAddress(buf);
    if (data) {
        int bytes = count < 0 ? 2048 : (count > 2048 ? 2048 : count);
        nibble_bulk_fill(data, value, bytes);
    }
}

JNIEXPORT jint JNICALL Java_com_nj_light_LightNative_nibbleBulkDiff(
    JNIEnv* env, jclass cls,
    jobject bufA, jobject bufB, jint count,
    jintArray outDiffs)
{
    uint8_t* a = (uint8_t*)env->GetDirectBufferAddress(bufA);
    uint8_t* b = (uint8_t*)env->GetDirectBufferAddress(bufB);
    if (!a || !b) return 0;

    int byteCount = count < 0 ? 2048 : (count > 2048 ? 2048 : count);
    jint* diffs = env->GetIntArrayElements(outDiffs, nullptr);
    jint len = env->GetArrayLength(outDiffs);
    int maxDiffs = len / 3;

    int found = nibble_bulk_diff(a, b, byteCount, diffs, diffs + maxDiffs, diffs + maxDiffs * 2, maxDiffs);
    env->ReleaseIntArrayElements(outDiffs, diffs, 0);
    return found;
}

JNIEXPORT jint JNICALL Java_com_nj_light_LightNative_propagateBlockLight(
    JNIEnv* env, jclass cls,
    jobject lightBuf, jint sectionX, jint sectionY, jint sectionZ,
    jbyteArray opacityGrid, jbyteArray sourceLuminance,
    jlongArray queuePositions, jlongArray queueFlags,
    jint decreaseCount, jint increaseCount)
{
    uint8_t* lightBytes = (uint8_t*)env->GetDirectBufferAddress(lightBuf);
    jbyte* opacity = env->GetByteArrayElements(opacityGrid, nullptr);
    jbyte* sources = env->GetByteArrayElements(sourceLuminance, nullptr);
    jlong* positions = env->GetLongArrayElements(queuePositions, nullptr);
    jlong* flags = env->GetLongArrayElements(queueFlags, nullptr);

    int result = 0;
    if (lightBytes && opacity && sources && positions && flags) {
        result = propagate_block_light_internal(
            lightBytes, (const uint8_t*)opacity, (const uint8_t*)sources,
            positions, flags, decreaseCount, increaseCount);
    }

    env->ReleaseByteArrayElements(opacityGrid, opacity, JNI_ABORT);
    env->ReleaseByteArrayElements(sourceLuminance, sources, JNI_ABORT);
    env->ReleaseLongArrayElements(queuePositions, positions, JNI_ABORT);
    env->ReleaseLongArrayElements(queueFlags, flags, JNI_ABORT);
    return result;
}

JNIEXPORT jint JNICALL Java_com_nj_light_LightNative_propagateSkyLight(
    JNIEnv* env, jclass cls,
    jobject lightBuf, jint sectionX, jint sectionY, jint sectionZ,
    jbyteArray opacityGrid,
    jlongArray queuePositions, jlongArray queueFlags,
    jint decreaseCount, jint increaseCount,
    jint sectionsBelow)
{
    uint8_t* lightBytes = (uint8_t*)env->GetDirectBufferAddress(lightBuf);
    jbyte* opacity = env->GetByteArrayElements(opacityGrid, nullptr);
    jlong* positions = env->GetLongArrayElements(queuePositions, nullptr);
    jlong* flags = env->GetLongArrayElements(queueFlags, nullptr);

    int result = 0;
    if (lightBytes && opacity && positions && flags) {
        result = propagate_sky_light_internal(
            lightBytes, (const uint8_t*)opacity,
            positions, flags, decreaseCount, increaseCount, sectionsBelow);
    }

    env->ReleaseByteArrayElements(opacityGrid, opacity, JNI_ABORT);
    env->ReleaseLongArrayElements(queuePositions, positions, JNI_ABORT);
    env->ReleaseLongArrayElements(queueFlags, flags, JNI_ABORT);
    return result;
}