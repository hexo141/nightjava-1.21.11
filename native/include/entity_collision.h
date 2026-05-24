#pragma once

#include <jni.h>

#ifdef _WIN32
#define JNI_EXPORT __declspec(dllexport)
#else
#define JNI_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jobject JNICALL Java_com_nj_collision_EntityCollisionNative_getOrCreateCollisionBuffer(
    JNIEnv* env, jclass clazz, jint minPairs);

JNIEXPORT jboolean JNICALL Java_com_nj_collision_EntityCollisionNative_processEntityCollisions(
    JNIEnv* env, jclass clazz, jint pairCount);

JNIEXPORT void JNICALL Java_com_nj_collision_EntityCollisionNative_cleanupCollisionNative(
    JNIEnv* env, jclass clazz);

#ifdef __cplusplus
}
#endif