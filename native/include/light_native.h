#pragma once

#include <jni.h>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_com_nj_light_LightNative_nibbleBulkCopy(
    JNIEnv* env, jclass cls,
    jobject dstBuf, jobject srcBuf, jint count);

JNIEXPORT void JNICALL Java_com_nj_light_LightNative_nibbleBulkFill(
    JNIEnv* env, jclass cls,
    jobject buf, jint value, jint count);

JNIEXPORT jint JNICALL Java_com_nj_light_LightNative_nibbleBulkDiff(
    JNIEnv* env, jclass cls,
    jobject bufA, jobject bufB, jint count,
    jintArray outDiffs);

JNIEXPORT jint JNICALL Java_com_nj_light_LightNative_propagateBlockLight(
    JNIEnv* env, jclass cls,
    jobject lightBuf, jint sectionX, jint sectionY, jint sectionZ,
    jbyteArray opacityGrid, jbyteArray sourceLuminance,
    jlongArray queuePositions, jlongArray queueFlags,
    jint decreaseCount, jint increaseCount);

JNIEXPORT jint JNICALL Java_com_nj_light_LightNative_propagateSkyLight(
    JNIEnv* env, jclass cls,
    jobject lightBuf, jint sectionX, jint sectionY, jint sectionZ,
    jbyteArray opacityGrid,
    jlongArray queuePositions, jlongArray queueFlags,
    jint decreaseCount, jint increaseCount,
    jint sectionsBelow);

#ifdef __cplusplus
}
#endif